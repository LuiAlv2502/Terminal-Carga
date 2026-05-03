#include "planificador.h"
#include "cola.h"

pthread_mutex_t mutex_planif = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_planif  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  cond_camion[MAX_CAMIONES];
int             seleccionado[MAX_CAMIONES];
int             camiones_activos = 0;
Algoritmo       algoritmo_actual = PROCESO_FIFO;
 

void *hilo_planificador(void *arg)
{
    (void)arg;
 
    while (1) {
        pthread_mutex_lock(&mutex_planif);
        while (cola_vacia(&cola_listos) && camiones_activos > 0)
            pthread_cond_wait(&cond_planif, &mutex_planif);
 
        if (cola_vacia(&cola_listos) && camiones_activos == 0) {
            pthread_mutex_unlock(&mutex_planif);
            break;
        }
        pthread_mutex_unlock(&mutex_planif);
 
        sem_wait(&sem_muelles);
 
        pthread_mutex_lock(&mutex_planif);
 
        if (cola_vacia(&cola_listos)) {
            sem_post(&sem_muelles);
            pthread_mutex_unlock(&mutex_planif);
            continue;
        }
 
        int elegido = (algoritmo_actual == PROCESO_FIFO)
                      ? cola_desencolar_prioridad(&cola_listos, camiones)
                      : cola_desencolar(&cola_listos);
 
        seleccionado[elegido] = 1;
        pthread_cond_signal(&cond_camion[elegido]);
        pthread_mutex_unlock(&mutex_planif);
    }
    return NULL;
}

void *hilo_camion(void *arg)
{
    Camion *c = (Camion *)arg;
 
    cambiar_estado(c->id, ESTADO_NUEVO);
    c->t_llegada = tiempo_actual();
 
    cambiar_estado(c->id, ESTADO_LISTO);
 
    pthread_mutex_lock(&mutex_planif);
    cola_encolar(&cola_listos, c->id);
    pthread_cond_signal(&cond_planif);
 
    while (1) {
        while (!seleccionado[c->id])
            pthread_cond_wait(&cond_camion[c->id], &mutex_planif);
        seleccionado[c->id] = 0;
        pthread_mutex_unlock(&mutex_planif);
 
        if (camiones[c->id].t_inicio == 0.0)
            camiones[c->id].t_inicio = tiempo_actual();
 
        cambiar_estado(c->id, ESTADO_EJECUCION);
 
        if (algoritmo_actual == PROCESO_RR) {
            int trabajo = (c->burst_restante < QUANTUM_RR)
                          ? c->burst_restante : QUANTUM_RR;
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "RR: cargando %d/%d unidades (quantum=%d)",
                     trabajo, c->burst_total, QUANTUM_RR);
            log_evento(c->id, msg);
            sleep(trabajo);
            c->burst_restante -= trabajo;
 
            sem_post(&sem_muelles);
 
            if (c->burst_restante > 0) {
                cambiar_estado(c->id, ESTADO_LISTO);
                pthread_mutex_lock(&mutex_planif);
                cola_encolar(&cola_listos, c->id);
                pthread_cond_signal(&cond_planif);
                continue;
            }
 
            pthread_mutex_lock(&mutex_planif);
            pthread_cond_signal(&cond_planif);
            pthread_mutex_unlock(&mutex_planif);
        } else {
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "FIFO: cargando %d unidades completas", c->burst_total);
            log_evento(c->id, msg);
            sleep(c->burst_total);
 
            sem_post(&sem_muelles);
            pthread_mutex_lock(&mutex_planif);
            pthread_cond_signal(&cond_planif);
            pthread_mutex_unlock(&mutex_planif);
        }
 
        break;
    }
 
    camiones[c->id].t_fin     = tiempo_actual();
    camiones[c->id].t_retorno = camiones[c->id].t_fin - camiones[c->id].t_llegada;
    camiones[c->id].t_espera  = camiones[c->id].t_retorno - camiones[c->id].burst_total;
    if (camiones[c->id].t_espera < 0) camiones[c->id].t_espera = 0;
 
    pthread_mutex_lock(&mutex_planif);
    camiones_activos--;
    pthread_cond_signal(&cond_planif);
    pthread_mutex_unlock(&mutex_planif);
 
    cambiar_estado(c->id, ESTADO_TERMINADO);
    return NULL;
}