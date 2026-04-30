#include "estadisticas.h"
 
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
 
void imprimir_comparacion_final(void)
{
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
}