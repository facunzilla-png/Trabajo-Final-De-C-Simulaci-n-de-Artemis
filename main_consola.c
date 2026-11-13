/*
 * main.c
 * Menú principal y flujo general del programa.
 * Toda la lógica está en artemis.c; todas las lecturas seguras, en validaciones.c.
 */
#include <stdio.h>
#include "artemis.h"
#include "validaciones.h"

#ifdef _WIN32
#include <windows.h>   /* para mostrar bien tildes y símbolos en la consola */
#endif

static void mostrar_menu(void)
{
    puts("\n==========================================");
    puts("   ARTEMIS - REINGRESO Y AMERIZAJE (Orion)");
    puts("==========================================");
    puts("  1. Configurar datos de la misión");
    puts("  2. Jugar misión de reingreso");
    puts("  3. Análisis analítico (preguntas del escenario)");
    puts("  4. Ver reporte de la última misión");
    puts("  5. Reglas del juego");
    puts("  0. Salir");
}

int main(void)
{
    DatosMision datos;
    Reporte ultimo;
    int hay_reporte = 0;
    int opcion;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    cargar_datos_por_defecto(&datos);

    do {
        limpiar_pantalla();
        mostrar_menu();
        opcion = leer_entero("Elegí una opción", 0, 5);
        limpiar_pantalla();

        switch (opcion) {
        case 1:
            mostrar_datos(&datos);
            if (leer_caracter("¿Usar los datos del enunciado? (s/n)", "sn") == 's') {
                cargar_datos_por_defecto(&datos);
                puts("Se cargaron los datos del enunciado.");
            } else {
                ingresar_datos_mision(&datos);
                puts("Datos actualizados.");
            }
            pausar();
            break;

        case 2:
            ejecutar_mision(&datos, seleccionar_dificultad(), &ultimo);
            hay_reporte = 1;
            mostrar_reporte(&ultimo);
            pausar();
            break;

        case 3:
            mostrar_datos(&datos);
            mostrar_analisis(&datos,
                             leer_double("Ángulo gamma a analizar (grados)", 0.0, 90.0));
            pausar();
            break;

        case 4:
            if (hay_reporte) {
                mostrar_reporte(&ultimo);
            } else {
                mostrar_advertencia("Todavía no jugaste ninguna misión.");
            }
            pausar();
            break;

        case 5:
            mostrar_reglas();
            pausar();
            break;

        case 0:
            puts("\nFin del programa. ¡Buen viaje!");
            break;
        }
    } while (opcion != 0);

    return 0;
}
