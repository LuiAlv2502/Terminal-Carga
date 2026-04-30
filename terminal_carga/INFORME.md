# Sistema de Gestión de Terminal de Carga Automatizada
## EIF-212 Sistemas Operativos — I Ciclo 2026

---

## 1. Descripción General

El programa simula una terminal logística donde múltiples **camiones** (hilos POSIX) compiten por el acceso a un número limitado de **muelles de carga** (recurso compartido). El servidor de la terminal coordina el acceso mediante semáforos, mutex y dos algoritmos de planificación: **FIFO con prioridad** y **Round Robin**.

---

## 2. Arquitectura del Sistema

### Módulos

| Archivo | Rol |
|---|---|
| `terminal.h` | Definiciones, estructuras y prototipos |
| `terminal.c` | Implementación: hilos, sincronización, cola, log |
| `main.c` | Servidor principal y menú interactivo |
| `Makefile` | Compilación con `-pthread` |

### Estructuras principales

**`Camion`** — parámetros y métricas de cada hilo:
```c
typedef struct {
    int        id;
    TipoCarga  tipo_carga;   // PERECEDERO, INDUSTRIAL, GENERAL
    int        burst_total;  // unidades de trabajo totales
    int        burst_restante; // para Round Robin
    int        prioridad;    // 0 = máxima prioridad
    EstadoHilo estado;
    double     t_llegada, t_inicio, t_fin, t_espera, t_retorno;
} Camion;
```

**`Cola`** — cola circular de IDs para la planificación:
```c
typedef struct {
    int ids[MAX_CAMIONES];
    int frente, fin, tam;
} Cola;
```

---

## 3. Sincronización y Secciones Críticas

### 3.1 Semáforo de Muelles (`sem_muelles`)

```c
sem_init(&sem_muelles, 0, NUM_MUELLES); // NUM_MUELLES = 3
```

Controla cuántos hilos pueden estar "cargando" simultáneamente. Al intentar adquirir el semáforo con `sem_wait()`, si el valor es 0, el hilo se bloquea. Esto garantiza que **a lo sumo 3 camiones** usen un muelle al mismo tiempo.

### 3.2 Mutex del Log (`mutex_log`)

```c
pthread_mutex_lock(&mutex_log);
// ← Sección crítica: escritura en archivo y consola
pthread_mutex_unlock(&mutex_log);
```

Protege el buffer/archivo de log compartido. Sin este mutex, múltiples hilos escribirían simultáneamente causando **condición de carrera** (salidas mezcladas o corrupción del archivo).

### 3.3 Mutex de Cola (`mutex_cola`)

Protege la cola de planificación compartida contra accesos concurrentes al encolar y desencolar.

### 3.4 Mutex de Estado (`mutex_estado`)

Protege las actualizaciones del campo `estado` de cada camión, reportando correctamente la transición `EJECUCION → BLOQUEADO → EJECUCION` cuando un hilo espera el `mutex_log`.

---

## 4. Ciclo de Vida del Hilo (Estados)

```
pthread_create()
      │
      ▼
   [NUEVO]
      │  ingresa a la cola
      ▼
   [LISTO] ◄──────────────────────────┐
      │  sem_wait() + planificador     │ (RR: quantum expirado)
      ▼                               │
  [EJECUCION]  ──► [BLOQUEADO]  ──►  ┤
      │           (espera mutex_log)  │
      │  carga completa               │
      ▼                               │
  [TERMINADO]                         │
      │                               │
   pthread_join() ───────────────────►┘  (nunca si terminó)
```

Cada transición se registra en el log con marca de tiempo.

---

## 5. Algoritmos de Planificación

### 5.1 FIFO con Prioridad (`cola_desencolar_prioridad`)

- **No preemptivo**: el camión que obtiene el muelle lo ocupa hasta terminar su carga completa (`burst_total`).
- La cola se recorre para extraer el camión de **menor valor de prioridad** (0 = perecedero = más urgente).
- Puede producir **efecto convoy** si un camión con burst largo bloquea a camiones cortos de menor prioridad.

### 5.2 Round Robin (`cola_desencolar` + quantum)

- **Preemptivo**: cada camión usa el muelle por máximo `QUANTUM_RR = 2` segundos simulados.
- Si `burst_restante > 0` al expirar el quantum, el hilo **libera el semáforo**, cambia a LISTO y se reencola.
- Garantiza **equidad temporal**: ningún camión monopoliza el muelle.
- Costo: mayor número de cambios de contexto (cada reencola implica otra iteración del planificador).

---

## 6. Prevención de Interbloqueo (Deadlock)

Se aplicó la estrategia de **ordenamiento estricto de adquisición de recursos**:

1. Nunca se adquieren dos mutex al mismo tiempo en orden inverso.
2. El `mutex_estado` se adquiere y libera antes de tomar `mutex_log`.
3. El semáforo `sem_muelles` se libera siempre con `sem_post()` incluso en la ruta de reencola del Round Robin, garantizando que el recurso siempre sea devuelto.
4. No hay **espera circular**: los hilos esperan muelle → log, nunca log → muelle.
5. `pthread_join()` garantiza que el proceso principal no termina hasta recolectar todos los hilos (evita recursos huérfanos).

---

## 7. Tabla Comparativa de Resultados (ejemplo con 6 camiones)

| # | Tipo | Burst | FIFO T.Espera | FIFO T.Retorno | RR T.Espera | RR T.Retorno |
|---|---|---|---|---|---|---|
| 0 | Perecedero | 3 | 0.00 | 3.00 | 1.10 | 4.10 |
| 1 | Industrial | 5 | 0.00 | 5.00 | 1.90 | 6.90 |
| 2 | General | 2 | 0.00 | 2.00 | 0.00 | 2.00 |
| 3 | Perecedero | 4 | 1.90 | 5.90 | 1.80 | 5.80 |
| 4 | General | 1 | 4.70 | 5.70 | 1.70 | 2.70 |
| 5 | Industrial | 6 | 2.50 | 8.50 | 1.70 | 7.70 |
| **Prom.** | | | **1.52** | **5.02** | **1.37** | **4.87** |

### Observaciones

| Algoritmo | T.Espera Prom. | T.Retorno Prom. | Observaciones |
|---|---|---|---|
| FIFO (Prioridad) | 1.52 s | 5.02 s | Los primeros camiones (3 muelles libres) tienen espera 0. El efecto convoy aparece en camiones tardíos si un burst largo ocupa un muelle. |
| Round Robin | 1.37 s | 4.87 s | Mayor equidad: ningún camión espera demasiado. El overhead de reencola aumenta el número de operaciones pero reduce la varianza. |

**Conclusión**: Round Robin produce menor tiempo de espera promedio y mayor equidad, al costo de más cambios de contexto. FIFO con prioridad beneficia directamente a camiones perecederos de alta prioridad con espera cero cuando hay muelles disponibles.

---

## 8. Compilación y Uso

```bash
make
./terminal_carga
# → ingresar número de camiones (ej: 6)
# → elegir opción: 1=FIFO, 2=RR, 3=Ambas
```

El archivo `operaciones.log` registra todos los eventos con marca de tiempo.
