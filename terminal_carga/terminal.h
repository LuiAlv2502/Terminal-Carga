#ifndef TERMINAL_H
#define TERMINAL_H
 
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
 
#include "cola.h"
 
#define NUM_MUELLES  3
#define QUANTUM_RR   2
#define LOG_FILE     "operaciones.log"
 
typedef enum {
    ESTADO_CREADO    = 0,
    ESTADO_NUEVO     = 1,
    ESTADO_LISTO     = 2,
    ESTADO_EJECUCION = 3,
    ESTADO_BLOQUEADO = 4,
    ESTADO_TERMINADO = 5
} EstadoHilo;
 
extern const char *nombre_estado[];
 
typedef enum {
    CARGA_PERECEDERO = 0,
    CARGA_INDUSTRIAL = 1,
    CARGA_GENERAL    = 2
} TipoCarga;
 
extern const char *nombre_carga[];
 
typedef enum {
    PROCESO_FIFO = 0,
    PROCESO_RR   = 1
} Algoritmo;
 
struct Camion {
    int        id;
    TipoCarga  tipo_carga;
    int        burst_total;
    int        burst_restante;
    int        prioridad;
    EstadoHilo estado;
 
    double    t_llegada;
    double    t_inicio;
    double    t_fin;
    double    t_espera;
    double    t_retorno;
 
    Algoritmo algoritmo;
};
 
typedef struct Camion Camion;
 
extern sem_t           sem_muelles;
extern pthread_mutex_t mutex_log;
extern pthread_mutex_t mutex_cola;
extern pthread_mutex_t mutex_estado;
extern Camion          camiones[MAX_CAMIONES];
extern FILE           *archivo_log;
extern int             total_camiones;
 
/* Declarada aquí (no en cola.h) porque necesita Camion completo */
int cola_desencolar_prioridad(Cola *c, Camion camiones[]);
 
double tiempo_actual(void);
void   log_evento(int id_camion, const char *evento);
void   cambiar_estado(int id, EstadoHilo nuevo);
void   ejecutar_simulacion(Algoritmo algo, int n_camiones);
 
#endif