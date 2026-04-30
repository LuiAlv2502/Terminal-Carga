#ifndef COLA_H
#define COLA_H
 
#define MAX_CAMIONES 20
 
/* ─────────────────────────────────────────────
   Cola de planificación (lista circular)
   ───────────────────────────────────────────── */
typedef struct {
    int ids[MAX_CAMIONES];
    int frente;
    int fin;
    int tam;
} Cola;
 
extern Cola cola_listos;
 
void cola_init(Cola *c);
void cola_encolar(Cola *c, int id);
int  cola_desencolar(Cola *c);
int  cola_vacia(Cola *c);
 
/*
 * Requiere la definición completa de Camion.
 * Se declara en terminal.h (después de definir Camion)
 * para evitar dependencia circular.
 */
 
#endif /* COLA_H */