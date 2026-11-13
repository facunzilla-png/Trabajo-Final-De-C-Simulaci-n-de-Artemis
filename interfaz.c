
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

/* Barra de progreso con color según el límite (verde / amarillo / rojo). */
static void dibujar_barra(int x, int y, int ancho, const char *titulo,
                          double valor, double limite, const char *unidad)
{
    double escala = limite * 1.5;              /* el límite queda a 2/3 de la barra */
    int lleno = (int)(valor / escala * ancho);
    Color color = (valor <= 0.8 * limite) ? COLOR_OK
                : (valor <= limite)       ? COLOR_AVISO
                                          : COLOR_ERROR;

    if (lleno > ancho) lleno = ancho;
    if (lleno < 0)     lleno = 0;

    DrawText(titulo, x, y, 18, COLOR_TEXTO);
    DrawRectangle(x, y + 24, ancho, 22, COLOR_BARRA);
    DrawRectangle(x, y + 24, lleno, 22, color);
    DrawRectangle(x + (int)(ancho / 1.5), y + 20, 2, 30, COLOR_TEXTO);   /* marca del límite */
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

static void dibujar_grafico_velocidad(int x, int y, int ancho, int alto,
                                      const PasoFrenado tabla[], int n,
                                      double v_max, double t_max)
{
    DrawRectangle(x, y, ancho, alto, COLOR_BARRA);
    DrawRectangleLines(x, y, ancho, alto, COLOR_SUAVE);

    for (int i = 1; i < n; i++) {
        Vector2 a = { x + (float)(tabla[i - 1].tiempo / t_max * ancho),
                      y + alto - (float)(tabla[i - 1].velocidad / v_max * alto) };
        Vector2 b = { x + (float)(tabla[i].tiempo / t_max * ancho),
                      y + alto - (float)(tabla[i].velocidad / v_max * alto) };
        DrawLineEx(a, b, 3.0f, COLOR_AVISO);
    }
    for (int i = 0; i < n; i++) {
        DrawCircle(x + (int)(tabla[i].tiempo / t_max * ancho),
                   y + alto - (int)(tabla[i].velocidad / v_max * alto), 4, COLOR_TEXTO);
    }

    DrawText(TextFormat("%.0f m/s", v_max), x, y - 22, 16, COLOR_SUAVE);
    DrawText("0 s", x, y + alto + 6, 16, COLOR_SUAVE);
    DrawText(TextFormat("%.0f s", t_max), x + ancho - 50, y + alto + 6, 16, COLOR_SUAVE);
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
    resolver_frenado(&j->datos, j->dificultad, &j->reporte);   /* se calcula UNA vez */
    j->pantalla = PANTALLA_EVENTO2;
}

static void pasar_a_amerizaje(Juego *j)
{
    resolver_amerizaje(&j->datos, j->dificultad, &j->reporte);
    j->pantalla = PANTALLA_EVENTO3;
}

static void terminar_mision(Juego *j)
{
    calcular_puntaje(&j->reporte);
    j->hay_reporte = 1;
    j->misiones_jugadas++;
    j->pantalla = PANTALLA_REPORTE;
}

/* =====================================================================
 *                              PANTALLAS
 * ===================================================================== */

/* Dibuja una ilustración vectorial animada de la cápsula Orion en reingreso */
static void dibujar_escena_orion(int x, int y, int ancho, int alto, const Juego *j)
{
    float t = (float)GetTime();
    float dy = sinf(t * 2.5f) * 6.0f;          /* flotación vertical suave */
    float pulso = sinf(t * 12.0f) * 3.0f;      /* vibración del plasma     */

    int cx = x + ancho / 2 + 10;
    int cy = y + alto / 2 - 15 + (int)dy;


    /* Estrellas de fondo */
    DrawCircle(x + 45,  y + 40,  1.5f, COLOR_TEXTO);
    DrawCircle(x + 120, y + 85,  1.0f, COLOR_SUAVE);
    DrawCircle(x + 340, y + 50,  2.0f, COLOR_TEXTO);
    DrawCircle(x + 380, y + 140, 1.5f, COLOR_SUAVE);
    DrawCircle(x + 70,  y + 200, 1.0f, COLOR_TEXTO);
    DrawCircle(x + 310, y + 260, 1.5f, COLOR_SUAVE);

    /* Curvatura de la atmósfera terrestre (abajo) */
    DrawCircle(x + ancho / 2, y + alto + 260, 340, (Color){ 20, 50, 110, 140 });
    DrawCircle(x + ancho / 2, y + alto + 260, 315, (Color){ 35, 95, 190, 180 });
    DrawCircle(x + ancho / 2, y + alto + 260, 295, (Color){ 18, 42, 92, 255 });

    /* Estela de plasma (hacia arriba a la izquierda) */
    Vector2 p_origen = { (float)cx, (float)cy };
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 115), (float)(cy - 85 + pulso) }, 26.0f, (Color){ 240, 80, 40, 75 });
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 85),  (float)(cy - 62 - pulso) }, 16.0f, (Color){ 255, 160, 40, 130 });
    DrawLineEx(p_origen, (Vector2){ (float)(cx - 50),  (float)(cy - 38) },         8.0f,  (Color){ 255, 230, 120, 200 });

    /* Onda de choque / Arco incandescente del escudo térmico */
    DrawCircle(cx + 18, cy + 16, 44 + pulso * 0.4f, (Color){ 255, 100, 30, 90 });
    DrawCircle(cx + 14, cy + 12, 34, (Color){ 255, 190, 60, 160 });

    /* Cuerpo cónico de la cápsula Orion (en sentido antihorario para raylib) */
    Vector2 v_punta = { (float)(cx - 28), (float)(cy - 22) };
    Vector2 v_abajo = { (float)(cx - 4),  (float)(cy + 36) };
    Vector2 v_der   = { (float)(cx + 38), (float)(cy - 8)  };
    DrawTriangle(v_punta, v_abajo, v_der, (Color){ 210, 220, 240, 255 });

    /* Sombra lateral del cono para darle volumen 3D */
    Vector2 v_medio = { (float)(cx + 17), (float)(cy + 14) };
    DrawTriangle(v_punta, v_abajo, v_medio, (Color){ 150, 165, 195, 255 });

    /* Escudo térmico frontal apoyado contra el plasma */
    DrawLineEx((Vector2){ (float)(cx - 4), (float)(cy + 36) },
               (Vector2){ (float)(cx + 38), (float)(cy - 8) },
               7.0f, (Color){ 255, 120, 40, 255 });

    /* Ventanilla de la tripulación y detalles estructurales */
    DrawCircle(cx - 2, cy - 2, 6, COLOR_FONDO);
    DrawCircle(cx - 3, cy - 3, 4, (Color){ 100, 200, 255, 255 });
    DrawLineEx((Vector2){ (float)(cx - 28), (float)(cy - 22) },
               (Vector2){ (float)(cx - 16), (float)(cy - 10) },
               4.0f, COLOR_BARRA);

    /* Pie de telemetría decorativo en el panel */
    DrawText(" CÁPSULA ORION // FASE DE ENTRADA ATMOSFÉRICA", x + 12, y + 12, 14, COLOR_SUAVE);
}

static void pantalla_menu(Juego *j)
{
    /* Encabezado principal */
    DrawText("ARTEMIS", 50, 36, 56, COLOR_TEXTO);
    DrawRectangle(50, 96, 350, 3, COLOR_AVISO);
    DrawText("Simulador de Reingreso y Amerizaje", 50, 108, 19, COLOR_SUAVE);

    /* Tarjeta de estado actual */
    DrawRectangle(50, 142, 350, 36, COLOR_BARRA);
    DrawText(TextFormat("Datos: %s  |  Misiones: %d",
                        j->datos_personalizados ? "Personalizados" : "Oficiales",
                        j->misiones_jugadas),
             62, 151, 17, COLOR_AVISO);

    /* Botón principal destacado (ancho completo de la columna izquierda) */
    if (boton(50, 195, 350, 58, "JUGAR MISIÓN")) {
        j->pantalla = PANTALLA_DIFICULTAD;
    }

    /* Grilla 2x2 para opciones secundarias */
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

    /* Botón de salida separado abajo */
    if (boton(50, 464, 350, 48, "Salir del simulador")) {
        j->salir = 1;
    }

    /* Panel derecho con la ilustración animada de la cápsula */
    dibujar_escena_orion(435, 36, 420, 476, j);
}

static void pantalla_reglas(Juego *j)
{
    DrawText("REGLAS DEL SIMULADOR", 60, 28, 30, COLOR_TEXTO);
    DrawText("Sos el director de vuelo del reingreso de la cápsula Orion (Misión Artemis).", 60, 68, 18, COLOR_SUAVE);

    /* Evento 1 detallado con las nuevas mecánicas */
    DrawText("1) Evento 1 - Alineación de Reingreso con RCS (por turnos):", 60, 104, 19, COLOR_TEXTO);
    DrawText(TextFormat("   • Meta en t = 0: llegar con ángulo entre %.1f° y %.1f° Y giro estable (|giro| <= %.2f°/t).",
                        GAMMA_MIN, GAMMA_MAX, OMEGA_MAX_SEGURO),
             60, 130, 16, COLOR_OK);
    DrawText("   • Inercia (1.ª Ley de Newton): la nave conserva su velocidad de giro (omega) entre turnos.",
             60, 152, 16, COLOR_SUAVE);
    DrawText(TextFormat("   • Propulsores: Pulso Suave (±%.1f°/t, %d RCS), Pulso Fuerte (±%.1f°/t, %d RCS) o Mantener.",
                        PULSO_SUAVE, COSTO_SUAVE, PULSO_FUERTE, COSTO_FUERTE),
             60, 174, 16, COLOR_SUAVE);
    DrawText("   • Prógrado (-) aplana la trayectoria (baja el ángulo); Retrógrado (+) la empina (sube).",
             60, 196, 16, COLOR_SUAVE);
    DrawText("   • Toberas y Radar: usar un Pulso Fuerte recalienta las toberas por 1 turno. El Radar",
             60, 218, 16, COLOR_SUAVE);
    DrawText("     anticipa la anomalía gravitatoria/atmosférica que actuará en el turno actual.",
             60, 238, 16, COLOR_SUAVE);

    /* Eventos 2 y 3 */
    DrawText("2) Evento 2 - Frenado Aerodinámico: la desaceleración media no debe superar los g máximos.",
             60, 274, 19, COLOR_TEXTO);
    DrawText("3) Evento 3 - Paracaídas y Amerizaje: la fuerza del agua no debe quebrar la estructura.",
             60, 306, 19, COLOR_TEXTO);

    /* Sistema de puntaje */
    DrawRectangle(60, 344, 780, 108, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(60, 344, 780, 108, COLOR_BARRA);
    DrawText(TextFormat("PUNTAJE: %d pts por evento superado (-%d pts por cada unidad de RCS gastada).",
                        PUNTOS_EVENTO, PENALIZACION_RCS),
             78, 360, 17, COLOR_AVISO);
    DrawText(TextFormat("BONUS DE PRECISIÓN: +%d pts si completás la misión con ángulo a ±%.2f° de %.2f°.",
                        BONUS_PRECISION, TOLERANCIA_BONUS, GAMMA_CENTRO),
             78, 388, 17, COLOR_AVISO);
    DrawText(TextFormat("Puntaje máximo posible: %d puntos. La dificultad ajusta RCS, turnos, deriva y límites g.",
                        NUM_EVENTOS * PUNTOS_EVENTO + BONUS_PRECISION),
             78, 416, 16, COLOR_SUAVE);

    if (boton(60, 480, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_config(Juego *j)
{
    DrawText("CONFIGURAR DATOS DE LA MISIÓN", 40, 22, 28, COLOR_TEXTO);
    DrawText("Aviso: Los minijuegos están calibrados con los datos del enunciado. Cambiarlos puede volver imposible la misión.",
             40, 54, 15, COLOR_AVISO);

    for (int i = 0; i < NUM_CAMPOS; i++) {
        int y = 80 + i * 42;

        DrawText(CAMPOS[i].etiqueta, 40, y + 6, 18, COLOR_TEXTO);
        /* GuiTextBox devuelve 1 cuando se hace clic: ahí se activa o se desactiva la edición */
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

/* Dibuja el corte atmosférico y la curva de trayectoria según el ángulo gamma */
static void dibujar_trayectoria_analisis(int x, int y, int ancho, int alto, float gamma)
{
    EstadoAngulo estado = clasificar_angulo(gamma);
    Color color_curva = (estado == ANGULO_SEGURO)   ? COLOR_OK
                      : (estado == ANGULO_EXCESIVO) ? COLOR_ERROR
                                                    : COLOR_AVISO;

    /* Fondo del visor espacial */
    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

    /* Capas de la atmósfera (alta atmósfera y océano abajo) */
    int y_atm = y + 95;
    int y_mar = y + alto - 36;
    DrawRectangle(x + 1, y_atm, ancho - 2, y_mar - y_atm, (Color){ 20, 45, 95, 90 });
    DrawRectangle(x + 1, y_mar, ancho - 2, 35, (Color){ 25, 85, 170, 200 });

    /* Línea punteada de interfaz de entrada (h = 120 km) */
    for (int lx = x + 10; lx < x + ancho - 10; lx += 18) {
        DrawLine(lx, y_atm, lx + 10, y_atm, COLOR_SUAVE);
    }
    DrawText("Interfaz 120 km", x + ancho - 125, y_atm - 18, 14, COLOR_SUAVE);
    DrawText("Océano Pacífico", x + ancho - 125, y_mar + 10, 14, COLOR_TEXTO);

    /* Punto de entrada atmosférica */
    Vector2 p_inicio = { (float)(x + 30), (float)(y + 35) };
    Vector2 p_entrada = { (float)(x + 125), (float)y_atm };
    DrawLineEx(p_inicio, p_entrada, 2.5f, COLOR_SUAVE);

    /* Cono de ventana segura (referencia visual tenue en verde) */
    DrawTriangle(p_entrada,
                 (Vector2){ (float)(x + 250), (float)y_mar },
                 (Vector2){ (float)(x + 330), (float)y_mar },
                 (Color){ 60, 220, 120, 28 });

    /* Curva dinámica por tramos según el ángulo elegido en el slider */
    Vector2 p_ant = p_entrada;
    int pasos = 24;

    for (int i = 1; i <= pasos; i++) {
        float t = (float)i / (float)pasos;
        Vector2 p_act;

        if (estado == ANGULO_REBOTE) {
            /* Parábola que toca las capas altas y rebota de nuevo al espacio */
            float prof = 45.0f * (gamma / (float)GAMMA_MIN);
            p_act.x = p_entrada.x + t * (ancho - 150);
            p_act.y = p_entrada.y + 4.0f * prof * t * (1.0f - t) - t * 40.0f;
        } else {
            /* Penetración hacia el mar: cuanto mayor es gamma, más corto y brusco cae */
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

    /* Dibujo de la cápsula en el punto de entrada y marcador final */
    DrawCircleV(p_entrada, 5.0f, COLOR_TEXTO);
    DrawCircleV(p_ant, 6.0f, color_curva);
}

static void pantalla_analisis(Juego *j)
{
    const DatosMision *d = &j->datos;

    DrawText("LABORATORIO DE ANÁLISIS FÍSICO", 40, 22, 28, COLOR_TEXTO);

    /* --- COLUMNA IZQUIERDA: Controles + Gráfico de trayectoria --- */
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

    /* --- COLUMNA DERECHA: Resultados de las 4 preguntas al presionar Calcular --- */
    DrawRectangle(435, 64, 425, 472, (Color){ 16, 24, 48, 255 });
    DrawRectangleLines(435, 64, 425, 472, COLOR_BARRA);

    if (!j->analisis_calculado) {
        DrawText("Ajustá el ángulo con el deslizador\ny presioná \"CALCULAR SIMULACIÓN\"\npara obtener el reporte analítico.",
                 465, 250, 20, COLOR_SUAVE);
        return;
    }

    /* Cálculos base del enunciado (para gamma nominal = 6.25°) */
    double ei        = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double w_nominal = calcular_trabajo_rozamiento(d);
    double a_nominal = calcular_desaceleracion_media(d);
    double f_agua    = calcular_fuerza_amerizaje(d);
    EstadoAngulo estado = clasificar_angulo(j->gamma_calculado);

    /* Factor físico geométrico según el ángulo de entrada: sin(gamma) / sin(6.25°) */
    double rad_ref = 6.25 * (3.14159265358979323846 / 180.0);
    double rad_act = ((j->gamma_calculado < 0.5f) ? 0.5 : j->gamma_calculado) * (3.14159265358979323846 / 180.0);
    double factor_angulo = sin(rad_act) / sin(rad_ref);

    /* Tiempo efectivo de frenado y desaceleración real con ese ángulo */
    double dt_efectivo = d->dt_frenado / factor_angulo;
    double a_real      = (d->v_inicial - d->v_paracaidas) / dt_efectivo;

    /* Si rebota (< 5.5°), solo roza la atmósfera y no completa el frenado */
    double fraccion_captura = (estado == ANGULO_REBOTE)
                            ? (j->gamma_calculado / GAMMA_MIN) * 0.35
                            : 1.0;
    double w_real = w_nominal * fraccion_captura;

    DrawText(TextFormat("REPORTE ANALÍTICO (gamma = %.2f°)", j->gamma_calculado),
             455, 78, 20, COLOR_AVISO);
    DrawRectangle(455, 104, 385, 2, COLOR_BARRA);

    /* 1) Trabajo de rozamiento */
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

    /* 2) Desaceleración media */
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

    /* 4) Evaluación del ángulo */
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

/* Visor espacial limpio y amplio en la parte superior */
static void dibujar_aproximacion_evento1(int x, int y, int ancho, int alto, const Reporte *r)
{
    const Dificultad *dif = r->dificultad;
    float progreso = (dif->turnos_rcs > 0)
                   ? (float)r->num_turnos / (float)dif->turnos_rcs : 1.0f;

    DrawRectangle(x, y, ancho, alto, (Color){ 8, 12, 26, 255 });
    DrawRectangleLines(x, y, ancho, alto, COLOR_BARRA);

    /* Banda atmosférica a la derecha (meta en t = 0) */
    int x_atm = x + ancho - 110;
    DrawRectangle(x_atm, y + 1, 109, alto - 2, (Color){ 25, 65, 135, 120 });
    DrawLine(x_atm, y, x_atm, y + alto, COLOR_AVISO);
    DrawText("INTERFAZ\n120 km", x_atm + 18, y + 14, 15, COLOR_SUAVE);

    /* Corredor seguro (5.5° a 7.0°) */
    int y_min_seg = y + (int)((GAMMA_MIN / GAMMA_ESCALA) * alto);
    int y_max_seg = y + (int)((GAMMA_MAX / GAMMA_ESCALA) * alto);
    DrawRectangle(x_atm, y_min_seg, 109, y_max_seg - y_min_seg, (Color){ 60, 220, 120, 85 });
    DrawLine(x + 40, y + alto / 2, x_atm, y_min_seg, (Color){ 60, 220, 120, 90 });
    DrawLine(x + 40, y + alto / 2, x_atm, y_max_seg, (Color){ 60, 220, 120, 90 });
    DrawText("5.5° - 7.0°", x_atm + 18, (y_min_seg + y_max_seg) / 2 - 7, 14, COLOR_OK);

    /* Posición actual de la cápsula */
    float nav_x = (float)(x + 60) + progreso * (float)(x_atm - x - 75);
    float gamma_clamp = (float)r->gamma;
    if (gamma_clamp < 0.5f) gamma_clamp = 0.5f;
    if (gamma_clamp > 14.5f) gamma_clamp = 14.5f;
    float nav_y = (float)y + (gamma_clamp / GAMMA_ESCALA) * (float)alto;

    /* Vector de predicción para el próximo turno */
    if (r->eventos[0] == EV_PENDIENTE) {
        float prox_x = (float)(x + 60) + ((float)(r->num_turnos + 1) / (float)dif->turnos_rcs) * (float)(x_atm - x - 75);
        float prox_gamma = (float)(r->gamma + r->omega + r->deriva_proxima);
        if (prox_gamma < 0.5f) prox_gamma = 0.5f;
        if (prox_gamma > 14.5f) prox_gamma = 14.5f;
        float prox_y = (float)y + (prox_gamma / GAMMA_ESCALA) * (float)alto;

        DrawLineEx((Vector2){ nav_x, nav_y }, (Vector2){ prox_x, prox_y }, 2.0f, COLOR_AVISO);
        DrawCircle((int)prox_x, (int)prox_y, 4, COLOR_AVISO);
    }

    /* Dibujo vectorial de la cápsula Orion */
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

    /* Encabezado y línea única de telemetría limpia */
    DrawText("EVENTO 1: ALINEACIÓN CON RCS", 40, 20, 26, COLOR_TEXTO);
    DrawText(TextFormat("Ángulo: %.2f°   |   Giro: %+.2f°/t   |   Radar: %+.2f°/t   |   Turnos: %d   |   RCS: %d/%d",
                        r->gamma, r->omega, r->deriva_proxima,
                        turnos_rest, rcs_restante, dif->combustible_rcs),
             40, 54, 18, COLOR_SUAVE);

    /* Gráfico grande arriba aprovechando todo el ancho */
    dibujar_aproximacion_evento1(40, 84, 820, 205, r);

    /* --- Abajo a la izquierda: Botones de dos líneas --- */
    if (r->eventos[0] == EV_PENDIENTE) {
        /* Fila 1: Pulsos suaves */
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

        /* Fila 2: Pulsos fuertes */
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

        /* Fila 3: Mantener actitud */
        if (boton_doble(40, 420, 470, 48, "Mantener actitud", "0.0°/t  |  0 RCS (enfría toberas)")) {
            aplicar_turno_rcs(r, MANIOBRA_MANTENER, 0);
            j->gamma = (float)r->gamma;
        }
    } else {
        /* Cuadro de resultado al llegar a t = 0 */
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

    /* --- Abajo a la derecha: Bitácora compacta (mitad de altura) --- */
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

    /* Botones de navegación inferiores */
    if (r->eventos[0] == EV_SUPERADO) {
        if (boton(40, 488, 300, 46, "Continuar al frenado")) pasar_a_frenado(j);
    } else if (r->eventos[0] == EV_FALLIDO) {
        if (boton(40, 488, 300, 46, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 488, 240, 46, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
}
static void pantalla_evento2(Juego *j)
{
    const Reporte *r = &j->reporte;
    const DatosMision *d = &j->datos;
    int ok = (r->eventos[1] == EV_SUPERADO);

    DrawText("EVENTO 2: FRENADO AERODINÁMICO", 40, 30, 28, COLOR_TEXTO);
    DrawText(TextFormat("De %.0f m/s a %.0f m/s en %.0f s",
                        d->v_inicial, d->v_paracaidas, d->dt_frenado),
             40, 78, 18, COLOR_SUAVE);

    DrawText("Velocidad en el tiempo", 70, 112, 18, COLOR_TEXTO);
    dibujar_grafico_velocidad(70, 160, 450, 230, r->tabla, TAM_TABLA, d->v_inicial, d->dt_frenado);
    DrawText("Se supone desaceleración media constante.", 70, 424, 16, COLOR_SUAVE);

    DrawText("RESULTADOS", 590, 90, 24, COLOR_TEXTO);
    DrawText("Trabajo del rozamiento:", 590, 130, 18, COLOR_SUAVE);
    DrawText(TextFormat("%.3e J", r->trabajo_roz), 590, 154, 22, COLOR_TEXTO);
    dibujar_barra(590, 210, 270, "Carga de g (frenado)", r->desaceleracion_g,
                  j->dificultad->limite_g_frenado, "g");

    if (ok) {
        DrawText("La tripulación\nsoporta el frenado.", 590, 320, 22, COLOR_OK);
    } else {
        DrawText("Carga de g excesiva:\nla tripulación no\nlo resiste.", 590, 320, 22, COLOR_ERROR);
    }

    if (ok) {
        if (boton(40, 480, 300, 50, "Continuar al amerizaje")) pasar_a_amerizaje(j);
    } else {
        if (boton(40, 480, 300, 50, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 480, 240, 50, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_evento3(Juego *j)
{
    const Reporte *r = &j->reporte;
    const DatosMision *d = &j->datos;
    int ok = (r->eventos[2] == EV_SUPERADO);

    DrawText("EVENTO 3: PARACAÍDAS Y AMERIZAJE", 40, 30, 28, COLOR_TEXTO);

    DrawText("Paracaídas principales", 40, 100, 22, COLOR_TEXTO);
    DrawText(TextFormat("Se abren a %.0f m con %.1f m/s", d->h_paracaidas, d->v_paracaidas),
             40, 130, 18, COLOR_SUAVE);

    DrawText("Amerizaje", 40, 190, 22, COLOR_TEXTO);
    DrawText(TextFormat("Impacto a %.1f m/s. La cápsula se hunde %.2f m\nhasta detenerse.",
                        d->v_impacto, d->d_hundimiento),
             40, 220, 18, COLOR_SUAVE);

    DrawText("Fuerza promedio del agua", 40, 300, 22, COLOR_TEXTO);
    DrawText("Teorema trabajo-energía:  F = m (g + v²/2d)", 40, 330, 18, COLOR_SUAVE);
    DrawText(TextFormat("F = %.1f N", r->fuerza_impacto), 40, 360, 22, COLOR_AVISO);

    DrawText("RESULTADOS", 590, 90, 24, COLOR_TEXTO);
    dibujar_barra(590, 140, 270, "Fuerza en el amerizaje", r->fuerza_en_pesos,
                  j->dificultad->limite_g_impacto, "x peso");

    if (ok) {
        DrawText("La estructura resiste\nel amerizaje.", 590, 250, 22, COLOR_OK);
    } else {
        DrawText("La estructura no\nresiste el impacto.", 590, 250, 22, COLOR_ERROR);
    }

    if (boton(40, 480, 300, 50, "Ver reporte final")) terminar_mision(j);
    if (boton(360, 480, 240, 50, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
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

/* Tema oscuro para los botones, cuadros de texto y sliders de raygui. */
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
    SetExitKey(KEY_NULL);           /* que ESC no cierre el programa por error */
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
