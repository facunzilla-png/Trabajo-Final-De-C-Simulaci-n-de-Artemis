/*
 * interfaz.c
 * Interfaz gráfica (raylib + raygui): dibuja las pantallas y atiende los botones.
 *
 * Esta capa NO hace cálculos físicos ni reglas del juego: llama a las funciones de
 * artemis.c (resolver_angulo, resolver_frenado, resolver_amerizaje, ...) y a
 * validaciones.c (validar_double). Solo muestra los resultados.
 */
#include <stdio.h>
#include <string.h>
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION      /* raygui.h se "implementa" en UN solo .c: este */
#include "raygui.h"
#include "interfaz.h"
#include "validaciones.h"

#define ANCHO_VENTANA   900
#define ALTO_VENTANA    560
#define GAMMA_ESCALA    15.0f       /* el slider de ángulo va de 0° a 15° */

/* ---------- Colores del tema ---------- */
#define COLOR_FONDO     (Color){  12,  18,  38, 255 }
#define COLOR_TEXTO     (Color){ 230, 235, 255, 255 }
#define COLOR_SUAVE     (Color){ 160, 172, 205, 255 }
#define COLOR_OK        (Color){  60, 220, 120, 255 }
#define COLOR_AVISO     (Color){ 255, 190,  60, 255 }
#define COLOR_ERROR     (Color){ 240,  80,  90, 255 }
#define COLOR_BARRA     (Color){  50,  60,  95, 255 }

/* ---------- Campos de la pantalla de configuración ---------- */
typedef struct {
    const char *etiqueta;
    double minimo;
    double maximo;
} CampoConfig;

/* El orden es el mismo que el de los campos de DatosMision. */
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

/* =====================================================================
 *                      ELEMENTOS VISUALES REUTILIZABLES
 * ===================================================================== */

static int boton(int x, int y, int ancho, int alto, const char *texto)
{
    return GuiButton((Rectangle){ (float)x, (float)y, (float)ancho, (float)alto }, texto);
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

/* Franja con las zonas de ángulo: rebote / seguro / excesivo, y un cursor. */
static void dibujar_ventana_angulo(int x, int y, int ancho, float gamma)
{
    float u  = ancho / GAMMA_ESCALA;           /* píxeles por grado */
    int   x1 = x + (int)(GAMMA_MIN * u);
    int   x2 = x + (int)(GAMMA_MAX * u);

    DrawRectangle(x,  y, x1 - x,         30, COLOR_AVISO);   /* rebote   */
    DrawRectangle(x1, y, x2 - x1,        30, COLOR_OK);      /* seguro   */
    DrawRectangle(x2, y, x + ancho - x2, 30, COLOR_ERROR);   /* excesivo */
    DrawRectangle(x + (int)(gamma * u) - 2, y - 8, 4, 46, COLOR_TEXTO);   /* cursor */

    DrawText("rebote",   x + 4,          y + 36, 16, COLOR_AVISO);
    DrawText("seguro",   x1 - 6,         y + 36, 16, COLOR_OK);
    DrawText("excesivo", x + ancho - 80, y + 36, 16, COLOR_ERROR);
}

/* Gráfico de velocidad en función del tiempo (usa la tabla del reporte). */
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

/* =====================================================================
 *                 CAMBIOS DE PANTALLA (usan la lógica de artemis.c)
 * ===================================================================== */

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

/*
 * Valida los 8 cuadros de texto. Si todos son válidos y coherentes entre sí,
 * los copia a 'salida' y devuelve 1. Si no, devuelve 0 y deja el motivo en 'mensaje'.
 */
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
    j->gamma = 6.0f;
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

static void pantalla_menu(Juego *j)
{
    DrawText("ARTEMIS", 60, 50, 60, COLOR_TEXTO);
    DrawText("Reingreso, frenado aerodinámico y amerizaje", 60, 120, 22, COLOR_SUAVE);
    DrawText(TextFormat("Datos: %s   |   Misiones jugadas: %d",
                        j->datos_personalizados ? "personalizados" : "del enunciado",
                        j->misiones_jugadas),
             60, 160, 18, COLOR_AVISO);

    if (boton(60, 210, 320, 52, "Jugar misión"))        j->pantalla = PANTALLA_DIFICULTAD;
    if (boton(60, 272, 320, 52, "Configurar datos"))    entrar_config(j);
    if (boton(60, 334, 320, 52, "Análisis analítico"))  j->pantalla = PANTALLA_ANALISIS;
    if (boton(60, 396, 320, 52, "Reglas"))              j->pantalla = PANTALLA_REGLAS;
    if (boton(60, 458, 320, 52, "Salir"))               j->salir = 1;

    if (!j->hay_reporte) {
        GuiDisable();
    }
    if (boton(420, 210, 320, 52, "Ver último reporte")) j->pantalla = PANTALLA_REPORTE;
    GuiEnable();
}

static void pantalla_reglas(Juego *j)
{
    DrawText("REGLAS DEL JUEGO", 60, 40, 32, COLOR_TEXTO);
    DrawText("Sos el director de vuelo del reingreso de la cápsula Orion.", 60, 100, 20, COLOR_SUAVE);
    DrawText("La misión tiene 3 eventos que se juegan en orden:", 60, 130, 20, COLOR_SUAVE);

    DrawText(TextFormat("1) Ventana de reingreso: el ángulo debe estar entre %.1f° y %.1f°.", GAMMA_MIN, GAMMA_MAX),
             60, 175, 20, COLOR_TEXTO);
    DrawText("    Muy chico: rebote atmosférico. Muy grande: desaceleración excesiva.", 60, 202, 18, COLOR_SUAVE);
    DrawText("    Cada error gasta una corrección de trayectoria.", 60, 226, 18, COLOR_SUAVE);

    DrawText("2) Frenado aerodinámico: la carga de g no debe superar el límite.", 60, 266, 20, COLOR_TEXTO);
    DrawText("3) Amerizaje: la fuerza del agua no debe superar el límite.", 60, 306, 20, COLOR_TEXTO);

    DrawText(TextFormat("Puntaje: %d por evento (-%d por corrección usada en el 1°)", PUNTOS_EVENTO, PENALIZACION_CORR),
             60, 356, 20, COLOR_AVISO);
    DrawText(TextFormat("+ %d de bonus si completás todo sin usar correcciones. Máximo: %d.",
                        BONUS_SIN_CORREC, NUM_EVENTOS * PUNTOS_EVENTO + BONUS_SIN_CORREC),
             60, 384, 20, COLOR_AVISO);
    DrawText("La dificultad cambia los límites y las correcciones disponibles.", 60, 424, 20, COLOR_SUAVE);

    if (boton(60, 480, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_config(Juego *j)
{
    DrawText("CONFIGURAR DATOS DE LA MISIÓN", 40, 25, 28, COLOR_TEXTO);

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
        DrawText(TextFormat("Frenado: máx. %.1f g", dif->limite_g_frenado), 330, y + 8, 20, COLOR_TEXTO);
        DrawText(TextFormat("Amerizaje: máx. %.1fx el peso  |  Correcciones: %d",
                            dif->limite_g_impacto, dif->correcciones),
                 330, y + 38, 18, COLOR_SUAVE);
    }

    if (boton(60, 480, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_analisis(Juego *j)
{
    const DatosMision *d = &j->datos;
    double ei = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double ef = energia_mecanica(d->masa, d->v_paracaidas, d->h_paracaidas);
    double w  = calcular_trabajo_rozamiento(d);
    double a  = calcular_desaceleracion_media(d);
    double f  = calcular_fuerza_amerizaje(d);
    EstadoAngulo estado = clasificar_angulo(j->gamma_analisis);

    DrawText("ANÁLISIS ANALÍTICO", 40, 20, 28, COLOR_TEXTO);

    DrawText("1) Trabajo del rozamiento:  W = Em final - Em inicial", 40, 66, 18, COLOR_TEXTO);
    DrawText(TextFormat("Em inicial = %.4e J     Em final = %.4e J", ei, ef), 60, 90, 18, COLOR_SUAVE);
    DrawText(TextFormat("W = %.4e J  (el aire disipa el %.3f %% de la energía)", w, 100.0 * (-w) / ei),
             60, 114, 18, COLOR_AVISO);

    DrawText("2) Desaceleración media del frenado", 40, 154, 18, COLOR_TEXTO);
    DrawText(TextFormat("a = (vi - vf) / dt = %.2f m/s² = %.2f g", a, a / G_TERRESTRE),
             60, 178, 18, COLOR_AVISO);

    DrawText("3) Fuerza promedio del agua en el amerizaje:  F = m (g + v²/2d)", 40, 218, 18, COLOR_TEXTO);
    DrawText(TextFormat("F = %.1f N = %.2f veces el peso", f, f / (d->masa * G_TERRESTRE)),
             60, 242, 18, COLOR_AVISO);

    DrawText(TextFormat("4) Ángulo de reingreso: %.1f°", j->gamma_analisis), 40, 282, 18, COLOR_TEXTO);
    GuiSlider((Rectangle){ 60, 308, 500, 26 }, "0°", "15°", &j->gamma_analisis, 0.0f, GAMMA_ESCALA);
    dibujar_ventana_angulo(60, 362, 500, j->gamma_analisis);

    switch (estado) {
    case ANGULO_SEGURO:
        DrawText("Dentro de la ventana segura:\nel frenado es controlado.", 600, 330, 18, COLOR_OK);
        break;
    case ANGULO_EXCESIVO:
        DrawText("Trayectoria muy empinada: la nave\nentra rápido en aire denso. Desaceleración\nexcesiva y calentamiento extremo.",
                 600, 330, 18, COLOR_ERROR);
        break;
    default:
        DrawText("Trayectoria muy tendida: la nave no\nes capturada por la atmósfera y rebota\nal espacio.",
                 600, 330, 18, COLOR_AVISO);
        break;
    }

    if (boton(40, 490, 220, 46, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

static void pantalla_evento1(Juego *j)
{
    const Reporte *r = &j->reporte;
    int restantes = j->dificultad->correcciones - r->correcciones_usadas;

    DrawText("EVENTO 1: VENTANA DE REINGRESO", 40, 30, 28, COLOR_TEXTO);
    DrawText(TextFormat("Ángulo de reingreso: %.1f°", j->gamma), 40, 90, 22, COLOR_TEXTO);

    if (r->eventos[0] != EV_PENDIENTE) {
        GuiDisable();                           /* ya no se puede mover el slider */
    }
    GuiSlider((Rectangle){ 40, 140, 500, 28 }, "0°", "15°", &j->gamma, 0.0f, GAMMA_ESCALA);
    GuiEnable();

    dibujar_ventana_angulo(40, 230, 500, j->gamma);
    DrawText(TextFormat("Ventana segura: %.1f° a %.1f°", GAMMA_MIN, GAMMA_MAX), 40, 330, 18, COLOR_SUAVE);

    /* Panel de resultado (columna derecha) */
    DrawText("RESULTADO", 590, 90, 24, COLOR_TEXTO);
    DrawText(TextFormat("Correcciones: %d de %d", restantes, j->dificultad->correcciones),
             590, 125, 18, COLOR_AVISO);

    if (!j->hubo_intento) {
        DrawText("Elegí el ángulo y tocá\n\"Confirmar ángulo\".", 590, 170, 20, COLOR_SUAVE);
    } else {
        switch (j->ultimo_angulo) {
        case ANGULO_REBOTE:
            DrawText("REBOTE ATMOSFÉRICO", 590, 170, 22, COLOR_AVISO);
            DrawText("El ángulo es muy chico: la\ncápsula rebota y vuelve\nal espacio.", 590, 205, 18, COLOR_SUAVE);
            break;
        case ANGULO_EXCESIVO:
            DrawText("DESACELERACIÓN\nEXCESIVA", 590, 170, 22, COLOR_ERROR);
            DrawText("El ángulo es muy grande:\nel frenado es demasiado\nbrusco.", 590, 240, 18, COLOR_SUAVE);
            break;
        default:
            DrawText("VENTANA SEGURA", 590, 170, 22, COLOR_OK);
            DrawText("El ángulo permite un\nfrenado controlado.", 590, 205, 18, COLOR_SUAVE);
            break;
        }
        if (r->eventos[0] == EV_FALLIDO) {
            DrawText("Sin correcciones:\nse pierde la misión.", 590, 340, 22, COLOR_ERROR);
        } else if (r->eventos[0] == EV_PENDIENTE) {
            DrawText("Probá con otro ángulo.", 590, 340, 20, COLOR_TEXTO);
        }
    }

    /* Botón principal: cambia según cómo va el evento */
    if (r->eventos[0] == EV_PENDIENTE) {
        if (boton(40, 480, 300, 50, "Confirmar ángulo")) {
            j->ultimo_angulo = resolver_angulo(&j->reporte, j->dificultad, j->gamma);
            j->hubo_intento = 1;
        }
    } else if (r->eventos[0] == EV_SUPERADO) {
        if (boton(40, 480, 300, 50, "Continuar al frenado")) pasar_a_frenado(j);
    } else {
        if (boton(40, 480, 300, 50, "Ver reporte final")) terminar_mision(j);
    }
    if (boton(360, 480, 240, 50, "Abandonar misión")) j->pantalla = PANTALLA_MENU;
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
    DrawText(TextFormat("Ángulo de reingreso: %.2f°   |   Correcciones usadas: %d de %d",
                        r->gamma, r->correcciones_usadas, r->dificultad->correcciones),
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

    DrawText(TextFormat("PUNTAJE: %d / %d", r->puntaje, NUM_EVENTOS * PUNTOS_EVENTO + BONUS_SIN_CORREC),
             40, 390, 30, COLOR_AVISO);
    DrawText(r->exito ? "MISIÓN EXITOSA: la tripulación está a salvo." : "MISIÓN FALLIDA.",
             40, 432, 26, r->exito ? COLOR_OK : COLOR_ERROR);

    if (boton(40, 490, 240, 50, "Jugar de nuevo")) j->pantalla = PANTALLA_DIFICULTAD;
    if (boton(300, 490, 240, 50, "Volver al menú")) j->pantalla = PANTALLA_MENU;
}

/* =====================================================================
 *                         INTERFAZ PÚBLICA
 * ===================================================================== */

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
