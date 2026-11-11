/*
 * artemis.c
 * Lógica del simulador: física, eventos de la misión, puntaje y reportes.
 *
 * Organización del archivo:
 *   1) Física                    -> cálculos puros
 *   2) Lógica de la misión       -> la usan la consola Y la interfaz gráfica
 *   3) Versión de consola        -> printf + teclado (no la usa la interfaz gráfica)
 */
#include <stdio.h>
#include <string.h>
#include "artemis.h"
#include "validaciones.h"

/* ---------- Tablas constantes (arreglos) ---------- */
static const Dificultad DIFICULTADES[NUM_DIFICULTADES] = {
    /* nombre     g frenado  g impacto  correcciones */
    { "Fácil",    5.0,       5.0,       3 },
    { "Normal",   3.0,       3.5,       2 },
    { "Difícil",  2.5,       3.0,       1 }
};

static const char *NOMBRES_EVENTOS[NUM_EVENTOS] = {
    "Ventana de reingreso",
    "Frenado aerodinámico",
    "Paracaídas y amerizaje"
};

/* =====================================================================
 *                           1) FÍSICA
 * ===================================================================== */

double energia_cinetica(double masa, double velocidad)
{
    return 0.5 * masa * velocidad * velocidad;
}

double energia_potencial(double masa, double altura)
{
    return masa * G_TERRESTRE * altura;
}

double energia_mecanica(double masa, double velocidad, double altura)
{
    return energia_cinetica(masa, velocidad) + energia_potencial(masa, altura);
}

/*
 * Teorema trabajo-energía: W_no_conservativas = Em_final - Em_inicial
 * Acá la única fuerza no conservativa es el rozamiento del aire (W < 0).
 */
double calcular_trabajo_rozamiento(const DatosMision *d)
{
    double e_inicial = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double e_final   = energia_mecanica(d->masa, d->v_paracaidas, d->h_paracaidas);

    return e_final - e_inicial;
}

/* Desaceleración media = variación de velocidad / tiempo (valor positivo). */
double calcular_desaceleracion_media(const DatosMision *d)
{
    return (d->v_inicial - d->v_paracaidas) / d->dt_frenado;
}

/*
 * Amerizaje: la cápsula frena de v_impacto a 0 recorriendo d_hundimiento.
 * Trabajo neto = variación de energía cinética:
 *      (m*g - F) * d = 0 - (1/2)*m*v^2   ->   F = m * (g + v^2 / (2*d))
 * (se desprecia el empuje del agua / flotación)
 */
double calcular_fuerza_amerizaje(const DatosMision *d)
{
    double v2 = d->v_impacto * d->v_impacto;

    return d->masa * (G_TERRESTRE + v2 / (2.0 * d->d_hundimiento));
}

EstadoAngulo clasificar_angulo(double gamma)
{
    if (gamma < GAMMA_MIN) {
        return ANGULO_REBOTE;
    }
    if (gamma > GAMMA_MAX) {
        return ANGULO_EXCESIVO;
    }
    return ANGULO_SEGURO;
}

/* Velocidad en cada instante suponiendo desaceleración media constante. */
void generar_tabla_frenado(const DatosMision *d, PasoFrenado tabla[], int n)
{
    double a    = calcular_desaceleracion_media(d);
    double paso = d->dt_frenado / n;

    for (int i = 0; i <= n; i++) {
        tabla[i].tiempo           = paso * i;
        tabla[i].velocidad        = d->v_inicial - a * tabla[i].tiempo;
        tabla[i].energia_cinetica = energia_cinetica(d->masa, tabla[i].velocidad);
    }
}

/* =====================================================================
 *            2) LÓGICA DE LA MISIÓN (sin printf ni teclado)
 * ===================================================================== */

const Dificultad *obtener_dificultad(int indice)
{
    if (indice < 0) {
        indice = 0;
    }
    if (indice >= NUM_DIFICULTADES) {
        indice = NUM_DIFICULTADES - 1;
    }
    return &DIFICULTADES[indice];
}

const char *nombre_evento(int indice)
{
    if (indice < 0 || indice >= NUM_EVENTOS) {
        return "";
    }
    return NOMBRES_EVENTOS[indice];
}

const char *nombre_estado_evento(EstadoEvento estado)
{
    switch (estado) {
    case EV_SUPERADO: return "SUPERADO";
    case EV_FALLIDO:  return "FALLIDO";
    default:          return "no alcanzado";
    }
}

/* Validación cruzada: que los datos tengan sentido físico entre sí. */
int datos_coherentes(const DatosMision *d, char *mensaje, int tam)
{
    const char *motivo = NULL;

    if (d->h_inicial <= d->h_paracaidas) {
        motivo = "La altitud de reingreso debe ser mayor que la de los paracaídas.";
    } else if (d->v_inicial <= d->v_paracaidas) {
        motivo = "La velocidad de reingreso debe ser mayor que la de apertura de paracaídas.";
    } else if (d->v_paracaidas <= d->v_impacto) {
        motivo = "La velocidad al abrir paracaídas debe ser mayor que la de impacto.";
    }

    if (motivo != NULL) {
        if (mensaje != NULL && tam > 0) {
            snprintf(mensaje, (size_t)tam, "%s", motivo);
        }
        return 0;
    }
    return 1;
}

void iniciar_reporte(Reporte *rep, const Dificultad *dif)
{
    memset(rep, 0, sizeof *rep);
    rep->dificultad = dif;
}

EstadoAngulo resolver_angulo(Reporte *rep, const Dificultad *dif, double gamma)
{
    EstadoAngulo estado = clasificar_angulo(gamma);

    rep->gamma = gamma;
    if (estado == ANGULO_SEGURO) {
        rep->eventos[0] = EV_SUPERADO;
    } else if (rep->correcciones_usadas >= dif->correcciones) {
        rep->eventos[0] = EV_FALLIDO;
    } else {
        rep->correcciones_usadas++;
    }
    return estado;
}

void resolver_frenado(const DatosMision *d, const Dificultad *dif, Reporte *rep)
{
    rep->energia_inicial  = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    rep->energia_final    = energia_mecanica(d->masa, d->v_paracaidas, d->h_paracaidas);
    rep->trabajo_roz      = calcular_trabajo_rozamiento(d);
    rep->desaceleracion   = calcular_desaceleracion_media(d);
    rep->desaceleracion_g = rep->desaceleracion / G_TERRESTRE;
    generar_tabla_frenado(d, rep->tabla, NUM_PASOS_FRENADO);

    rep->eventos[1] = (rep->desaceleracion_g <= dif->limite_g_frenado) ? EV_SUPERADO : EV_FALLIDO;
}

void resolver_amerizaje(const DatosMision *d, const Dificultad *dif, Reporte *rep)
{
    rep->fuerza_impacto  = calcular_fuerza_amerizaje(d);
    rep->fuerza_en_pesos = rep->fuerza_impacto / (d->masa * G_TERRESTRE);

    rep->eventos[2] = (rep->fuerza_en_pesos <= dif->limite_g_impacto) ? EV_SUPERADO : EV_FALLIDO;
}

void calcular_puntaje(Reporte *rep)
{
    int puntos = 0;
    int superados = 0;

    if (rep->eventos[0] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO - PENALIZACION_CORR * rep->correcciones_usadas;
        if (puntos < 0) {
            puntos = 0;
        }
    }
    if (rep->eventos[1] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO;
    }
    if (rep->eventos[2] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO;
    }

    for (int i = 0; i < NUM_EVENTOS; i++) {
        if (rep->eventos[i] == EV_SUPERADO) {
            superados++;
        }
    }
    rep->exito = (superados == NUM_EVENTOS);

    if (rep->exito && rep->correcciones_usadas == 0) {
        puntos += BONUS_SIN_CORREC;
    }
    rep->puntaje = puntos;
}

/* =====================================================================
 *        3) VERSIÓN DE CONSOLA (printf + teclado)
 * ===================================================================== */

void cargar_datos_por_defecto(DatosMision *d)
{
    d->masa          = MASA_CAPSULA;
    d->v_inicial     = V_REINGRESO;
    d->h_inicial     = H_REINGRESO;
    d->v_paracaidas  = V_PARACAIDAS;
    d->h_paracaidas  = H_PARACAIDAS;
    d->v_impacto     = V_IMPACTO;
    d->dt_frenado    = DT_FRENADO;
    d->d_hundimiento = D_HUNDIMIENTO;
}

void ingresar_datos_mision(DatosMision *d)
{
    char mensaje[TAM_MENSAJE];

    while (1) {
        puts("\n--- Ingreso de datos de la misión ---");
        d->masa          = leer_double("Masa de la cápsula (kg)",            1000.0, 50000.0);
        d->v_inicial     = leer_double("Velocidad de reingreso (m/s)",        100.0, 15000.0);
        d->h_inicial     = leer_double("Altitud de reingreso (m)",          10000.0, 200000.0);
        d->v_paracaidas  = leer_double("Velocidad al abrir paracaídas (m/s)",   1.0, 200.0);
        d->h_paracaidas  = leer_double("Altitud de paracaídas (m)",           100.0, 10000.0);
        d->v_impacto     = leer_double("Velocidad de impacto (m/s)",            0.5, 30.0);
        d->dt_frenado    = leer_double("Duración del frenado máximo (s)",      10.0, 2000.0);
        d->d_hundimiento = leer_double("Hundimiento en el agua (m)",            0.1, 10.0);

        if (datos_coherentes(d, mensaje, TAM_MENSAJE)) {
            return;
        }
        mostrar_advertencia(mensaje);
    }
}

const Dificultad *seleccionar_dificultad(void)
{
    puts("\n--- Dificultad ---");
    for (int i = 0; i < NUM_DIFICULTADES; i++) {
        printf("  %d) %s | máx. %.1f g en frenado | máx. %.1fx peso en impacto | correcciones: %d\n",
               i + 1, DIFICULTADES[i].nombre,
               DIFICULTADES[i].limite_g_frenado,
               DIFICULTADES[i].limite_g_impacto,
               DIFICULTADES[i].correcciones);
    }
    int opcion = leer_entero("Elegí la dificultad", 1, NUM_DIFICULTADES);

    return &DIFICULTADES[opcion - 1];
}

static void explicar_angulo(EstadoAngulo estado)
{
    switch (estado) {
    case ANGULO_REBOTE:
        puts("  -> gamma demasiado chico: REBOTE ATMOSFÉRICO.");
        puts("     La cápsula rebota en las capas altas y vuelve al espacio sin frenar lo suficiente.");
        break;
    case ANGULO_EXCESIVO:
        puts("  -> gamma demasiado grande: DESACELERACIÓN EXCESIVA.");
        puts("     El frenado es tan brusco que supera lo que soportan la tripulación y el escudo térmico.");
        break;
    default:
        puts("  -> Ángulo dentro de la ventana segura.");
        break;
    }
}

static void mostrar_tabla_frenado(const PasoFrenado tabla[], int n)
{
    puts("\n    t [s]     v [m/s]      Ec [J]");
    puts("  ------------------------------------");
    for (int i = 0; i < n; i++) {
        printf("  %7.1f  %9.1f  %12.4e\n",
               tabla[i].tiempo, tabla[i].velocidad, tabla[i].energia_cinetica);
    }
}

void ejecutar_mision(const DatosMision *d, const Dificultad *dif, Reporte *rep)
{
    EstadoAngulo estado;
    double gamma;

    iniciar_reporte(rep, dif);

    /* ---------- EVENTO 1: ventana de reingreso ---------- */
    printf("\n===== EVENTO 1: %s =====\n", NOMBRES_EVENTOS[0]);
    printf("Ventana segura: %.1f° <= gamma <= %.1f°  |  Correcciones disponibles: %d\n",
           GAMMA_MIN, GAMMA_MAX, dif->correcciones);

    while (rep->eventos[0] == EV_PENDIENTE) {
        gamma  = leer_double("Ángulo de reingreso gamma (grados)", 0.0, 90.0);
        estado = resolver_angulo(rep, dif, gamma);
        explicar_angulo(estado);

        if (rep->eventos[0] == EV_FALLIDO) {
            puts("  Sin propelente para más correcciones: SE PIERDE LA MISIÓN.");
        } else if (estado != ANGULO_SEGURO) {
            printf("  Corrección usada (%d/%d). Probá con otro ángulo.\n",
                   rep->correcciones_usadas, dif->correcciones);
        }
    }
    if (rep->eventos[0] == EV_FALLIDO) {
        calcular_puntaje(rep);
        return;
    }

    /* ---------- EVENTO 2: frenado aerodinámico ---------- */
    printf("\n===== EVENTO 2: %s =====\n", NOMBRES_EVENTOS[1]);
    resolver_frenado(d, dif, rep);

    mostrar_tabla_frenado(rep->tabla, TAM_TABLA);
    printf("\n  Trabajo del rozamiento : %.4e J\n", rep->trabajo_roz);
    printf("  Desaceleración media   : %.2f m/s^2 = %.2f g  (límite %.1f g)\n",
           rep->desaceleracion, rep->desaceleracion_g, dif->limite_g_frenado);

    if (rep->eventos[1] == EV_SUPERADO) {
        puts("  -> La tripulación soporta el frenado. EVENTO SUPERADO.");
    } else {
        puts("  -> Carga de g excesiva para la tripulación. EVENTO FALLIDO.");
        calcular_puntaje(rep);
        return;
    }

    /* ---------- EVENTO 3: paracaídas y amerizaje ---------- */
    printf("\n===== EVENTO 3: %s =====\n", NOMBRES_EVENTOS[2]);
    printf("  Paracaídas abiertos a %.1f m con %.1f m/s. Impacto a %.1f m/s, hundimiento %.2f m.\n",
           d->h_paracaidas, d->v_paracaidas, d->v_impacto, d->d_hundimiento);

    resolver_amerizaje(d, dif, rep);

    printf("  Fuerza promedio del agua: %.1f N = %.2f veces el peso (límite %.1fx)\n",
           rep->fuerza_impacto, rep->fuerza_en_pesos, dif->limite_g_impacto);

    if (rep->eventos[2] == EV_SUPERADO) {
        puts("  -> La estructura resiste el amerizaje. EVENTO SUPERADO.");
    } else {
        puts("  -> La estructura no resiste el impacto. EVENTO FALLIDO.");
    }

    calcular_puntaje(rep);
}

void mostrar_reglas(void)
{
    puts("\n================ REGLAS DEL JUEGO ================");
    puts("Sos el director de vuelo del reingreso de la cápsula Orion (Artemis).");
    puts("La misión tiene 3 eventos y se juega por turnos:");
    puts("  1) Ventana de reingreso: elegís el ángulo gamma. Debe estar entre");
    printf("     %.1f° y %.1f°. Si no, hay rebote atmosférico (muy chico) o\n", GAMMA_MIN, GAMMA_MAX);
    puts("     desaceleración excesiva (muy grande). Cada error gasta una corrección.");
    puts("  2) Frenado aerodinámico: se calcula el trabajo del rozamiento y la");
    puts("     desaceleración media. Si supera el límite de g, la tripulación no resiste.");
    puts("  3) Paracaídas y amerizaje: se calcula la fuerza promedio del agua.");
    puts("     Si supera el límite, la estructura falla.");
    puts("Puntaje: 300 por evento (-100 por cada corrección usada) + 100 de bonus");
    puts("si completás todo sin correcciones. Máximo: 1000 puntos.");
    puts("La dificultad cambia los límites y las correcciones disponibles.");
}

void mostrar_datos(const DatosMision *d)
{
    puts("\n--- Datos actuales de la misión ---");
    printf("  Masa de la cápsula        : %.1f kg\n", d->masa);
    printf("  Velocidad de reingreso    : %.1f m/s\n", d->v_inicial);
    printf("  Altitud de reingreso      : %.1f m\n", d->h_inicial);
    printf("  Velocidad al abrir paracaídas: %.1f m/s\n", d->v_paracaidas);
    printf("  Altitud de paracaídas     : %.1f m\n", d->h_paracaidas);
    printf("  Velocidad de impacto      : %.1f m/s\n", d->v_impacto);
    printf("  Duración frenado máximo   : %.1f s\n", d->dt_frenado);
    printf("  Hundimiento en el agua    : %.2f m\n", d->d_hundimiento);
}

/* Respuestas analíticas a las 4 preguntas del enunciado. */
void mostrar_analisis(const DatosMision *d, double gamma)
{
    double ei = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double ef = energia_mecanica(d->masa, d->v_paracaidas, d->h_paracaidas);
    double w  = calcular_trabajo_rozamiento(d);
    double a  = calcular_desaceleracion_media(d);
    double f  = calcular_fuerza_amerizaje(d);

    puts("\n================ ANÁLISIS ANALÍTICO ================");

    puts("\n1) Trabajo de la fuerza de rozamiento (W_roz = Em_final - Em_inicial)");
    printf("   Em inicial = %.4e J\n", ei);
    printf("   Em final   = %.4e J\n", ef);
    printf("   W_roz      = %.4e J  (negativo: el aire disipa energía)\n", w);
    printf("   Energía disipada: %.3f %% de la inicial\n", 100.0 * (-w) / ei);

    puts("\n2) Desaceleración media durante el frenado");
    printf("   a = (vi - vf) / dt = %.2f m/s^2 = %.2f g\n", a, a / G_TERRESTRE);

    puts("\n3) Fuerza promedio del agua en el amerizaje  F = m (g + v^2 / 2d)");
    printf("   F = %.1f N = %.2f veces el peso de la nave\n", f, f / (d->masa * G_TERRESTRE));

    printf("\n4) Ángulo de reingreso gamma = %.2f° (ventana segura: %.1f° a %.1f°)\n",
           gamma, GAMMA_MIN, GAMMA_MAX);
    switch (clasificar_angulo(gamma)) {
    case ANGULO_SEGURO:
        puts("   Dentro de la ventana: el frenado es controlado.");
        break;
    case ANGULO_EXCESIVO:
        puts("   gamma > 7,0°: trayectoria muy empinada, la nave se mete rápido en aire denso.");
        puts("   Desaceleración excesiva y calentamiento extremo (riesgo para tripulación y escudo).");
        break;
    default:
        puts("   gamma < 5,5°: trayectoria muy tendida, la nave no es capturada por la atmósfera.");
        puts("   Rebote atmosférico: la nave vuelve al espacio y se pierde el reingreso.");
        break;
    }
}

void mostrar_reporte(const Reporte *r)
{
    puts("\n=============== REPORTE FINAL DE LA MISIÓN ===============");
    printf("Dificultad           : %s\n", r->dificultad->nombre);
    printf("Ángulo de reingreso  : %.2f°\n", r->gamma);
    printf("Correcciones usadas  : %d de %d\n", r->correcciones_usadas, r->dificultad->correcciones);

    for (int i = 0; i < NUM_EVENTOS; i++) {
        printf("Evento %d (%s): %s\n", i + 1, nombre_evento(i), nombre_estado_evento(r->eventos[i]));
    }

    if (r->eventos[1] != EV_PENDIENTE) {
        printf("Trabajo del rozamiento: %.4e J\n", r->trabajo_roz);
        printf("Desaceleración media  : %.2f g\n", r->desaceleracion_g);
    }
    if (r->eventos[2] != EV_PENDIENTE) {
        printf("Fuerza en amerizaje   : %.1f N (%.2fx el peso)\n", r->fuerza_impacto, r->fuerza_en_pesos);
    }

    printf("\nPUNTAJE: %d / %d\n", r->puntaje,
           NUM_EVENTOS * PUNTOS_EVENTO + BONUS_SIN_CORREC);
    puts(r->exito ? "RESULTADO: ¡MISIÓN EXITOSA! La tripulación está a salvo."
                  : "RESULTADO: MISIÓN FALLIDA.");
}
