/*
 * interfaz.h
 * Interfaz gráfica (raylib + raygui) del simulador de reingreso.
 *
 * Idea clave: en una interfaz gráfica no se "espera" al usuario. main.c repite un
 * bucle ~60 veces por segundo y en cada vuelta llama a interfaz_dibujar(), que
 * DIBUJA la pantalla actual y REVISA si se tocó algún botón. Cuál es la pantalla
 * actual se guarda en juego->pantalla (esto se llama "máquina de estados").
 */
#ifndef INTERFAZ_H
#define INTERFAZ_H

#include "artemis.h"

#define NUM_CAMPOS   8      /* cuadros de texto de la pantalla de configuración */
#define TAM_CAMPO    32     /* largo máximo de cada cuadro de texto             */

typedef enum {
    PANTALLA_MENU,
    PANTALLA_CONFIG,
    PANTALLA_ANALISIS,
    PANTALLA_REGLAS,
    PANTALLA_DIFICULTAD,
    PANTALLA_EVENTO1,
    PANTALLA_EVENTO2,
    PANTALLA_EVENTO3,
    PANTALLA_REPORTE
} Pantalla;

/* Todo el estado del programa en un solo lugar. */
typedef struct {
    Pantalla pantalla;              /* pantalla que se está mostrando */
    int salir;                      /* 1 = cerrar el programa */

    DatosMision datos;              /* datos de la misión actuales */
    int datos_personalizados;       /* 0 = del enunciado, 1 = editados */
    int misiones_jugadas;

    const Dificultad *dificultad;   /* dificultad de la misión en curso */
    Reporte reporte;                /* resultados de la misión en curso */
    int hay_reporte;                /* 1 = hay al menos una misión terminada */

    float gamma;                    /* ángulo elegido en el evento 1 */
    float gamma_analisis;           /* ángulo elegido en la pantalla de análisis */
    EstadoAngulo ultimo_angulo;     /* resultado del último intento de ángulo */
    int hubo_intento;               /* 1 = ya se confirmó algún ángulo */

    char campos[NUM_CAMPOS][TAM_CAMPO];  /* texto de los cuadros de configuración */
    int campo_activo;                    /* cuadro que se está editando (-1 = ninguno) */
    char mensaje_config[TAM_MENSAJE];    /* mensaje bajo los cuadros de texto */
    int config_ok;                       /* 1 = el mensaje es de éxito, 0 = de error */
} Juego;

void juego_iniciar(Juego *j);           /* deja el estado listo para empezar */
void iniciar_interfaz(void);            /* abre la ventana */
int  interfaz_activa(const Juego *j);   /* 1 mientras el programa deba seguir */
void interfaz_dibujar(Juego *j);        /* dibuja un cuadro y atiende los botones */
void cerrar_interfaz(void);             /* cierra la ventana */

#endif /* INTERFAZ_H */
