#include "cola.h"
#include "terminal.h"   /* definición completa de Camion */
 
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
 
int cola_desencolar(Cola *c)
{
    if (c->tam == 0) return -1;
    int id = c->ids[c->frente];
    c->frente = (c->frente + 1) % MAX_CAMIONES;
    c->tam--;
    return id;
}
 
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
    for (int i = 0; i < c->tam - 1; i++) {
        int pos_actual    = (mejor_pos + i)     % MAX_CAMIONES;
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