# 🚛 Terminal de Carga Automatizada

**EIF-212 Sistemas Operativos — I Ciclo 2026**

Simulación de una terminal logística donde múltiples camiones compiten por muelles de carga limitados, coordinados mediante semáforos, mutex y dos algoritmos de planificación.

---

## 📁 Archivos

| Archivo | Descripción |
|---|---|
| `terminal.h` | Definiciones y estructuras |
| `terminal.c` | Implementación: hilos, sincronización, cola, log |
| `planificador.c` | Lógica FIFO y Round Robin |
| `cola.c` | Cola circular de planificación |
| `estadisticas.c` | Tabla comparativa de tiempos |
| `Makefile` | Compilación con `-pthread` |

---

## ⚙️ Compilación y uso

```bash
make
./terminal_carga
```

1. Ingresar número de camiones (ej: `6`)
2. Elegir algoritmo:
   - `1` — FIFO con prioridad
   - `2` — Round Robin
   - `3` — Ambos (muestra tabla comparativa al final)
3. Los eventos se registran en `operaciones.log` con marca de tiempo

---

## 🔄 Ciclo de vida del hilo

```
CREADO → NUEVO → LISTO → EJECUCIÓN ⇄ BLOQUEADO → TERMINADO
```

---

## 🔒 Sincronización

| Mecanismo | Tipo | Función |
|---|---|---|
| `sem_muelles` | Semáforo | Inicializado en `3` (`NUM_MUELLES`); limita los camiones cargando simultáneamente |
| `mutex_log` | Mutex | Protege escrituras en consola y archivo de log; evita condiciones de carrera |
| `mutex_cola` | Mutex | Protege la cola de planificación contra accesos concurrentes |
| `mutex_estado` | Mutex | Protege transiciones de estado: `LISTO → EJECUCIÓN → BLOQUEADO` |

---

## 🧱 Estructuras principales

**`Camion`** — parámetros y métricas de cada hilo:

```c
typedef struct {
    int        id;
    TipoCarga  tipo_carga;     // PERECEDERO, INDUSTRIAL, GENERAL
    int        burst_total;    // unidades de trabajo totales
    int        burst_restante; // para Round Robin
    int        prioridad;      // 0 = máxima prioridad
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

## 📊 Algoritmos de planificación

### FIFO con prioridad

> No preemptivo

- El camión ocupa el muelle hasta completar su `burst_total`
- Extrae el camión con menor valor de prioridad (`0` = perecedero = más urgente)
- Puede generar efecto convoy si un burst largo bloquea a camiones cortos

### Round Robin (Q = 2)

> Preemptivo

- Cada camión usa el muelle por máximo 2 unidades de tiempo (quantum)
- Si `burst_restante > 0` al expirar el quantum, el hilo se reencola
- Mayor equidad, a costo de más cambios de contexto

| Métrica | FIFO | Round Robin |
|---|---|---|
| Preemptivo | No | Sí |
| Orden de atención | Por prioridad de carga | Por orden de llegada |
| Efecto convoy | Posible | Mitigado |
| Cambios de contexto | Bajos | Altos |

---Documentación: Explicación de cómo evitaron el Interbloqueo (Deadlock)---

El interbloqueo ocurre cuando dos o más hilos se bloquean mutuamente esperando recursos que el otro retiene.
En este proyecto se tomaron las siguientes medidas para prevenirlo:

1. Orden fijo de adquisición de recursos: todos los hilos adquieren los mutex siempre en el mismo orden
   (mutex_cola → mutex_estado → mutex_log), eliminando la posibilidad de espera circular.

2. sem_muelles como semáforo contable: en lugar de múltiples mutex individuales por muelle, se usa un único
   semáforo inicializado en NUM_MUELLES (3). Esto garantiza que un hilo nunca retiene un muelle mientras
   espera otro, evitando retención y espera.

3. No hay espera circular: un camión adquiere sem_muelles (sem_wait), realiza su carga y libera (sem_post)
   antes de acceder a cualquier otro recurso protegido por mutex. Nunca se retiene el semáforo mientras se
   espera un mutex, ni viceversa.

4. mutex_log y mutex_estado son de uso breve y no anidado: se toman y liberan dentro de secciones críticas
   pequeñas, reduciendo la ventana de tiempo en que un hilo retiene el recurso.

