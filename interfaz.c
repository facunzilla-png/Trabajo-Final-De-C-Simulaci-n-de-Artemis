#include <stdio.h>
#include <string.h>
#include <math.h>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION     
#include "raygui.h"
#include "interfaz.h"
#include "validaciones.h"

#define ANCHO_VENTANA   900
#define ALTO_VENTANA    560
#define GAMMA_ESCALA    15.0f       

#define COLOR_FONDO     (Color){  12,  18,  38, 255 }
#define COLOR_TEXTO     (Color){ 230, 235, 255, 255 }
#define COLOR_SUAVE     (Color){ 160, 172, 205, 255 }
#define COLOR_OK        (Color){  60, 220, 120, 255 }
#define COLOR_AVISO     (Color){ 255, 190,  60, 255 }
#define COLOR_ERROR     (Color){ 240,  80,  90, 255 }
#define COLOR_BARRA     (Color){  50,  60,  95, 255 }

typedef struct {
    const char *etiqueta;
    double minimo;
    double maximo;
} CampoConfig;

static const CampoConfig CAMPOS[NUM_CAMPOS] = {
    { "Masa de la cápsula (kg)",          1000.0,   50000.0 },
    { "Velocidad de reingreso (m/s)",      100.0,   15000.0 },
    { "Altitud de reingreso (m)",        10000.0,  200000.0 },
    { "Vel. al abrir paracaídas (m/s)",      1.0,     200.0 },
    { "Altitud de paracaídas (m)",         100.0,   10000.0 },
    { "Velocidad de impacto (m/s)",          0.5,      30.0 },
    { "Duración del frenado (s)",           10.0,    2000.0 },
    { "Hundimiento en el agua (m)",          0.1,      10.0 }
};


static int boton(int x, int y, int ancho, int alto, const char *texto)
{
    return GuiButton((Rectangle){ (float)x, (float)y, (float)ancho, (float)alto }, texto);
}

static int boton_doble(int x, int y, int ancho, int alto,
                       const char *linea1, const char *linea2)
{
    int clic = GuiButton((Rectangle){ (float)x, (float)y, (float)ancho, (float)alto }, "");
    int deshabilitado = (GuiGetState() == STATE_DISABLED);

    Color c1 = deshabilitado ? (Color){ 111, 122, 168, 255 } : COLOR_TEXTO;
    Color c2 = deshabilitado ? (Color){  85,  95, 135, 255 } : COLOR_AVISO;

    int w1 = MeasureText(linea1, 18);
    int w2 = MeasureText(linea2, 14);

    DrawText(linea1, x + (ancho - w1) / 2, y + 7,  18, c1);
    DrawText(linea2, x + (ancho - w2) / 2, y + 28, 14, c2);

    return clic;
}


static void dibujar_barra(int x, int y, int ancho, const char *titulo,
                          double valor, double limite, const char *unidad)
{
    double escala = limite * 1.5;             
    int lleno = (int)(valor / escala * ancho);
    Color color = (valor <= 0.8 * limite) ? COLOR_OK
                : (valor <= limite)       ? COLOR_AVISO
                                          : COLOR_ERROR;

    if (lleno > ancho) lleno = ancho;
    if (lleno < 0)     lleno = 0;

    DrawText(titulo, x, y, 18, COLOR_TEXTO);
    DrawRectangle(x, y + 24, ancho, 22, COLOR_BARRA);
    DrawRectangle(x, y + 24, lleno, 22, color);
    DrawRectangle(x + (int)(ancho / 1.5), y + 20, 2, 30, COLOR_TEXTO);   
    DrawText(TextFormat("%.2f %s (límite %.1f)", valor, unidad, limite),
             x, y + 52, 16, COLOR_SUAVE);
}

static void dibujar_ventana_angulo(int x, int y, int ancho, float gamma)
{
    float u  = ancho / GAMMA_ESCALA;          
    int   x1 = x + (int)(GAMMA_MIN * u);
    int   x2 = x + (int)(GAMMA_MAX * u);

    DrawRectangle(x,  y, x1 - x,         30, COLOR_AVISO);   
    DrawRectangle(x1, y, x2 - x1,        30, COLOR_OK);     
    DrawRectangle(x2, y, x + ancho - x2, 30, COLOR_ERROR);   
    DrawRectangle(x + (int)(gamma * u) - 2, y - 8, 4, 46, COLOR_TEXTO);  

    DrawText("rebote",   x + 4,          y + 36, 16, COLOR_AVISO);
    DrawText("seguro",   x1 - 6,         y + 36, 16, COLOR_OK);
    DrawText("excesivo", x + ancho - 80, y + 36, 16, COLOR_ERROR);
}


static void datos_a_campos(const DatosMision *d, char campos[][TAM_CAMPO])
{
    const double valores[NUM_CAMPOS] = {
        d->masa, d->v_inicial, d->h_inicial, d->v_paracaidas,
        d->h_paracaidas, d->v_impacto, d->dt_frenado, d->d_hundimiento
    };

    for (int i = 0; i < NUM_CAMPOS; i++) {
        snprintf(campos[i], TAM_CAMPO, "%g", valores[i]);
    }
}


static int campos_a_datos(char campos[][TAM_CAMPO], DatosMision *salida,
                          char *mensaje, int tam)
{
    double v[NUM_CAMPOS];
    char error[TAM_MENSAJE];

    for (int i = 0; i < NUM_CAMPOS; i++) {
        if (!validar_double(campos[i], CAMPOS[i].minimo, CAMPOS[i].maximo,
                            &v[i], error, TAM_MENSAJE)) {
            snprintf(mensaje, (size_t)tam, "%s: %s", CAMPOS[i].etiqueta, error);
            return 0;
        }
    }

    salida->masa          = v[0];
    salida->v_inicial     = v[1];
    salida->h_inicial     = v[2];
    salida->v_paracaidas  = v[3];
    salida->h_paracaidas  = v[4];
    salida->v_impacto     = v[5];
    salida->dt_frenado    = v[6];
    salida->d_hundimiento = v[7];

    return datos_coherentes(salida, mensaje, tam);
}

static void entrar_config(Juego *j)
{
    datos_a_campos(&j->datos, j->campos);
    j->campo_activo = -1;
    j->mensaje_config[0] = '\0';
    j->pantalla = PANTALLA_CONFIG;
}

static void iniciar_mision(Juego *j, const Dificultad *dif)
{
    j->dificultad = dif;
    iniciar_reporte(&j->reporte, dif);
    j->gamma = (float)j->reporte.gamma;
    j->hubo_intento = 0;
    j->pantalla = PANTALLA_EVENTO1;
}
static void pasar_a_frenado(Juego *j)
{
    iniciar_frenado(&j->datos, &j->reporte);
    j->pantalla = PANTALLA_EVENTO2;
}

static void pasar_a_amerizaje(Juego *j)
{
    iniciar_amerizaje(&j->datos, &j->reporte);
    j->pantalla = PANTALLA_EVENTO3;
}

static void terminar_mision(Juego *j)
{
    calcular_puntaje(&j->reporte);
    j->hay_reporte = 1;
    j->misiones_jugadas++;
    j->pantalla = PANTALLA_REPORTE;
}

static void dibujar_escena_orion(int x, int y, int ancho, int alto, const Juego *j)
{
    float t = (float)GetTime();
    float dy = sinf(t * 2.5f) * 6.0f;       
    float pulso = sinf(t * 12.0f) * 3.0f;      

    int cx = x + ancho / 2 + 10;
    int cy = y + alto / 2 - 15 + (int)dy;


   
    DrawCircle(x + 45,  y + 40,  1.5f, COLOR_TEXTO);
    DrawCircle(x + 120, y + 85,  1.0f, COLOR_SUAVE);
    DrawCircle(x + 340, y + 50,  2.0f, COLOR_TEXTO);
    DrawCircle(x + 380, y + 140, 1.5f, COLOR_SUAVE);
    DrawCircle(x + 70,  y + 200, 1.0f, COLOR_TEXTO);
    DrawCircle(x + 310, y + 260, 1.5f, COLOR_SUAVE);

  
    DrawCircle(x + ancho / 2, y + alto + 260, 340, (Color){ 20, 50, 110, 140 });
    DrawCircle(x + ancho / 2, y + alto + 260, 315, (Color){ 35, 95, 190, 180 });
    DrawCircle(x + ancho / 2, y + alto + 260, 295, (Color){ 18, 42, 92, 255 });

  
    Vector2 p_origen = { (float)cx, (float)cy };
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 115), (float)(cy - 85 + pulso) }, 26.0f, (Color){ 240, 80, 40, 75 });
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 85),  (float)(cy - 62 - pulso) }, 16.0f, (Color){ 255, 160, 40, 130 });
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 50),  (float)(cy - 38) },         8.0f,  (Color){ 255, 230, 120, 200 });

  
    DrawCircle(cx + 18, cy + 16, 44 + pulso * 0.4f, (Color){ 255, 100, 30, 90 });
    DrawCircle(cx + 14, cy + 12, 34, (Color){ 255, 190, 60, 160 });

   
    Vector2 v_punta = { (float)(cx - 28), (float)(cy - 22) };
    Vector2 v_abajo = { (float)(cx - 4),  (float)(cy + 36) };
    Vector2 v_der   = { (float)(cx + 38), (float)(cy - 8)  };
    DrawTriangle(v_punta, v_abajo, v_der, (Color){ 210, 220, 240, 255 });

   
    Vector2 v_medio = { (float)(cx + 17), (float)(cy + 14) };
    DrawTriangle(v_punta, v_abajo, v_medio, (Color){ 150, 165, 195, 255 });

   
    DrawLineEx((Vector2){ (float)(cx - 4), (float)(cy + 36) },
               (Vector2){ (float)(cx + 38), (float)(cy - 8) },
               7.0f, (Color){ 255, 120, 40, 255 });

    
    DrawCircle(cx - 2, cy - 2, 6, COLOR_FONDO);
    DrawCircle(cx - 3, cy - 3, 4, (Color){ 100, 200, 255, 255 });
    DrawLineEx((Vector2){ (float)(cx - 28), (float)(cy - 22) },
               (Vector2){ (float)(cx - 16), (float)(cy - 10) },
               4.0f, COLOR_BARRA);

   
    DrawText(" CÁPSULA ORION // FASE DE ENTRADA ATMOSFÉRICA", x + 12, y + 12, 14, COLOR_SUAVE);
}

static void pantalla_menu(Juego *j)
{
    
    DrawText("ARTEMIS", 50, 36, 56, COLOR_TEXTO);
    DrawRectangle(50, 96, 350, 3, COLOR_AVISO);
    DrawText("Simulador de Reingreso y Amerizaje", 50, 108, 19, COLOR_SUAVE);

   
    DrawRectangle(50, 142, 350, 36, COLOR_BARRA);
    DrawText(TextFormat("Datos: %s  |  Misiones: %d",
                        j->datos_personalizados ? "Personalizados" : "Oficiales",
                        j->misiones_jugadas),
             62, 151, 17, COLOR_AVISO);

    
    if (boton(50, 195, 350, 58, "JUGAR MISIÓN")) {
        j->pantalla = PANTALLA_DIFICULTAD;
    }

  
    if (boton(50, 268, 168, 52, "Configurar datos"))   entrar_config(j);
    if (boton(232, 268, 168, 52, "Análisis físico")) {
        j->analisis_calculado = 0;
        j->pantalla = PANTALLA_ANALISIS;
    }

    if (boton(50, 332, 168, 52, "Reglas"))             j->pantalla = PANTALLA_REGLAS;

    if (!j->hay_reporte) {
        GuiDisable();
    }
    if (boton(232, 332, 168, 52, "Último reporte"))    j->pantalla = PANTALLA_REPORTE;
    GuiEnable();

   
    if (boton(50, 464, 350, 48, "Salir del simulador")) {
        j->salir = 1;
    }


    dibujar_escena_orion(435, 36, 420, 476, j);
}

static void pantalla_reglas(Juego *j)
{
    DrawText("REGLAS DEL SIMULADOR", 50, 22, 28, COLOR_TEXTO);
    DrawText("Sos el director de vuelo del reingreso de la cápsula Orion (Misión Artemis).",
             50, 56, 17, COLOR_SUAVE);

  
    DrawText("1) Evento 1 - Alineación de Reingreso con RCS (por turnos):", 50, 88, 18, COLOR_TEXTO);
    DrawText(TextFormat("   • Meta en t = 0: llegar con ángulo entre %.1f° y %.1f° Y giro estable (|omega| <= %.2f°/t).",
                        GAMMA_MIN, GAMMA_MAX, OMEGA_MAX_SEGURO),
             50, 112, 15, COLOR_OK);
    DrawText(TextFormat("   • La nave conserva su inercia de giro (omega). Usás pulsos suaves (±%.1f°/t, %d RCS) o fuertes\n"
                        "     (±%.1f°/t, %d RCS, recalienta toberas por 1 turno) guiándote por el Radar de anomalías.",
                        PULSO_SUAVE, COSTO_SUAVE, PULSO_FUERTE, COSTO_FUERTE),
             50, 132, 15, COLOR_SUAVE);

   
    DrawText("2) Evento 2 - Frenado Aerodinámico (4 fases de 100 s):", 50, 180, 18, COLOR_TEXTO);
    DrawText(TextFormat("   • Meta: frenar a <= %.0f m/s sin superar el límite de g ni el 100%% de calor del escudo.",
                        j->datos.v_paracaidas + MARGEN_VEL_PARAC),
             50, 204, 15, COLOR_OK);
    DrawText("   • En cada fase elegís el perfil del escudo según la densidad de la capa atmosférica:\n"
             "     Sustentación Alta (frena menos, baja g y enfría), Nominal o Inmersión Densa (frena más, sube g y calor).",
             50, 224, 15, COLOR_SUAVE);

  
    DrawText("3) Evento 3 - Paracaídas y Amerizaje (2 decisiones críticas):", 50, 272, 18, COLOR_TEXTO);
    DrawText("   • Decisión 1 (Paracaídas): Temprana (frena más, riesgo de rotura con ráfagas), Nominal o Tardía (+vel).",
             50, 296, 15, COLOR_SUAVE);
    DrawText("   • Decisión 2 (Entrada al agua): En Cuña (hunde más y amortigua), Nominal o Plana (frena en seco).\n"
             "     La fuerza resultante F = m(g + v²/2d) no debe superar la resistencia estructural del casco.",
             50, 316, 15, COLOR_SUAVE);

    
    DrawRectangle(50, 368, 800, 96, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(50, 368, 800, 96, COLOR_BARRA);
    DrawText(TextFormat("PUNTAJE: %d pts por evento superado (-%d pts por cada unidad de RCS usada en el Evento 1).",
                        PUNTOS_EVENTO, PENALIZACION_RCS),
             68, 380, 16, COLOR_AVISO);
    DrawText(TextFormat("BONUS DE PRECISIÓN: +%d pts si completás toda la misión con ángulo a ±%.2f° de %.2f°.",
                        BONUS_PRECISION, TOLERANCIA_BONUS, GAMMA_CENTRO),
             68, 406, 16, COLOR_AVISO);
    DrawText(TextFormat("Puntaje máximo: %d pts. La dificultad ajusta RCS, turnos, deriva y límites de g e impacto.",
                        NUM_EVENTOS * PUNTOS_EVENTO + BONUS_PRECISION),
             68, 432, 15, COLOR_SUAVE);

    if (boton(50, 484, 240, 48, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_config(Juego *j)
{
    DrawText("CONFIGURAR DATOS DE LA MISIÓN", 40, 22, 28, COLOR_TEXTO);
    DrawText("Aviso: Los minijuegos están calibrados con los datos del enunciado. Cambiarlos puede volver imposible la misión.",
             40, 54, 15, COLOR_AVISO);

    for (int i = 0; i < NUM_CAMPOS; i++) {
        int y = 80 + i * 42;

        DrawText(CAMPOS[i].etiqueta, 40, y + 6, 18, COLOR_TEXTO);
    
        if (GuiTextBox((Rectangle){ 370, (float)y, 170, 32 }, j->campos[i], TAM_CAMPO,
                       j->campo_activo == i)) {
            j->campo_activo = (j->campo_activo == i) ? -1 : i;
        }
        DrawText(TextFormat("[%g a %g]", CAMPOS[i].minimo, CAMPOS[i].maximo),
                 560, y + 8, 16, COLOR_SUAVE);
    }

    if (j->mensaje_config[0] != '\0') {
        DrawText(j->mensaje_config, 40, 425, 16, j->config_ok ? COLOR_OK : COLOR_ERROR);
    }

    if (boton(40, 480, 220, 50, "Aplicar datos")) {
        DatosMision nuevos;

        if (campos_a_datos(j->campos, &nuevos, j->mensaje_config, TAM_MENSAJE)) {
            j->datos = nuevos;
            j->datos_personalizados = 1;
            snprintf(j->mensaje_config, TAM_MENSAJE, "Datos actualizados.");
            j->config_ok = 1;
        } else {
            j->config_ok = 0;
        }
    }
    if (boton(280, 480, 260, 50, "Restaurar enunciado")) {
        cargar_datos_por_defecto(&j->datos);
        j->datos_personalizados = 0;
        datos_a_campos(&j->datos, j->campos);
        snprintf(j->mensaje_config, TAM_MENSAJE, "Se cargaron los datos del enunciado.");
        j->config_ok = 1;
    }
    if (boton(560, 480, 220, 50, "Volver al menú")) {
        j->campo_activo = -1;
        j->pantalla = PANTALLA_MENU;
    }
}

static void pantalla_dificultad(Juego *j)
{
    DrawText("ELEGÍ LA DIFICULTAD", 60, 40, 32, COLOR_TEXTO);

    for (int i = 0; i < NUM_DIFICULTADES; i++) {
        const Dificultad *dif = obtener_dificultad(i);
        int y = 120 + i * 110;

        if (boton(60, y, 240, 70, dif->nombre)) {
            iniciar_mision(j, dif);
        }
        DrawText(TextFormat("Frenado: máx. %.1f g   |   Amerizaje: máx. %.1fx el peso",
                            dif->limite_g_frenado, dif->limite_g_impacto),
                 330, y + 8, 19, COLOR_TEXTO);
        DrawText(TextFormat("RCS: %d u.  |  Turnos: %d  |  Deriva: ±%.2f°/turno",
                            dif->combustible_rcs, dif->turnos_rcs, dif->deriva_max),
                 330, y + 38, 18, COLOR_SUAVE);
    }

    if (boton(60, 480, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}


static void dibujar_trayectoria_analisis(int x, int y, int ancho, int alto, float gamma)
{
    EstadoAngulo estado = clasificar_angulo(gamma);
    Color color_curva = (estado == ANGULO_SEGURO)   ? COLOR_OK
                      : (estado == ANGULO_EXCESIVO) ? COLOR_ERROR
                                                    : COLOR_AVISO;

  
    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

    
    int y_atm = y + 95;
    int y_mar = y + alto - 36;
    DrawRectangle(x + 1, y_atm, ancho - 2, y_mar - y_atm, (Color){ 20, 45, 95, 90 });
    DrawRectangle(x + 1, y_mar, ancho - 2, 35, (Color){ 25, 85, 170, 200 });


    for (int lx = x + 10; lx < x + ancho - 10; lx += 18) {
        DrawLine(lx, y_atm, lx + 10, y_atm, COLOR_SUAVE);
    }
    DrawText("Interfaz 120 km", x + ancho - 125, y_atm - 18, 14, COLOR_SUAVE);
    DrawText("Océano Pacífico", x + ancho - 125, y_mar + 10, 14, COLOR_TEXTO);

   
    Vector2 p_inicio = { (float)(x + 30), (float)(y + 35) };
    Vector2 p_entrada = { (float)(x + 125), (float)y_atm };
    DrawLineEx(p_inicio, p_entrada, 2.5f, COLOR_SUAVE);

   
    DrawTriangle(p_entrada,
                 (Vector2){ (float)(x + 250), (float)y_mar },
                 (Vector2){ (float)(x + 330), (float)y_mar },
                 (Color){ 60, 220, 120, 28 });

    
    Vector2 p_ant = p_entrada;
    int pasos = 24;

    for (int i = 1; i <= pasos; i++) {
        float t = (float)i / (float)pasos;
        Vector2 p_act;

        if (estado == ANGULO_REBOTE) {
            
            float prof = 45.0f * (gamma / (float)GAMMA_MIN);
            p_act.x = p_entrada.x + t * (ancho - 150);
            p_act.y = p_entrada.y + 4.0f * prof * t * (1.0f - t) - t * 40.0f;
        } else {
           
            float factor = (gamma - (float)GAMMA_MIN) / (GAMMA_ESCALA - (float)GAMMA_MIN);
            if (factor < 0.0f) factor = 0.0f;
            if (factor > 1.0f) factor = 1.0f;

            float x_impacto = (estado == ANGULO_SEGURO)
                            ? 290.0f - ((gamma - (float)GAMMA_MIN) / (float)(GAMMA_MAX - GAMMA_MIN)) * 40.0f
                            : 230.0f - factor * 95.0f;

            p_act.x = p_entrada.x + t * (x_impacto - (p_entrada.x - x));
            p_act.y = p_entrada.y + powf(t, 1.15f - factor * 0.35f) * (y_mar - y_atm);
        }

        DrawLineEx(p_ant, p_act, 3.0f, color_curva);
        p_ant = p_act;
    }

   
    DrawCircleV(p_entrada, 5.0f, COLOR_TEXTO);
    DrawCircleV(p_ant, 6.0f, color_curva);
}

static void pantalla_analisis(Juego *j)
{
    const DatosMision *d = &j->datos;

    DrawText("LABORATORIO DE ANÁLISIS FÍSICO", 40, 22, 28, COLOR_TEXTO);

    
    DrawText(TextFormat("Ángulo de reingreso seleccionado: %.2f°", j->gamma_analisis),
             40, 64, 18, COLOR_TEXTO);
    GuiSlider((Rectangle){ 55, 92, 340, 24 }, "0°", "15°",
              &j->gamma_analisis, 0.0f, GAMMA_ESCALA);
    dibujar_ventana_angulo(40, 128, 370, j->gamma_analisis);

    if (boton(40, 190, 370, 44, "CALCULAR SIMULACIÓN")) {
        j->gamma_calculado = j->gamma_analisis;
        j->analisis_calculado = 1;
    }

    dibujar_trayectoria_analisis(40, 248, 370, 225, j->gamma_analisis);

    if (boton(40, 490, 220, 46, "Volver al menú")) {
        j->pantalla = PANTALLA_MENU;
    }

   
    DrawRectangle(435, 64, 425, 472, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(435, 64, 425, 472, COLOR_BARRA);

    if (!j->analisis_calculado) {
        DrawText("Ajustá el ángulo con el deslizador\ny presioná \"CALCULAR SIMULACIÓN\"\npara obtener el reporte analítico.",
                 465, 250, 20, COLOR_SUAVE);
        return;
    }

    
    double ei        = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double w_nominal = calcular_trabajo_rozamiento(d);
    double a_nominal = calcular_desaceleracion_media(d);
    double f_agua    = calcular_fuerza_amerizaje(d);
    EstadoAngulo estado = clasificar_angulo(j->gamma_calculado);

   
    double rad_ref = 6.25 * (3.14159265358979323846 / 180.0);
    double rad_act = ((j->gamma_calculado < 0.5f) ? 0.5 : j->gamma_calculado) * (3.14159265358979323846 / 180.0);
    double factor_angulo = sin(rad_act) / sin(rad_ref);

    
    double dt_efectivo = d->dt_frenado / factor_angulo;
    double a_real      = (d->v_inicial - d->v_paracaidas) / dt_efectivo;

   
    double fraccion_captura = (estado == ANGULO_REBOTE)
                            ? (j->gamma_calculado / GAMMA_MIN) * 0.35
                            : 1.0;
    double w_real = w_nominal * fraccion_captura;

    DrawText(TextFormat("REPORTE ANALÍTICO (gamma = %.2f°)", j->gamma_calculado),
             455, 78, 20, COLOR_AVISO);
    DrawRectangle(455, 104, 385, 2, COLOR_BARRA);

    
    DrawText("1) Trabajo del rozamiento (W = Em,f - Em,i)", 455, 114, 17, COLOR_TEXTO);
    DrawText(TextFormat("Teórico captura total: %.4e J", w_nominal), 455, 136, 15, COLOR_SUAVE);
    if (estado == ANGULO_REBOTE) {
        DrawText(TextFormat("Con %.2f° (rebote): %.4e J (disipa %.1f%%)",
                            j->gamma_calculado, w_real, 100.0 * (-w_real) / ei),
                 455, 156, 15, COLOR_AVISO);
    } else {
        double potencia_mw = (-w_real / dt_efectivo) / 1e6;
        DrawText(TextFormat("Disipa 99.96%% en %.0f s  |  Potencia térmica: %.1f MW",
                            dt_efectivo, potencia_mw),
                 455, 156, 15, COLOR_AVISO);
    }

    
    DrawText("2) Desaceleración en el frenado", 455, 190, 17, COLOR_TEXTO);
    DrawText(TextFormat("Nominal enunciado (%.0f s): %.2f m/s² (%.2f g)",
                        d->dt_frenado, a_nominal, a_nominal / G_TERRESTRE),
             455, 212, 15, COLOR_SUAVE);
    DrawText(TextFormat("Con ángulo %.2f° (dt = %.0f s): %.2f m/s² (%.2f g)",
                        j->gamma_calculado, dt_efectivo, a_real, a_real / G_TERRESTRE),
             455, 232, 16, (estado == ANGULO_EXCESIVO) ? COLOR_ERROR : COLOR_AVISO);


    DrawText("3) Fuerza promedio del agua en el amerizaje", 455, 266, 17, COLOR_TEXTO);
    if (estado == ANGULO_REBOTE) {
        DrawText("No alcanza el océano (la cápsula rebota al espacio).", 455, 290, 15, COLOR_AVISO);
        DrawText(TextFormat("(Si amerizara a %.1f m/s: %.1f N = %.2fx peso)",
                            d->v_impacto, f_agua, f_agua / (d->masa * G_TERRESTRE)),
                 455, 310, 14, COLOR_SUAVE);
    } else if (estado == ANGULO_EXCESIVO) {
        DrawText(TextFormat("F nominal: %.1f N (%.2fx peso) [Riesgo de fallo previo]",
                            f_agua, f_agua / (d->masa * G_TERRESTRE)),
                 455, 290, 15, COLOR_ERROR);
    } else {
        DrawText(TextFormat("F = m(g + v²/2d) = %.1f N  (%.2fx el peso)",
                            f_agua, f_agua / (d->masa * G_TERRESTRE)),
                 455, 290, 16, COLOR_OK);
    }

   
    DrawText(TextFormat("4) Diagnóstico de trayectoria (%.2f°)", j->gamma_calculado),
             455, 344, 17, COLOR_TEXTO);

    switch (estado) {
    case ANGULO_SEGURO:
        DrawText("DENTRO DE LA VENTANA SEGURA (5.5° a 7.0°)", 455, 368, 16, COLOR_OK);
        DrawText("La cápsula es capturada por la atmósfera\ny disipa su energía de forma controlada\nhasta desplegar paracaídas y amerizar.",
                 455, 392, 16, COLOR_SUAVE);
        break;
    case ANGULO_EXCESIVO:
        DrawText("PELIGRO: DESACELERACIÓN EXCESIVA (> 7.0°)", 455, 368, 16, COLOR_ERROR);
        DrawText("Trayectoria muy empinada: el tiempo de\nfrenado cae brusco y las fuerzas g junto\ncon el calor superan el límite tolerable.",
                 455, 392, 16, COLOR_SUAVE);
        break;
    default:
        DrawText("PELIGRO: REBOTE ATMOSFÉRICO (< 5.5°)", 455, 368, 16, COLOR_AVISO);
        DrawText("Trayectoria muy tendida: el aire superior\nno logra frenar la cápsula y esta rebota\nde regreso al espacio exterior.",
                 455, 392, 16, COLOR_SUAVE);
        break;
    }

    DrawRectangle(455, 468, 385, 1, COLOR_BARRA);
    DrawText("Nota: Los valores en gris corresponden al caso ideal",
             455, 476, 14, COLOR_SUAVE);
    DrawText("del enunciado (gamma = 6.25°) y sirven de comparación.",
             455, 494, 14, COLOR_SUAVE);

    if (fabsf(j->gamma_analisis - j->gamma_calculado) > 0.05f) {
        DrawText("* Moviste el slider: tocá CALCULAR para actualizar.",
                 455, 514, 14, COLOR_AVISO);
    }
}


static void dibujar_aproximacion_evento1(int x, int y, int ancho, int alto, const Reporte *r)
{
    const Dificultad *dif = r->dificultad;
    float progreso = (dif->turnos_rcs > 0)
                   ? (float)r->num_turnos / (float)dif->turnos_rcs : 1.0f;

    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

    
    int x_atm = x + ancho - 110;
    DrawRectangle(x_atm, y + 1, 109, alto - 2, (Color){ 25, 65, 135, 120 });
    DrawLine(x_atm, y, x_atm, y + alto, COLOR_AVISO);
    DrawText("INTERFAZ\n120 km", x_atm + 18, y + 14, 15, COLOR_SUAVE);

   
    int y_min_seg = y + (int)((GAMMA_MIN / GAMMA_ESCALA) * alto);
    int y_max_seg = y + (int)((GAMMA_MAX / GAMMA_ESCALA) * alto);
    DrawRectangle(x_atm, y_min_seg, 109, y_max_seg - y_min_seg, (Color){ 60, 220, 120, 85 });
    DrawLine(x + 40, y + alto / 2, x_atm, y_min_seg, (Color){ 60, 220, 120, 90 });
    DrawLine(x + 40, y + alto / 2, x_atm, y_max_seg, (Color){ 60, 220, 120, 90 });
    DrawText("5.5° - 7.0°", x_atm + 18, (y_min_seg + y_max_seg) / 2 - 7, 14, COLOR_OK);

    
    float nav_x = (float)(x + 60) + progreso * (float)(x_atm - x - 75);
    float gamma_clamp = (float)r->gamma;
    if (gamma_clamp < 0.5f) gamma_clamp = 0.5f;
    if (gamma_clamp > 14.5f) gamma_clamp = 14.5f;
    float nav_y = (float)y + (gamma_clamp / GAMMA_ESCALA) * (float)alto;

  
    if (r->eventos[0] == EV_PENDIENTE) {
        float prox_x = (float)(x + 60) + ((float)(r->num_turnos + 1) / (float)dif->turnos_rcs) * (float)(x_atm - x - 75);
        float prox_gamma = (float)(r->gamma + r->omega + r->deriva_proxima);
        if (prox_gamma < 0.5f) prox_gamma = 0.5f;
        if (prox_gamma > 14.5f) prox_gamma = 14.5f;
        float prox_y = (float)y + (prox_gamma / GAMMA_ESCALA) * (float)alto;

        DrawLineEx((Vector2){ nav_x, nav_y }, (Vector2){ prox_x, prox_y }, 2.0f, COLOR_AVISO);
        DrawCircle((int)prox_x, (int)prox_y, 4, COLOR_AVISO);
    }

  
    EstadoAngulo est = clasificar_angulo(r->gamma);
    Color c_nave = (est == ANGULO_SEGURO) ? COLOR_OK
                 : (est == ANGULO_REBOTE) ? COLOR_AVISO : COLOR_ERROR;

    DrawTriangle((Vector2){ nav_x - 14, nav_y - 10 },
                 (Vector2){ nav_x - 14, nav_y + 10 },
                 (Vector2){ nav_x + 12, nav_y },
                 c_nave);
    DrawLineEx((Vector2){ nav_x + 12, nav_y - 8 },
               (Vector2){ nav_x + 12, nav_y + 8 }, 3.5f, COLOR_TEXTO);
}
static void pantalla_evento1(Juego *j)
{
    Reporte *r = &j->reporte;
    const Dificultad *dif = j->dificultad;
    int rcs_restante = dif->combustible_rcs - r->rcs_usado;
    int turnos_rest  = dif->turnos_rcs - r->num_turnos;

   
    DrawText("EVENTO 1: ALINEACIÓN CON RCS", 40, 20, 26, COLOR_TEXTO);
    DrawText(TextFormat("Ángulo: %.2f°   |   Giro: %+.2f°/t   |   Radar: %+.2f°/t   |   Turnos: %d   |   RCS: %d/%d",
                        r->gamma, r->omega, r->deriva_proxima,
                        turnos_rest, rcs_restante, dif->combustible_rcs),
             40, 54, 18, COLOR_SUAVE);

   
    dibujar_aproximacion_evento1(40, 84, 820, 205, r);

    
    if (r->eventos[0] == EV_PENDIENTE) {
      
        if (rcs_restante < COSTO_SUAVE) GuiDisable();
        if (boton_doble(40, 304, 228, 48, "Suave Prógrado", "-0.4°/t  |  1 RCS")) {
            aplicar_turno_rcs(r, MANIOBRA_SUAVE, -1);
            j->gamma = (float)r->gamma;
        }
        if (boton_doble(282, 304, 228, 48, "Suave Retrógrado", "+0.4°/t  |  1 RCS")) {
            aplicar_turno_rcs(r, MANIOBRA_SUAVE, +1);
            j->gamma = (float)r->gamma;
        }
        GuiEnable();

        
        if (rcs_restante < COSTO_FUERTE || r->rcs_recalentado) GuiDisable();
        if (boton_doble(40, 362, 228, 48, "Fuerte Prógrado",
                        r->rcs_recalentado ? "EN ENFRIAMIENTO" : "-1.0°/t  |  2 RCS")) {
            aplicar_turno_rcs(r, MANIOBRA_FUERTE, -1);
            j->gamma = (float)r->gamma;
        }
        if (boton_doble(282, 362, 228, 48, "Fuerte Retrógrado",
                        r->rcs_recalentado ? "EN ENFRIAMIENTO" : "+1.0°/t  |  2 RCS")) {
            aplicar_turno_rcs(r, MANIOBRA_FUERTE, +1);
            j->gamma = (float)r->gamma;
        }
        GuiEnable();

       
        if (boton_doble(40, 420, 470, 48, "Mantener actitud", "0.0°/t  |  0 RCS (enfría toberas)")) {
            aplicar_turno_rcs(r, MANIOBRA_MANTENER, 0);
            j->gamma = (float)r->gamma;
        }
    } else {
       
        EstadoAngulo est_final = clasificar_angulo(r->gamma);
        int giro_ok = (fabs(r->omega) <= OMEGA_MAX_SEGURO);

        DrawRectangle(40, 304, 470, 164, (Color){ 16, 24, 48, 255 });
        DrawRectangleLines(40, 304, 470, 164, COLOR_BARRA);

        if (r->eventos[0] == EV_SUPERADO) {
            DrawText("CORREDOR CAPTURADO - EVENTO SUPERADO", 58, 326, 18, COLOR_OK);
            DrawText(TextFormat("Ingreso exitoso a %.2f° (giro %+.2f°/t).\nLa cápsula inicia el frenado aerodinámico.",
                                r->gamma, r->omega), 58, 362, 17, COLOR_SUAVE);
        } else if (!giro_ok && est_final == ANGULO_SEGURO) {
            DrawText("FALLO DE ACTITUD: GIRO INESTABLE", 58, 326, 18, COLOR_ERROR);
            DrawText(TextFormat("Ángulo en zona (%.2f°), pero giro excesivo\n(omega = %+.2f°/t, máx ±%.2f°/t).",
                                r->gamma, r->omega, OMEGA_MAX_SEGURO), 58, 362, 17, COLOR_SUAVE);
        } else if (est_final == ANGULO_REBOTE) {
            DrawText("REBOTE ATMOSFÉRICO - MISIÓN PERDIDA", 58, 326, 18, COLOR_AVISO);
            DrawText("Ángulo menor a 5.5°: la nave rebota al espacio.", 58, 362, 17, COLOR_SUAVE);
        } else {
            DrawText("DESACELERACIÓN EXCESIVA - MISIÓN PERDIDA", 58, 326, 18, COLOR_ERROR);
            DrawText("Ángulo mayor a 7.0°: supera el límite estructural.", 58, 362, 17, COLOR_SUAVE);
        }
    }

    
    DrawRectangle(535, 304, 325, 164, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(535, 304, 325, 164, COLOR_BARRA);
    DrawText("BITÁCORA RCS", 550, 312, 16, COLOR_AVISO);
    DrawText("T     Pulso   Radar   Giro   Gamma  RCS", 550, 334, 13, COLOR_TEXTO);
    DrawRectangle(550, 350, 295, 1, COLOR_BARRA);

    DrawText(TextFormat("Ini     -       -    %+5.2f  %5.2f°  %2d",
                        (r->gamma_inicial < GAMMA_MIN) ? -0.20 : +0.20,
                        r->gamma_inicial, dif->combustible_rcs),
             550, 356, 13, COLOR_SUAVE);

    for (int i = 0; i < r->num_turnos; i++) {
        const TurnoRCS *tr = &r->bitacora[i];
        int fila_y = 376 + i * 21;
        DrawText(TextFormat("T-%d  %+5.2f  %+5.2f  %+5.2f  %5.2f°  %2d",
                            tr->t, tr->delta_pulso, tr->deriva,
                            tr->omega_resultante, tr->gamma_resultante, tr->rcs_restante),
                 550, fila_y, 13, COLOR_TEXTO);
    }

    
    if (r->eventos[0] == EV_SUPERADO) {
        if (boton(40, 488, 300, 46, "Continuar al frenado")) pasar_a_frenado(j);
    } else if (r->eventos[0] == EV_FALLIDO) {
        if (boton(40, 488, 300, 46, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 488, 240, 46, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
}

static void dibujar_visor_frenado(int x, int y, int ancho, int alto,
                                  const DatosMision *d, const Dificultad *dif,
                                  const Reporte *r)
{
    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

   
    int gx = x + 48;
    int gy = y + 26;
    int gancho = 410;
    int galto  = alto - 54;

    DrawRectangle(gx, gy, gancho, galto, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(gx, gy, gancho, galto, COLOR_BARRA);

  
    for (int i = 1; i < FASES_FRENADO; i++) {
        int lx = gx + (gancho * i) / FASES_FRENADO;
        DrawLine(lx, gy, lx, gy + galto, (Color){ 35, 45, 75, 255 });
    }

    
    for (int i = 1; i <= r->fase_frenado; i++) {
        Vector2 p1 = {
            gx + (float)(r->tabla[i - 1].tiempo / d->dt_frenado) * gancho,
            gy + galto - (float)(r->tabla[i - 1].velocidad / d->v_inicial) * galto
        };
        Vector2 p2 = {
            gx + (float)(r->tabla[i].tiempo / d->dt_frenado) * gancho,
            gy + galto - (float)(r->tabla[i].velocidad / d->v_inicial) * galto
        };
        DrawLineEx(p1, p2, 3.0f, COLOR_AVISO);
    }
    for (int i = 0; i <= r->fase_frenado; i++) {
        int px = gx + (int)((r->tabla[i].tiempo / d->dt_frenado) * gancho);
        int py = gy + galto - (int)((r->tabla[i].velocidad / d->v_inicial) * galto);
        DrawCircle(px, py, 5, (i == r->fase_frenado) ? COLOR_OK : COLOR_TEXTO);
    }

    DrawText(TextFormat("%.0f", d->v_inicial), x + 6, gy - 4, 13, COLOR_SUAVE);
    DrawText("m/s", x + 10, gy + 12, 12, COLOR_SUAVE);
    DrawText("0 s", gx, gy + galto + 6, 13, COLOR_SUAVE);
    DrawText(TextFormat("%.0f s (Meta <= %.0f m/s)", d->dt_frenado, d->v_paracaidas + MARGEN_VEL_PARAC),
             gx + gancho - 175, gy + galto + 6, 13, COLOR_OK);

    
    double ult_g = (r->fase_frenado > 0) ? r->tabla[r->fase_frenado].g_fase : 0.0;
    dibujar_barra(x + 490, y + 22, 305, "Última carga G (Tripulación)",
                  ult_g, dif->limite_g_frenado, "g");
    dibujar_barra(x + 490, y + 112, 305, "Temperatura del Escudo Térmico",
                  r->calor_escudo, CALOR_MAXIMO, "%");
}

static void pantalla_evento2(Juego *j)
{
    Reporte *r = &j->reporte;
    const DatosMision *d = &j->datos;
    const Dificultad *dif = j->dificultad;
    double v_actual = r->tabla[r->fase_frenado].velocidad;
    double g_nom_est = (calcular_desaceleracion_media(d) * r->densidad_proxima) / G_TERRESTRE;

  
    DrawText("EVENTO 2: FRENADO AERODINÁMICO", 40, 20, 26, COLOR_TEXTO);
    DrawText(TextFormat("Fase: %d/%d   |   Vel: %.0f m/s   |   Escudo: %.0f%%   |   Densidad capa: %.0f%%   |   Pico: %.2f/%.1f g",
                        r->fase_frenado, FASES_FRENADO, v_actual, r->calor_escudo,
                        r->densidad_proxima * 100.0, r->desaceleracion_g, dif->limite_g_frenado),
             40, 54, 17, COLOR_SUAVE);

   
    dibujar_visor_frenado(40, 84, 820, 205, d, dif, r);

    
    if (r->eventos[1] == EV_PENDIENTE) {
        if (boton_doble(40, 304, 470, 48, "1. Sustentación Alta (Subir a aire tenue)",
                        TextFormat("Frena 0.78x  |  ~%.2f g  |  Enfría escudo (-15%%)",
                                   g_nom_est * FACTOR_SUST_ALTA))) {
            aplicar_turno_frenado(d, dif, r, PERFIL_SUST_ALTA);
        }

        if (boton_doble(40, 362, 470, 48, "2. Perfil Nominal (Trayectoria equilibrada)",
                        TextFormat("Frena 1.00x  |  ~%.2f g  |  Calor moderado (+18%%)",
                                   g_nom_est * FACTOR_NOMINAL))) {
            aplicar_turno_frenado(d, dif, r, PERFIL_NOMINAL);
        }

        if (boton_doble(40, 420, 470, 48, "3. Inmersión Densa (Bajar a aire denso)",
                        TextFormat("Frena 1.25x  |  ~%.2f g  |  Calor intenso (+38%%)",
                                   g_nom_est * FACTOR_INM_DENSA))) {
            aplicar_turno_frenado(d, dif, r, PERFIL_INM_DENSA);
        }
    } else {
        
        DrawRectangle(40, 304, 470, 164, (Color){ 16, 24, 48, 255 });
        DrawRectangleLines(40, 304, 470, 164, COLOR_BARRA);

        if (r->eventos[1] == EV_SUPERADO) {
            DrawText("FRENADO EXITOSO - EVENTO SUPERADO", 58, 322, 18, COLOR_OK);
            DrawText(TextFormat("Velocidad reducida a %.1f m/s (Pico: %.2f g).\nTrabajo del aire: %.3e J.\nListo para desplegar paracaídas.",
                                v_actual, r->desaceleracion_g, r->trabajo_roz),
                     58, 354, 16, COLOR_SUAVE);
        } else if (r->motivo_fallo_frenado == 1) {
            DrawText("EXCESO DE FUERZAS G - MISIÓN PERDIDA", 58, 322, 18, COLOR_ERROR);
            DrawText(TextFormat("La desaceleración alcanzó %.2f g, superando\nel límite de %.1f g que soporta la tripulación.",
                                r->desaceleracion_g, dif->limite_g_frenado),
                     58, 358, 16, COLOR_SUAVE);
        } else if (r->motivo_fallo_frenado == 2) {
            DrawText("COLAPSO DEL ESCUDO TÉRMICO - MISIÓN PERDIDA", 58, 322, 18, COLOR_ERROR);
            DrawText(TextFormat("La carga térmica llegó al %.0f%% (máx 100%%)\npor permanecer demasiado tiempo en aire denso.",
                                r->calor_escudo),
                     58, 358, 16, COLOR_SUAVE);
        } else {
            DrawText("VELOCIDAD EXCESIVA - MISIÓN PERDIDA", 58, 322, 18, COLOR_AVISO);
            DrawText(TextFormat("Terminaste a %.1f m/s (máx seguro %.0f m/s).\nAbrir los paracaídas a esa velocidad los desgarra.",
                                v_actual, d->v_paracaidas + MARGEN_VEL_PARAC),
                     58, 358, 16, COLOR_SUAVE);
        }
    }

    
    DrawRectangle(535, 304, 325, 164, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(535, 304, 325, 164, COLOR_BARRA);
    DrawText("BITÁCORA DE FRENADO", 550, 312, 16, COLOR_AVISO);
    DrawText("Fase   t[s]   v[m/s]    Pico G  Escudo", 550, 334, 13, COLOR_TEXTO);
    DrawRectangle(550, 350, 295, 1, COLOR_BARRA);

    DrawText(TextFormat("Ini      0    %6.0f      -      %3.0f%%",
                        r->tabla[0].velocidad, r->tabla[0].calor_escudo),
             550, 356, 13, COLOR_SUAVE);

    for (int i = 1; i <= r->fase_frenado; i++) {
        const PasoFrenado *pf = &r->tabla[i];
        int fila_y = 356 + i * 21;
        DrawText(TextFormat("F-%d   %4.0f    %6.0f    %4.2fg    %3.0f%%",
                            i, pf->tiempo, pf->velocidad, pf->g_fase, pf->calor_escudo),
                 550, fila_y, 13, COLOR_TEXTO);
    }

    
    if (r->eventos[1] == EV_SUPERADO) {
        if (boton(40, 488, 300, 46, "Continuar al amerizaje")) pasar_a_amerizaje(j);
    } else if (r->eventos[1] == EV_FALLIDO) {
        if (boton(40, 488, 300, 46, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 488, 240, 46, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
}


static void dibujar_visor_amerizaje(int x, int y, int ancho, int alto,
                                    const Dificultad *dif, const Reporte *r)
{
    float t = (float)GetTime();

    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

   
    int ex = x + 20;
    int ey = y + 16;
    int eancho = 430;
    int ealto  = alto - 32;
    int y_agua = ey + ealto - 46;

    DrawRectangle(ex, ey, eancho, ealto, (Color){ 14, 26, 54, 255 });
    DrawRectangle(ex, y_agua, eancho, 46, (Color){ 20, 80, 165, 220 });
    DrawLine(ex, y_agua, ex + eancho, y_agua, (Color){ 100, 190, 255, 255 });
    DrawRectangleLines(ex, ey, eancho, ealto, COLOR_BARRA);

   
    int cx = ex + eancho / 2;
    int cy = (r->etapa_amerizaje == 0) ? (ey + 55)
           : (r->etapa_amerizaje == 1) ? (ey + 95)
                                       : (y_agua + 6);
    cy += (int)(sinf(t * 3.0f) * 3.0f);

    
    if (r->motivo_fallo_amerizaje != 1) {
        DrawLine(cx - 24, cy - 42, cx - 6, cy - 8, COLOR_SUAVE);
        DrawLine(cx + 24, cy - 42, cx + 6, cy - 8, COLOR_SUAVE);
        DrawLine(cx,      cy - 46, cx,     cy - 8, COLOR_SUAVE);
        DrawCircleSector((Vector2){ (float)cx, (float)(cy - 42) },
                         28.0f, 180.0f, 360.0f, 16, (Color){ 240, 110, 50, 255 });
    }

   
    Color c_cap = (r->eventos[2] == EV_FALLIDO)  ? COLOR_ERROR
                : (r->eventos[2] == EV_SUPERADO) ? COLOR_OK : COLOR_TEXTO;
    DrawTriangle((Vector2){ (float)cx, (float)(cy - 12) },
                 (Vector2){ (float)(cx - 16), (float)(cy + 10) },
                 (Vector2){ (float)(cx + 16), (float)(cy + 10) },
                 c_cap);

    DrawText(TextFormat("Ráfagas / Oleaje: %.0f%%", r->factor_viento * 100.0),
             ex + 12, ey + 10, 15, (r->factor_viento > 1.08) ? COLOR_AVISO : COLOR_OK);
    DrawText("Océano Pacífico", ex + 12, y_agua + 16, 15, COLOR_TEXTO);

    
    dibujar_barra(x + 485, y + 28, 310, "Fuerza del impacto en el agua",
                  r->fuerza_en_pesos, dif->limite_g_impacto, "x peso");

    DrawText("Fórmula: F = m (g + v² / 2d)", x + 485, y + 118, 16, COLOR_TEXTO);
    DrawText(TextFormat("Velocidad impacto (v): %.2f m/s", r->v_impacto_real),
             x + 485, y + 144, 15, COLOR_AVISO);
    DrawText(TextFormat("Hundimiento (d): %.2f m", r->d_hundimiento_real),
             x + 485, y + 168, 15, COLOR_SUAVE);
}

static void pantalla_evento3(Juego *j)
{
    Reporte *r = &j->reporte;
    const DatosMision *d = &j->datos;
    const Dificultad *dif = j->dificultad;

    
    DrawText("EVENTO 3: PARACAÍDAS Y AMERIZAJE", 40, 20, 26, COLOR_TEXTO);
    DrawText(TextFormat("Decisión: %d/2   |   Meteorología: %.0f%%   |   Vel. impacto: %.2f m/s   |   Hundimiento: %.2f m",
                        (r->etapa_amerizaje < 2) ? r->etapa_amerizaje + 1 : 2,
                        r->factor_viento * 100.0, r->v_impacto_real, r->d_hundimiento_real),
             40, 54, 17, COLOR_SUAVE);

   
    dibujar_visor_amerizaje(40, 84, 820, 205, dif, r);

   
    if (r->etapa_amerizaje == 0) {
       
        if (boton_doble(40, 304, 470, 48, "1. Apertura Temprana (Alta cota ~4000 m)",
                        "Frena mejor (0.92x vel)  |  Riesgo de rotura si ráfagas > 108%")) {
            elegir_apertura_paracaidas(d, r, 1);
        }
        if (boton_doble(40, 362, 470, 48, "2. Apertura Nominal (Cota estándar 2500 m)",
                        TextFormat("Despliegue oficial  |  Velocidad objetivo ~%.1f m/s", d->v_impacto))) {
            elegir_apertura_paracaidas(d, r, 2);
        }
        if (boton_doble(40, 420, 470, 48, "3. Apertura Tardía (Baja cota ~1200 m)",
                        "Resiste cualquier ráfaga  |  Golpea el agua 1.45x más rápido")) {
            elegir_apertura_paracaidas(d, r, 3);
        }
    } else if (r->etapa_amerizaje == 1) {
       
        if (boton_doble(40, 304, 470, 48, "1. Entrada en Cuña (28° - Amortiguada)",
                        TextFormat("Aumenta hundimiento (d = %.2f m)  |  Baja fuerza (salvo mar > 112%%)",
                                   d->d_hundimiento * FACTOR_HUND_CUNA))) {
            elegir_actitud_impacto(d, dif, r, 1);
        }
        if (boton_doble(40, 362, 470, 48, "2. Entrada Nominal (18° - Estándar)",
                        TextFormat("Hundimiento oficial (d = %.2f m)  |  Amortiguación equilibrada",
                                   d->d_hundimiento))) {
            elegir_actitud_impacto(d, dif, r, 2);
        }
        if (boton_doble(40, 420, 470, 48, "3. Entrada Plana (0° - Bolsas de flotación)",
                        TextFormat("Frena en seco (d = %.2f m)  |  Sube la fuerza sobre el casco",
                                   d->d_hundimiento * FACTOR_HUND_PLANA))) {
            elegir_actitud_impacto(d, dif, r, 3);
        }
    } else {

        DrawRectangle(40, 304, 470, 164, (Color){ 16, 24, 48, 255 });
        DrawRectangleLines(40, 304, 470, 164, COLOR_BARRA);

        if (r->eventos[2] == EV_SUPERADO) {
            DrawText("AMERIZAJE EXITOSO - MISIÓN COMPLETADA", 58, 322, 18, COLOR_OK);
            DrawText(TextFormat("Impacto a %.2f m/s con %.2f m de hundimiento.\nFuerza: %.0f N (%.2fx el peso, máx %.1fx).\nLa cápsula flota estable en el Pacífico.",
                                r->v_impacto_real, r->d_hundimiento_real,
                                r->fuerza_impacto, r->fuerza_en_pesos, dif->limite_g_impacto),
                     58, 354, 16, COLOR_SUAVE);
        } else if (r->motivo_fallo_amerizaje == 1) {
            DrawText("PARACAÍDAS DESGARRADOS - MISIÓN PERDIDA", 58, 322, 18, COLOR_ERROR);
            DrawText("Abriste los paracaídas temprano en medio de\nráfagas intensas o con exceso de velocidad,\ncortando las líneas de suspensión.",
                     58, 354, 16, COLOR_SUAVE);
        } else {
            DrawText("FALLO ESTRUCTURAL EN EL AGUA - MISIÓN PERDIDA", 58, 322, 18, COLOR_ERROR);
            DrawText(TextFormat("La fuerza del agua alcanzó %.0f N (%.2fx el peso),\nsuperando el límite de %.1fx que resiste el casco.",
                                r->fuerza_impacto, r->fuerza_en_pesos, dif->limite_g_impacto),
                     58, 354, 16, COLOR_SUAVE);
        }
    }


    DrawRectangle(535, 304, 325, 164, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(535, 304, 325, 164, COLOR_BARRA);
    DrawText("BITÁCORA DE AMERIZAJE", 550, 312, 16, COLOR_AVISO);
    DrawRectangle(550, 334, 295, 1, COLOR_BARRA);

    DrawText(TextFormat("Meteorología : %.0f%%", r->factor_viento * 100.0), 550, 344, 14, COLOR_SUAVE);
    DrawText(TextFormat("Paracaídas   : %s", nombre_opcion_paracaidas(r->opcion_paracaidas)), 550, 366, 14, COLOR_TEXTO);
    DrawText(TextFormat("Actitud agua : %s", nombre_opcion_actitud(r->opcion_actitud_agua)), 550, 388, 14, COLOR_TEXTO);
    DrawText(TextFormat("Vel / Hund.  : %.2f m/s  |  %.2f m", r->v_impacto_real, r->d_hundimiento_real), 550, 412, 14, COLOR_AVISO);
    if (r->etapa_amerizaje == 2 && r->motivo_fallo_amerizaje != 1) {
        DrawText(TextFormat("Fuerza agua  : %.0f N (%.2fx)", r->fuerza_impacto, r->fuerza_en_pesos),
                 550, 436, 14, (r->eventos[2] == EV_SUPERADO) ? COLOR_OK : COLOR_ERROR);
    }


    if (r->etapa_amerizaje == 2) {
        if (boton(40, 488, 300, 46, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 488, 240, 46, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_reporte(Juego *j)
{
    const Reporte *r = &j->reporte;
    int y = 90;

    DrawText("REPORTE FINAL DE LA MISIÓN", 40, 30, 28, COLOR_TEXTO);

   DrawText(TextFormat("Dificultad: %s", r->dificultad->nombre), 40, y, 20, COLOR_TEXTO);
    y += 32;
    DrawText(TextFormat("Ángulo inicial: %.2f°  ->  Final (t=0): %.2f°   |   RCS usado: %d de %d u.",
                        r->gamma_inicial, r->gamma, r->rcs_usado, r->dificultad->combustible_rcs),
             40, y, 20, COLOR_TEXTO);
    y += 44;

    for (int i = 0; i < NUM_EVENTOS; i++) {
        Color c = (r->eventos[i] == EV_SUPERADO) ? COLOR_OK
                : (r->eventos[i] == EV_FALLIDO)  ? COLOR_ERROR
                                                 : COLOR_SUAVE;
        DrawText(TextFormat("Evento %d - %s: %s", i + 1, nombre_evento(i),
                            nombre_estado_evento(r->eventos[i])),
                 40, y, 20, c);
        y += 30;
    }
    y += 14;

    if (r->eventos[1] != EV_PENDIENTE) {
        DrawText(TextFormat("Trabajo del rozamiento: %.4e J   |   Desaceleración: %.2f g",
                            r->trabajo_roz, r->desaceleracion_g),
                 40, y, 20, COLOR_SUAVE);
        y += 30;
    }
    if (r->eventos[2] != EV_PENDIENTE) {
        DrawText(TextFormat("Fuerza en el amerizaje: %.1f N (%.2fx el peso)",
                            r->fuerza_impacto, r->fuerza_en_pesos),
                 40, y, 20, COLOR_SUAVE);
        y += 30;
    }

    DrawText(TextFormat("PUNTAJE: %d / %d", r->puntaje, NUM_EVENTOS * PUNTOS_EVENTO + BONUS_PRECISION),
             40, 390, 30, COLOR_AVISO);
    DrawText(r->exito ? "MISIÓN EXITOSA: la tripulación está a salvo." : "MISIÓN FALLIDA.",
             40, 432, 26, r->exito ? COLOR_OK : COLOR_ERROR);

    if (boton(40, 490, 240, 50, "Jugar de nuevo")) j->pantalla = PANTALLA_DIFICULTAD;
    if (boton(300, 490, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}



void juego_iniciar(Juego *j)
{
    memset(j, 0, sizeof *j);
    j->pantalla = PANTALLA_MENU;
    cargar_datos_por_defecto(&j->datos);
    j->dificultad = obtener_dificultad(1);
    j->gamma = 6.0f;
    j->gamma_analisis = 6.0f;
    j->campo_activo = -1;
    datos_a_campos(&j->datos, j->campos);
}

static void aplicar_estilo(void)
{
    const Color borde      = {  90, 106, 160, 255 };
    const Color base       = {  36,  48,  90, 255 };
    const Color enfocado   = {  51,  66, 127, 255 };
    const Color presionado = {  79,  99, 179, 255 };
    const Color borde_enf  = { 143, 163, 255, 255 };
    const Color apagado    = {  26,  34,  64, 255 };
    const Color texto_apag = { 111, 122, 168, 255 };

    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,      ColorToInt(COLOR_FONDO));
    GuiSetStyle(DEFAULT, LINE_COLOR,            ColorToInt(borde));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,   ColorToInt(borde));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,     ColorToInt(base));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,     ColorToInt(COLOR_TEXTO));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,  ColorToInt(borde_enf));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,    ColorToInt(enfocado));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,    ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,  ColorToInt(borde_enf));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,    ColorToInt(presionado));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,    ColorToInt(WHITE));
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, ColorToInt(apagado));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,   ColorToInt(apagado));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,   ColorToInt(texto_apag));
    GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
}

void iniciar_interfaz(void)
{
    InitWindow(ANCHO_VENTANA, ALTO_VENTANA, "Artemis - Reingreso y Amerizaje");
    SetExitKey(KEY_NULL);        
    SetTargetFPS(60);
    aplicar_estilo();
}

int interfaz_activa(const Juego *j)
{
    return !j->salir && !WindowShouldClose();
}

void interfaz_dibujar(Juego *j)
{
    BeginDrawing();
    ClearBackground(COLOR_FONDO);

    switch (j->pantalla) {
    case PANTALLA_MENU:       pantalla_menu(j);       break;
    case PANTALLA_CONFIG:     pantalla_config(j);     break;
    case PANTALLA_ANALISIS:   pantalla_analisis(j);   break;
    case PANTALLA_REGLAS:     pantalla_reglas(j);     break;
    case PANTALLA_DIFICULTAD: pantalla_dificultad(j); break;
    case PANTALLA_EVENTO1:    pantalla_evento1(j);    break;
    case PANTALLA_EVENTO2:    pantalla_evento2(j);    break;
    case PANTALLA_EVENTO3:    pantalla_evento3(j);    break;
    case PANTALLA_REPORTE:    pantalla_reporte(j);    break;
    }

    EndDrawing();
}

void cerrar_interfaz(void)
{
    CloseWindow();
}
