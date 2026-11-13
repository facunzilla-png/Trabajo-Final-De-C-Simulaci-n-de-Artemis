
#ifndef INTERFAZ_H
#define INTERFAZ_H

#include "artemis.h"

#define NUM_CAMPOS   8     
#define TAM_CAMPO    32    

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


typedef struct {
    Pantalla pantalla;         
    int salir;                    

    DatosMision datos;            
    int datos_personalizados;     
    int misiones_jugadas;

    const Dificultad *dificultad;   
    Reporte reporte;               
    int hay_reporte;               

    float gamma;                    
    float gamma_analisis;
    float gamma_calculado;          
    int analisis_calculado;          
    EstadoAngulo ultimo_angulo;     
    int hubo_intento;               

    char campos[NUM_CAMPOS][TAM_CAMPO]; 
    int campo_activo;                   
    char mensaje_config[TAM_MENSAJE];   
    int config_ok;                      
} Juego;

void juego_iniciar(Juego *j);           
void iniciar_interfaz(void);           
int  interfaz_activa(const Juego *j);   
void interfaz_dibujar(Juego *j);        
void cerrar_interfaz(void);             

#endif 