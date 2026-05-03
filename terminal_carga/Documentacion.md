Terminal de Carga Automatizada
EIF-212 Sistemas Operativos — I Ciclo 2026

Simulación de una terminal logística donde múltiples camiones compiten por muelles de carga limitados, 
coordinados mediante semáforos, mutex y dos algoritmos de planificación.


| `terminal.h` | Definiciones, estructuras |
| `terminal.c` | Implementación: hilos, sincronización, cola, log |
| `Makefile` | Compilación con `-pthread` |

---Compilacion y uso---
make
./terminal_carga

Ingresar número de camiones (ej: 6)
1. 	Elegir algoritmo: 1 = FIFO, 2 = Round Robin, 3 = Ambos
2.	Los eventos se registran en operaciones.log con marca de tiempo

---Sincronización---

1. sem_muelles — semáforo inicializado en 3 (NUM_MUELLES); limita los camiones cargando simultáneamente
2. mutex_log — protege escrituras en consola y archivo de log; evita condiciones de carrera
3. mutex_cola — protege la cola de planificación contra accesos concurrentes
4. mutex_estado — protege transiciones de estado de cada hilo (LISTO → EJECUCION → BLOQUEADO)

---Estructuras usadas---

Camion— parámetros y métricas de cada hilo:

typedef struct {
int id;
TipoCarga tipo_carga; // PERECEDERO, INDUSTRIAL, GENERAL
int burst_total; // unidades de trabajo totales
int burst_restante; // para Round Robin
int prioridad; // 0 = máxima prioridad
EstadoHilo estado;
double } Camion;
t_llegada, t_inicio, t_fin, t_espera, t_retorno;


Cola — cola circular de IDs para la planificación:

typedef struct {
int ids[MAX_CAMIONES];
int frente, fin, tam;
} Cola;


---Algoritmos de planificación---

~FIFO con Prioridad

1. No preemptivo: el camión ocupa el muelle hasta completar su burst_total
2. Extrae el camión con menor valor de prioridad (0 = perecedero = más urgente)
3. Puede generar efecto convoy si un burst largo bloquea a camiones cortos

~Round Robin (Q = 2)

1. Preemptivo: cada camión usa el muelle por máximo 2 unidades de tiempo
2. Si burst_restante > 0 al expirar el quantum, el hilo se reencola
3. Mayor equidad, a costo de más cambios de contexto




