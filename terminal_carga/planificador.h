#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H
 
#include <pthread.h>
#include "terminal.h"
 
/* ─────────────────────────────────────────────
   Variables globales del planificador
   (definidas en planificador.c)
   ───────────────────────────────────────────── */
extern pthread_mutex_t mutex_planif;
extern pthread_cond_t  cond_planif;
extern pthread_cond_t  cond_camion[MAX_CAMIONES];
extern int             seleccionado[MAX_CAMIONES];
extern int             camiones_activos;
extern Algoritmo       algoritmo_actual;
 
/* ─────────────────────────────────────────────
   Funciones de planificador.c
   ───────────────────────────────────────────── */
void *hilo_planificador(void *arg);
void *hilo_camion(void *arg);
 
#endif /* PLANIFICADOR_H */