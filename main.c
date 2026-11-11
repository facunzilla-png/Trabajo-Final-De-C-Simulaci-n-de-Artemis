/*
 * main.c
 * Punto de entrada de la versión gráfica.
 * Abre la ventana y repite el bucle del programa: cada vuelta dibuja un cuadro
 * (~60 por segundo) y atiende los botones. Toda la lógica está en artemis.c y
 * todo el dibujo, en interfaz.c.
 */
#include "interfaz.h"

int main(void)
{
    Juego juego;

    juego_iniciar(&juego);
    iniciar_interfaz();

    while (interfaz_activa(&juego)) {
        interfaz_dibujar(&juego);
    }

    cerrar_interfaz();
    return 0;
}
