

#include "terminal.h"

/* ─────────────────────────────────────────────
   Variables globales compartidas
   ───────────────────────────────────────────── */
sem_t           sem_muelles;
pthread_mutex_t mutex_log    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola   = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_planif = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_planif  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  cond_camion[MAX_CAMIONES];  /* init en ejecutar_simulacion */
int             seleccionado[MAX_CAMIONES];
int             camiones_activos = 0;
Cola            cola_listos;
Camion          camiones[MAX_CAMIONES];
FILE           *archivo_log  = NULL;
int             total_camiones = 0;
Algoritmo       algoritmo_actual = PROCESO_FIFO;

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
   Cola de planificación
   ───────────────────────────────────────────── */
void cola_init(Cola *c)
{
    c->frente = 0;
    c->fin    = 0;
    c->tam    = 0;
}

void cola_encolar(Cola *c, int id)
{
    if (c->tam >= MAX_CAMIONES) return;
    c->ids[c->fin] = id;
    c->fin = (c->fin + 1) % MAX_CAMIONES;
    c->tam++;
}

/* FIFO simple: extrae el primero */
int cola_desencolar(Cola *c)
{
    if (c->tam == 0) return -1;
    int id = c->ids[c->frente];
    c->frente = (c->frente + 1) % MAX_CAMIONES;
    c->tam--;
    return id;
}

/* Con prioridad: extrae el de menor valor de prioridad (0 = más alto) */
int cola_desencolar_prioridad(Cola *c, Camion arr[])
{
    if (c->tam == 0) return -1;
    int mejor_pos  = c->frente;
    int mejor_prio = arr[c->ids[c->frente]].prioridad;

    for (int i = 1; i < c->tam; i++) {
        int pos = (c->frente + i) % MAX_CAMIONES;
        if (arr[c->ids[pos]].prioridad < mejor_prio) {
            mejor_prio = arr[c->ids[pos]].prioridad;
            mejor_pos  = pos;
        }
    }

    int id = c->ids[mejor_pos];
    /* corremos los elementos para llenar el hueco */
    for (int i = 0; i < c->tam - 1; i++) {
        int pos_actual  = (mejor_pos + i)     % MAX_CAMIONES;
        int pos_siguiente = (mejor_pos + i + 1) % MAX_CAMIONES;
        c->ids[pos_actual] = c->ids[pos_siguiente];
    }
    c->fin = (c->fin - 1 + MAX_CAMIONES) % MAX_CAMIONES;
    c->tam--;
    return id;
}

int cola_vacia(Cola *c)
{
    return c->tam == 0;
}

/* ─────────────────────────────────────────────
   Log de operaciones (protegido por mutex)
   ───────────────────────────────────────────── */
void log_evento(int id_camion, const char *evento)
{
    /*
     * El hilo intenta tomar el mutex; si está ocupado,
     * su estado pasa a BLOQUEADO mientras espera.
     */
    pthread_mutex_lock(&mutex_estado);
    if (id_camion >= 0 && camiones[id_camion].estado == ESTADO_EJECUCION) {
        camiones[id_camion].estado = ESTADO_BLOQUEADO;
    }
    pthread_mutex_unlock(&mutex_estado);

    pthread_mutex_lock(&mutex_log);   /* ← sección crítica del log */

    /* Restaurar a EJECUCION una vez dentro */
    pthread_mutex_lock(&mutex_estado);
    if (id_camion >= 0 && camiones[id_camion].estado == ESTADO_BLOQUEADO) {
        camiones[id_camion].estado = ESTADO_EJECUCION;
    }
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

    pthread_mutex_unlock(&mutex_log); /* ← fin sección crítica log */
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
   Hilo planificador dedicado
   Decide qué camión toma el muelle y lo despierta
   mediante variables de condición, eliminando el
   busy-wait y la livelock del esquema anterior.
   ───────────────────────────────────────────── */
void *hilo_planificador(void *arg)
{
    (void)arg;

    while (1) {
        /* Esperar hasta que haya camiones en cola */
        pthread_mutex_lock(&mutex_planif);
        while (cola_vacia(&cola_listos) && camiones_activos > 0)
            pthread_cond_wait(&cond_planif, &mutex_planif);

        /* Condición de salida: no quedan camiones ni en cola ni activos */
        if (cola_vacia(&cola_listos) && camiones_activos == 0) {
            pthread_mutex_unlock(&mutex_planif);
            break;
        }
        pthread_mutex_unlock(&mutex_planif);

        /* Esperar muelle libre usando el semáforo (requerimiento del enunciado) */
        sem_wait(&sem_muelles);

        pthread_mutex_lock(&mutex_planif);

        /* Si la cola quedó vacía mientras esperábamos el semáforo, devolverlo */
        if (cola_vacia(&cola_listos)) {
            sem_post(&sem_muelles);
            pthread_mutex_unlock(&mutex_planif);
            continue;
        }

        /* Seleccionar camión según algoritmo */
        int elegido = (algoritmo_actual == PROCESO_FIFO)
                      ? cola_desencolar_prioridad(&cola_listos, camiones)
                      : cola_desencolar(&cola_listos);

        seleccionado[elegido] = 1;
        pthread_cond_signal(&cond_camion[elegido]); /* despertar al elegido */
        pthread_mutex_unlock(&mutex_planif);
    }
    return NULL;
}

/* ─────────────────────────────────────────────
   Función del hilo de cada camión
   ───────────────────────────────────────────── */
void *hilo_camion(void *arg)
{
    Camion *c = (Camion *)arg;

    /* ── ESTADO: NUEVO ── */
    cambiar_estado(c->id, ESTADO_NUEVO);
    c->t_llegada = tiempo_actual();

    /* ── ESTADO: LISTO (entra a la cola del planificador) ── */
    cambiar_estado(c->id, ESTADO_LISTO);

    pthread_mutex_lock(&mutex_planif);
    cola_encolar(&cola_listos, c->id);
    pthread_cond_signal(&cond_planif); /* notificar al planificador */

    /* Bucle principal: esperar selección → trabajar → reencolar si RR */
    while (1) {
        /* Esperar a ser elegido (libera mutex mientras duerme) */
        while (!seleccionado[c->id])
            pthread_cond_wait(&cond_camion[c->id], &mutex_planif);
        seleccionado[c->id] = 0;
        pthread_mutex_unlock(&mutex_planif);

        /* ── ESTADO: EJECUCION ── */
        if (camiones[c->id].t_inicio == 0.0)
            camiones[c->id].t_inicio = tiempo_actual();

        cambiar_estado(c->id, ESTADO_EJECUCION);

        if (algoritmo_actual == PROCESO_RR) {
            /* Round Robin: trabaja un quantum */
            int trabajo = (c->burst_restante < QUANTUM_RR)
                          ? c->burst_restante : QUANTUM_RR;
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "RR: cargando %d/%d unidades (quantum=%d)",
                     trabajo, c->burst_total, QUANTUM_RR);
            log_evento(c->id, msg);
            sleep(trabajo);
            c->burst_restante -= trabajo;

            sem_post(&sem_muelles);           /* libera muelle físico */

            if (c->burst_restante > 0) {
                /* Reencolar → LISTO de nuevo */
                cambiar_estado(c->id, ESTADO_LISTO);
                pthread_mutex_lock(&mutex_planif);
                cola_encolar(&cola_listos, c->id);
                pthread_cond_signal(&cond_planif);
                continue;  /* mutex_planif sigue tomado → correcto para cond_wait */
            }

            pthread_mutex_lock(&mutex_planif);
            pthread_cond_signal(&cond_planif);
            pthread_mutex_unlock(&mutex_planif);
        } else {
            /* FIFO+Prioridad: trabaja hasta terminar (no preemptivo) */
            char msg[64];
            snprintf(msg, sizeof(msg),
                     "FIFO: cargando %d unidades completas", c->burst_total);
            log_evento(c->id, msg);
            sleep(c->burst_total);

            sem_post(&sem_muelles);           /* libera muelle físico */
            pthread_mutex_lock(&mutex_planif);
            pthread_cond_signal(&cond_planif);
            pthread_mutex_unlock(&mutex_planif);
        }

        break; /* carga completada */
    }

    /* ── ESTADO: TERMINADO ── */
    camiones[c->id].t_fin     = tiempo_actual();
    camiones[c->id].t_retorno = camiones[c->id].t_fin - camiones[c->id].t_llegada;
    camiones[c->id].t_espera  = camiones[c->id].t_retorno - camiones[c->id].burst_total;
    if (camiones[c->id].t_espera < 0) camiones[c->id].t_espera = 0;

    /* Notificar al planificador que este camión terminó */
    pthread_mutex_lock(&mutex_planif);
    camiones_activos--;
    pthread_cond_signal(&cond_planif);
    pthread_mutex_unlock(&mutex_planif);

    cambiar_estado(c->id, ESTADO_TERMINADO);
    return NULL;
}

/* ─────────────────────────────────────────────
   Tabla comparativa de resultados
   ───────────────────────────────────────────── */
void imprimir_tabla_comparativa(Camion arr[], int n, Algoritmo algo)
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║        ANÁLISIS COMPARATIVO – Algoritmo: %-6s                     ║\n",
           algo == PROCESO_FIFO ? "FIFO" : "RR");
    printf("╠═══╦══════════╦══════════╦══════════╦══════════╦══════════╦══════════╣\n");
    printf("║ # ║ Tipo     ║ Burst    ║ Llegada  ║ T.Espera ║T.Retorno ║ Estado   ║\n");
    printf("╠═══╬══════════╬══════════╬══════════╬══════════╬══════════╬══════════╣\n");

    double sum_espera = 0, sum_retorno = 0;
    for (int i = 0; i < n; i++) {
        printf("║%2d ║ %-8s ║  %5d   ║  %6.2f  ║  %6.2f  ║  %6.2f  ║ %-8s ║\n",
               arr[i].id,
               nombre_carga[arr[i].tipo_carga],
               arr[i].burst_total,
               arr[i].t_llegada,
               arr[i].t_espera,
               arr[i].t_retorno,
               nombre_estado[arr[i].estado]);
        sum_espera  += arr[i].t_espera;
        sum_retorno += arr[i].t_retorno;
    }

    printf("╠═══╩══════════╩══════════╩══════════╬══════════╬══════════╬══════════╣\n");
    printf("║                             PROMEDIO ║  %6.2f  ║  %6.2f  ║          ║\n",
           sum_espera / n, sum_retorno / n);
    printf("╚═══════════════════════════════════════╩══════════╩══════════╩══════════╝\n");

    if (archivo_log) {
        fprintf(archivo_log,
                "\n[RESUMEN %s] T.Espera promedio=%.2f | T.Retorno promedio=%.2f\n",
                algo == PROCESO_FIFO ? "FIFO" : "RR",
                sum_espera / n, sum_retorno / n);
    }
}

/* ─────────────────────────────────────────────
   Ejecutar una simulación completa
   ───────────────────────────────────────────── */
void ejecutar_simulacion(Algoritmo algo, int n_camiones)
{
    algoritmo_actual = algo;
    total_camiones   = n_camiones;
    camiones_activos = n_camiones;

    /* Inicializar semáforo, cola y variables del planificador */
    sem_init(&sem_muelles, 0, NUM_MUELLES);
    cola_init(&cola_listos);
    for (int i = 0; i < n_camiones; i++) {
        pthread_cond_init(&cond_camion[i], NULL);
        seleccionado[i] = 0;
    }

    /* Inicializar camiones con datos de prueba */
    TipoCarga tipos[]  = { CARGA_PERECEDERO, CARGA_INDUSTRIAL, CARGA_GENERAL,
                           CARGA_PERECEDERO, CARGA_GENERAL,    CARGA_INDUSTRIAL };
    int bursts[]       = { 3, 5, 2, 4, 1, 6 };

    pthread_t hilos[MAX_CAMIONES];
    pthread_t th_planificador;

    for (int i = 0; i < n_camiones; i++) {
        camiones[i].id            = i;
        camiones[i].tipo_carga    = tipos[i % 6];
        camiones[i].burst_total   = bursts[i % 6];
        camiones[i].burst_restante= bursts[i % 6];
        camiones[i].prioridad     = (int)camiones[i].tipo_carga; /* 0=perecedero */
        camiones[i].estado        = ESTADO_CREADO;
        camiones[i].t_llegada     = 0;
        camiones[i].t_inicio      = 0;
        camiones[i].t_fin         = 0;
        camiones[i].t_espera      = 0;
        camiones[i].t_retorno     = 0;
        camiones[i].algoritmo     = algo;
    }

    printf("\n▶ Iniciando simulación con algoritmo: %s (%d camiones, %d muelles)\n\n",
           algo == PROCESO_FIFO ? "FIFO (Prioridad)" : "Round Robin",
           n_camiones, NUM_MUELLES);

    if (archivo_log)
        fprintf(archivo_log, "\n=== SIMULACIÓN %s ===\n",
                algo == PROCESO_FIFO ? "FIFO" : "RR");

    /* Crear hilo planificador (antes que los camiones) */
    pthread_create(&th_planificador, NULL, hilo_planificador, NULL);

    /* Crear hilos */
    for (int i = 0; i < n_camiones; i++) {
        usleep(100000); /* separación de 100ms entre llegadas */
        pthread_create(&hilos[i], NULL, hilo_camion, &camiones[i]);
    }

    /* Esperar a todos (pthread_join recolecta → evita zombie threads) */
    for (int i = 0; i < n_camiones; i++) {
        pthread_join(hilos[i], NULL);
    }
    pthread_join(th_planificador, NULL);

    /* Destruir semáforo y variables de condición */
    sem_destroy(&sem_muelles);
    for (int i = 0; i < n_camiones; i++) {
        pthread_cond_destroy(&cond_camion[i]);
    }

    /* Mostrar tabla */
    imprimir_tabla_comparativa(camiones, n_camiones, algo);
}
