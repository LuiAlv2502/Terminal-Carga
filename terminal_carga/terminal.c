#include "terminal.h"
#include "planificador.h"
#include "estadisticas.h"
 
/* ─────────────────────────────────────────────
   Variables globales compartidas
   ───────────────────────────────────────────── */
sem_t           sem_muelles;
pthread_mutex_t mutex_log    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
Cola            cola_listos;
Camion          camiones[MAX_CAMIONES];
FILE           *archivo_log  = NULL;
int             total_camiones = 0;
 
const char *nombre_estado[] = {
    "CREADO", "NUEVO", "LISTO", "EJECUCION", "BLOQUEADO", "TERMINADO"
};
const char *nombre_carga[] = {
    "Perecedero", "Industrial", "General"
};
 
/* ─────────────────────────────────────────────
   Tiempo en segundos de alta resolución
   ───────────────────────────────────────────── */
double tiempo_actual(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
 
/* ─────────────────────────────────────────────
   Log de operaciones (protegido por mutex)
   ───────────────────────────────────────────── */
void log_evento(int id_camion, const char *evento)
{
    pthread_mutex_lock(&mutex_estado);
    if (id_camion >= 0 && camiones[id_camion].estado == ESTADO_EJECUCION)
        camiones[id_camion].estado = ESTADO_BLOQUEADO;
    pthread_mutex_unlock(&mutex_estado);
 
    pthread_mutex_lock(&mutex_log);
 
    pthread_mutex_lock(&mutex_estado);
    if (id_camion >= 0 && camiones[id_camion].estado == ESTADO_BLOQUEADO)
        camiones[id_camion].estado = ESTADO_EJECUCION;
    pthread_mutex_unlock(&mutex_estado);
 
    double t = tiempo_actual();
    if (archivo_log) {
        fprintf(archivo_log, "[%.3f] Camion #%02d | %-12s | %s\n",
                t, id_camion,
                (id_camion >= 0) ? nombre_carga[camiones[id_camion].tipo_carga] : "SISTEMA",
                evento);
        fflush(archivo_log);
    }
    printf("[%.3f] Camion #%02d | %-12s | %s\n",
           t, id_camion,
           (id_camion >= 0) ? nombre_carga[camiones[id_camion].tipo_carga] : "SISTEMA",
           evento);
 
    pthread_mutex_unlock(&mutex_log);
}
 
/* ─────────────────────────────────────────────
   Cambio de estado con reporte
   ───────────────────────────────────────────── */
void cambiar_estado(int id, EstadoHilo nuevo)
{
    pthread_mutex_lock(&mutex_estado);
    EstadoHilo anterior = camiones[id].estado;
    camiones[id].estado = nuevo;
    pthread_mutex_unlock(&mutex_estado);
 
    char msg[128];
    snprintf(msg, sizeof(msg), "Estado: %s → %s",
             nombre_estado[anterior], nombre_estado[nuevo]);
    log_evento(id, msg);
}
 
/* ─────────────────────────────────────────────
   Ejecutar una simulación completa
   ───────────────────────────────────────────── */
void ejecutar_simulacion(Algoritmo algo, int n_camiones)
{
    algoritmo_actual = algo;
    total_camiones   = n_camiones;
    camiones_activos = n_camiones;
 
    sem_init(&sem_muelles, 0, NUM_MUELLES);
    cola_init(&cola_listos);
    for (int i = 0; i < n_camiones; i++) {
        pthread_cond_init(&cond_camion[i], NULL);
        seleccionado[i] = 0;
    }
 
    TipoCarga tipos[] = { CARGA_PERECEDERO, CARGA_INDUSTRIAL, CARGA_GENERAL,
                          CARGA_PERECEDERO, CARGA_GENERAL,    CARGA_INDUSTRIAL };
    int bursts[]      = { 3, 5, 2, 4, 1, 6 };
 
    pthread_t hilos[MAX_CAMIONES];
    pthread_t th_planificador;
 
    for (int i = 0; i < n_camiones; i++) {
        camiones[i].id             = i;
        camiones[i].tipo_carga     = tipos[i % 6];
        camiones[i].burst_total    = bursts[i % 6];
        camiones[i].burst_restante = bursts[i % 6];
        camiones[i].prioridad      = (int)camiones[i].tipo_carga;
        camiones[i].estado         = ESTADO_CREADO;
        camiones[i].t_llegada      = 0;
        camiones[i].t_inicio       = 0;
        camiones[i].t_fin          = 0;
        camiones[i].t_espera       = 0;
        camiones[i].t_retorno      = 0;
        camiones[i].algoritmo      = algo;
    }
 
    printf("\n▶ Iniciando simulación con algoritmo: %s (%d camiones, %d muelles)\n\n",
           algo == PROCESO_FIFO ? "FIFO (Prioridad)" : "Round Robin",
           n_camiones, NUM_MUELLES);
 
    if (archivo_log)
        fprintf(archivo_log, "\n=== SIMULACIÓN %s ===\n",
                algo == PROCESO_FIFO ? "FIFO" : "RR");
 
    pthread_create(&th_planificador, NULL, hilo_planificador, NULL);
 
    for (int i = 0; i < n_camiones; i++) {
        usleep(100000);
        pthread_create(&hilos[i], NULL, hilo_camion, &camiones[i]);
    }
 
    for (int i = 0; i < n_camiones; i++)
        pthread_join(hilos[i], NULL);
    pthread_join(th_planificador, NULL);
 
    sem_destroy(&sem_muelles);
    for (int i = 0; i < n_camiones; i++)
        pthread_cond_destroy(&cond_camion[i]);
 
    imprimir_tabla_comparativa(camiones, n_camiones, algo);
}