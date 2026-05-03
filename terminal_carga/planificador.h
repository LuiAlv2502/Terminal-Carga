#ifndef PLANIFICADOR_H
#define PLANIFICADOR_H
 
#include <pthread.h>
#include "terminal.h"
 
extern pthread_mutex_t mutex_planif;
extern pthread_cond_t  cond_planif;
extern pthread_cond_t  cond_camion[MAX_CAMIONES];
extern int             seleccionado[MAX_CAMIONES];
extern int             camiones_activos;
extern Algoritmo       algoritmo_actual;
 

void *hilo_planificador(void *arg);
void *hilo_camion(void *arg);
 
#endif 