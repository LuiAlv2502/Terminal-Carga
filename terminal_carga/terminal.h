#ifndef TERMINAL_H
#define TERMINAL_H

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* ─────────────────────────────────────────────
   Configuración general
   ───────────────────────────────────────────── */
#define MAX_CAMIONES      20
#define NUM_MUELLES        3
#define QUANTUM_RR         2   /* segundos simulados por quantum */
#define LOG_FILE          "operaciones.log"

/* ─────────────────────────────────────────────
   Estados del hilo (ciclo de vida)
   ───────────────────────────────────────────── */
typedef enum {
    ESTADO_CREADO     = 0,   /* antes de que el hilo arranque */
    ESTADO_NUEVO      = 1,
    ESTADO_LISTO      = 2,
    ESTADO_EJECUCION  = 3,
    ESTADO_BLOQUEADO  = 4,
    ESTADO_TERMINADO  = 5
} EstadoHilo;

extern const char *nombre_estado[];

/* ─────────────────────────────────────────────
   Tipos de carga (afectan prioridad)
   ───────────────────────────────────────────── */
typedef enum {
    CARGA_PERECEDERO  = 0,   /* mayor prioridad */
    CARGA_INDUSTRIAL  = 1,
    CARGA_GENERAL     = 2
} TipoCarga;

extern const char *nombre_carga[];

/* ─────────────────────────────────────────────
   Algoritmos de planificación
   ───────────────────────────────────────────── */
typedef enum {
    PROCESO_FIFO = 0,
    PROCESO_RR   = 1
} Algoritmo;

/* ─────────────────────────────────────────────
   Parámetros de cada camión / hilo
   ───────────────────────────────────────────── */
typedef struct {
    int        id;
    TipoCarga  tipo_carga;
    int        burst_total;      /* unidades de trabajo total */
    int        burst_restante;   /* para Round Robin          */
    int        prioridad;        /* 0 = máxima                */
    EstadoHilo estado;

    /* métricas de tiempo */
    double  t_llegada;
    double  t_inicio;
    double  t_fin;
    double  t_espera;
    double  t_retorno;

    Algoritmo algoritmo;         /* cuál algoritmo usa esta corrida */
} Camion;

/* ─────────────────────────────────────────────
   Cola de planificación (lista circular)
   ───────────────────────────────────────────── */
typedef struct {
    int  ids[MAX_CAMIONES];
    int  frente;
    int  fin;
    int  tam;
} Cola;

void cola_init(Cola *c);
void cola_encolar(Cola *c, int id);
int  cola_desencolar(Cola *c);          /* FIFO / RR sin prioridad     */
int  cola_desencolar_prioridad(Cola *c, Camion camiones[]); /* con prio */
int  cola_vacia(Cola *c);

/* ─────────────────────────────────────────────
   Recursos globales (definidos en terminal.c)
   ───────────────────────────────────────────── */
extern sem_t           sem_muelles;
extern pthread_mutex_t mutex_log;
extern pthread_mutex_t mutex_cola;
extern pthread_mutex_t mutex_estado;
extern pthread_mutex_t mutex_planif;
extern pthread_cond_t  cond_planif;
extern pthread_cond_t  cond_camion[MAX_CAMIONES];
extern int             seleccionado[MAX_CAMIONES];
extern int             camiones_activos;
extern Cola            cola_listos;
extern Camion          camiones[MAX_CAMIONES];
extern FILE           *archivo_log;
extern int             total_camiones;
extern Algoritmo       algoritmo_actual;

/* ─────────────────────────────────────────────
   Funciones auxiliares
   ───────────────────────────────────────────── */
double tiempo_actual(void);
void   log_evento(int id_camion, const char *evento);
void   cambiar_estado(int id, EstadoHilo nuevo);
void  *hilo_planificador(void *arg);
void  *hilo_camion(void *arg);
void   imprimir_tabla_comparativa(Camion arr[], int n, Algoritmo algo);
void   ejecutar_simulacion(Algoritmo algo, int n_camiones);

#endif /* TERMINAL_H */
