
#include "terminal.h"



static void mostrar_menu(void)
{
    printf("\n┌─────────────────────────────────────────┐\n");
    printf("│          MENÚ PRINCIPAL                 │\n");
    printf("├─────────────────────────────────────────┤\n");
    printf("│  1. Ejecutar simulación FIFO            │\n");
    printf("│  2. Ejecutar simulación Round Robin     │\n");
    printf("│  3. Ejecutar AMBAS y comparar           │\n");
    printf("│  4. Salir                               │\n");
    printf("└─────────────────────────────────────────┘\n");
    printf("Opción: ");
}

int main(void)
{
    archivo_log = fopen(LOG_FILE, "w");
    if (!archivo_log) {
        perror("Error al abrir archivo de log");
        return EXIT_FAILURE;
    }
    fprintf(archivo_log, "=== LOG DE OPERACIONES – Terminal de Carga ===\n\n");

    int opcion      = 0;
    int n_camiones  = 0;

    printf("¿Cuántos camiones desea simular? (1–%d): ", MAX_CAMIONES);
    if (scanf("%d", &n_camiones) != 1 || n_camiones < 1 || n_camiones > MAX_CAMIONES) {
        printf("Valor inválido. Usando 6 camiones por defecto.\n");
        n_camiones = 6;
    }

    mostrar_menu();
    if (scanf("%d", &opcion) != 1) opcion = 4;

    switch (opcion) {
        case 1:
            ejecutar_simulacion(PROCESO_FIFO, n_camiones);
            break;

        case 2:
            ejecutar_simulacion(PROCESO_RR, n_camiones);
            break;

        case 3:
        
            ejecutar_simulacion(PROCESO_FIFO, n_camiones);

            printf("\n\n══════ Preparando segunda corrida con Round Robin ══════\n");
            sleep(1);

            ejecutar_simulacion(PROCESO_RR, n_camiones);

            printf("\n");
            printf("╔══════════════════════════════════════════════════════════╗\n");
            printf("║           COMPARACIÓN FINAL DE ALGORITMOS               ║\n");
            printf("╠═══════════════════╦══════════════════╦═══════════════════╣\n");
            printf("║ Métrica           ║ FIFO (Prioridad) ║   Round Robin     ║\n");
            printf("╠═══════════════════╬══════════════════╬═══════════════════╣\n");
            printf("║ Preemptivo        ║       No         ║       Sí          ║\n");
            printf("║ Efecto Convoy     ║   Posible        ║   Mitigado        ║\n");
            printf("║ Cambio contexto   ║     Bajo         ║    Alto           ║\n");
            printf("║ Equidad           ║  Por prioridad   ║  Por tiempo       ║\n");
            printf("╚═══════════════════╩══════════════════╩═══════════════════╝\n");
            printf("\nVer tabla de tiempos individuales arriba para cada corrida.\n");
            break;

        case 4:
        default:
            printf("Saliendo...\n");
            break;
    }

    if (archivo_log) {
        fclose(archivo_log);
        printf("\nLog de operaciones guardado en: %s\n", LOG_FILE);
    }

    return EXIT_SUCCESS;
}
