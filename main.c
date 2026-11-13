
#include <stdlib.h>
#include <time.h>
#include "interfaz.h"

int main(void)
{
    Juego juego;

    srand((unsigned)time(NULL));
    juego_iniciar(&juego);
    iniciar_interfaz();

    while (interfaz_activa(&juego)) {
        interfaz_dibujar(&juego);
    }

    cerrar_interfaz();
    return 0;
}
