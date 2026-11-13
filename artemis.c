
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "artemis.h"
#include "validaciones.h"

static const Dificultad DIFICULTADES[NUM_DIFICULTADES] = {
    { "Fácil",    5.0,       5.0,       8,   4,      0.20 },
    { "Normal",   3.0,       3.5,       6,   4,      0.30 },
    { "Difícil",  2.5,       3.0,       5,   4,      0.40 }
};

static const char *NOMBRES_MANIOBRAS[] = {
    "",                                 
    "Pulso suave",
    "Pulso fuerte",
    "Mantener actitud"
};

static const char *NOMBRES_EVENTOS[NUM_EVENTOS] = {
    "Alineación de reingreso con RCS",
    "Frenado aerodinámico",
    "Paracaídas y amerizaje"
};


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

double calcular_trabajo_rozamiento(const DatosMision *d)
{
    double e_inicial = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    double e_final   = energia_mecanica(d->masa, d->v_paracaidas, d->h_paracaidas);

    return e_final - e_inicial;
}


double calcular_desaceleracion_media(const DatosMision *d)
{
    return (d->v_inicial - d->v_paracaidas) / d->dt_frenado;
}

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

static double aleatorio_entre(double minimo, double maximo)
{
    return minimo + (maximo - minimo) * ((double)rand() / RAND_MAX);
}


double generar_gamma_inicial(void)
{
    double g;

    if (rand() % 2 == 0) {
        g = aleatorio_entre(GAMMA_INI_BAJO_MIN, GAMMA_INI_BAJO_MAX);
    } else {
        g = aleatorio_entre(GAMMA_INI_ALTO_MIN, GAMMA_INI_ALTO_MAX);
    }
    return round(g * 10.0) / 10.0;
}

double generar_deriva(double deriva_max)
{
    return aleatorio_entre(-deriva_max, deriva_max);
}

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

//logica de la mision//
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

const char *nombre_maniobra(TipoManiobra m)
{
    if (m < MANIOBRA_SUAVE || m > MANIOBRA_MANTENER) {
        return "-";
    }
    return NOMBRES_MANIOBRAS[m];
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
    rep->dificultad      = dif;
    rep->gamma_inicial   = generar_gamma_inicial();
    rep->gamma           = rep->gamma_inicial;
    /* Empieza con una leve deriva rotacional hacia afuera de la ventana */
    rep->omega           = (rep->gamma_inicial < GAMMA_MIN) ? -0.20 : +0.20;
    rep->deriva_proxima  = round(generar_deriva(dif->deriva_max) * 100.0) / 100.0;
    rep->rcs_recalentado = 0;
}

void aplicar_turno_rcs(Reporte *rep, TipoManiobra m, int direccion)
{
    const Dificultad *dif = rep->dificultad;
    if (rep->num_turnos >= dif->turnos_rcs) {
        return;
    }

    TurnoRCS *turno = &rep->bitacora[rep->num_turnos];
    int rcs_disp = dif->combustible_rcs - rep->rcs_usado;
    double delta = 0.0;
    int costo = 0;

    if (m == MANIOBRA_SUAVE && rcs_disp >= COSTO_SUAVE) {
        delta = direccion * PULSO_SUAVE;
        costo = COSTO_SUAVE;
        rep->rcs_recalentado = 0;
    } else if (m == MANIOBRA_FUERTE && rcs_disp >= COSTO_FUERTE && !rep->rcs_recalentado) {
        delta = direccion * PULSO_FUERTE;
        costo = COSTO_FUERTE;
        rep->rcs_recalentado = 1; /* bloquea el pulso fuerte en el próximo turno */
    } else {
        m = MANIOBRA_MANTENER;
        rep->rcs_recalentado = 0;
    }

    rep->rcs_usado += costo;
    turno->t           = dif->turnos_rcs - rep->num_turnos;
    turno->maniobra    = m;
    turno->delta_pulso = delta;
    turno->deriva      = rep->deriva_proxima;

    /* 1.ª Ley de Newton: el pulso y la anomalía cambian la velocidad angular (omega),
     * y el ángulo gamma avanza según esa velocidad angular acumulada. */
    rep->omega += delta + turno->deriva;
    rep->gamma += rep->omega;
    if (rep->gamma < 0.5) {
        rep->gamma = 0.5;
    } else if (rep->gamma > 14.5) {
        rep->gamma = 14.5;
    }

    turno->omega_resultante = rep->omega;
    turno->gamma_resultante = rep->gamma;
    turno->rcs_restante     = dif->combustible_rcs - rep->rcs_usado;
    rep->num_turnos++;

    /* Pronóstico del radar para el próximo turno */
    rep->deriva_proxima = round(generar_deriva(dif->deriva_max) * 100.0) / 100.0;

    if (rep->num_turnos >= dif->turnos_rcs) {
        int angulo_ok = (clasificar_angulo(rep->gamma) == ANGULO_SEGURO);
        int giro_ok   = (fabs(rep->omega) <= OMEGA_MAX_SEGURO);
        rep->eventos[0] = (angulo_ok && giro_ok) ? EV_SUPERADO : EV_FALLIDO;
    }
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

void calcular_puntaje(Reporte *r)
{
    int puntos = 0;
    int superados = 0;

    if (r->eventos[0] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO - PENALIZACION_RCS * r->rcs_usado;
        if (puntos < 0) {
            puntos = 0;
        }
    }
    if (r->eventos[1] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO;
    }
    if (r->eventos[2] == EV_SUPERADO) {
        puntos += PUNTOS_EVENTO;
    }

    for (int i = 0; i < NUM_EVENTOS; i++) {
        if (r->eventos[i] == EV_SUPERADO) {
            superados++;
        }
    }
    r->exito = (superados == NUM_EVENTOS);

    if (r->exito && fabs(r->gamma - GAMMA_CENTRO) <= TOLERANCIA_BONUS) {
        puntos += BONUS_PRECISION;
    }
    r->puntaje = puntos;
}

//version de consola//
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
        printf("  %d) %s | máx. %.1f g en frenado | máx. %.1fx peso en impacto\n"
               "     RCS: %d u. | turnos de alineación: %d | deriva: ±%.2f°/turno\n",
               i + 1, DIFICULTADES[i].nombre,
               DIFICULTADES[i].limite_g_frenado,
               DIFICULTADES[i].limite_g_impacto,
               DIFICULTADES[i].combustible_rcs,
               DIFICULTADES[i].turnos_rcs,
               DIFICULTADES[i].deriva_max);
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

/* ---------------- Minijuego del Evento 1: alineación con RCS ---------------- */

static void mostrar_indicador_gamma(double gamma)
{
    double paso = (INDICADOR_MAX - INDICADOR_MIN) / (INDICADOR_ANCHO - 1);
    int pos = (int)round((gamma - INDICADOR_MIN) / paso);
    int i;

    if (pos < 0) {
        pos = 0;
    } else if (pos > INDICADOR_ANCHO - 1) {
        pos = INDICADOR_ANCHO - 1;
    }

    printf("    ");
    for (i = 0; i < pos; i++) {
        putchar(' ');
    }
    puts("v");

    printf("    ");
    for (i = 0; i < INDICADOR_ANCHO; i++) {
        double g = INDICADOR_MIN + paso * i;
        putchar((g >= GAMMA_MIN - 1e-9 && g <= GAMMA_MAX + 1e-9) ? '=' : '-');
    }
    putchar('\n');
    printf("    %.0f°%*s%.0f°\n", INDICADOR_MIN, INDICADOR_ANCHO - 5, "", INDICADOR_MAX);
    printf("    (ventana segura '=': %.1f° a %.1f°)\n", GAMMA_MIN, GAMMA_MAX);
}

static TipoManiobra elegir_maniobra(int rcs)
{
    TipoManiobra m;

    puts("\n  Propulsores disponibles:");
    printf("    1) Pulso suave  prógrado/retrógrado (±%.1f°, gasta %d RCS)\n",
           PULSO_SUAVE, COSTO_SUAVE);
    printf("    2) Pulso fuerte prógrado/retrógrado (±%.1f°, gasta %d RCS)\n",
           PULSO_FUERTE, COSTO_FUERTE);
    puts("    3) Mantener actitud (0°, gasta 0 RCS, solo actúa la deriva)");

    while (1) {
        m = (TipoManiobra)leer_entero("  Elegí la maniobra", 1, 3);
        if (m == MANIOBRA_SUAVE && rcs < COSTO_SUAVE) {
            mostrar_advertencia("No queda RCS para un pulso suave.");
        } else if (m == MANIOBRA_FUERTE && rcs < COSTO_FUERTE) {
            mostrar_advertencia("No alcanza el RCS para un pulso fuerte.");
        } else {
            return m;
        }
    }
}

static double pedir_direccion(double magnitud)
{
    puts("    p = prógrado  (aplana la trayectoria, gamma baja)");
    puts("    r = retrógrado (empina la trayectoria, gamma sube)");
    if (leer_caracter("  Dirección del pulso", "pr") == 'p') {
        return -magnitud;
    }
    return magnitud;
}

static EstadoAngulo minijuego_alineacion_rcs(const Dificultad *dif, Reporte *rep)
{
    printf("Telemetría: la nave se aproxima con gamma = %.2f° y giro omega = %+.2f°/turno.\n",
           rep->gamma, rep->omega);
    printf("Objetivo en t = 0: ángulo entre %.1f° y %.1f° Y giro estabilizado (|omega| <= %.2f°/t).\n",
           GAMMA_MIN, GAMMA_MAX, OMEGA_MAX_SEGURO);

    while (rep->num_turnos < dif->turnos_rcs) {
        int t = dif->turnos_rcs - rep->num_turnos;
        int rcs = dif->combustible_rcs - rep->rcs_usado;
        TipoManiobra m = MANIOBRA_MANTENER;
        int dir = 0;

        printf("\n----------- T-%d turno%s para el borde atmosférico -----------\n",
               t, t == 1 ? "" : "s");
        printf("  Ángulo actual gamma : %.2f°  |  Velocidad de giro (omega): %+.2f°/turno\n",
               rep->gamma, rep->omega);
        printf("  [RADAR] Anomalía detectada para este turno: %+.2f°/turno\n",
               rep->deriva_proxima);
        printf("  Proyección sin tocar propulsores -> omega: %+.2f°/t | gamma: %.2f°\n",
               rep->omega + rep->deriva_proxima,
               rep->gamma + rep->omega + rep->deriva_proxima);
        printf("  Combustible RCS     : %d / %d u.%s\n", rcs, dif->combustible_rcs,
               rep->rcs_recalentado ? "  [!] TOBERAS CALIENTES (Pulso fuerte bloqueado)" : "");
        mostrar_indicador_gamma(rep->gamma);

        if (rcs == 0) {
            puts("\n  Sin combustible RCS: la nave mantiene actitud por inercia.");
        } else {
            while (1) {
                m = elegir_maniobra(rcs);
                if (m == MANIOBRA_FUERTE && rep->rcs_recalentado) {
                    mostrar_advertencia("Toberas recalentadas por el turno anterior: usá pulso suave o mantener.");
                } else {
                    break;
                }
            }
            if (m != MANIOBRA_MANTENER) {
                dir = (pedir_direccion(1.0) < 0.0) ? -1 : +1;
            }
        }

        aplicar_turno_rcs(rep, m, dir);
        const TurnoRCS *ult = &rep->bitacora[rep->num_turnos - 1];
        printf("\n  %s (%+.2f°/t) + Radar (%+.2f°/t) -> Giro omega = %+.2f°/t | Nuevo gamma = %.2f°\n",
               NOMBRES_MANIOBRAS[ult->maniobra], ult->delta_pulso,
               ult->deriva, ult->omega_resultante, ult->gamma_resultante);
    }

    printf("\n=========== t = 0: INTERFAZ ATMOSFÉRICA ===========\n");
    printf("  Ángulo final: %.2f°  |  Giro residual: %+.2f°/t  |  RCS usado: %d u.\n",
           rep->gamma, rep->omega, rep->rcs_usado);
    mostrar_indicador_gamma(rep->gamma);
    return clasificar_angulo(rep->gamma);
}

static void mostrar_bitacora_rcs(const Reporte *r)
{
    puts("\n  Bitácora de alineación RCS:");
    puts("    Turno  Maniobra            Pulso    Radar    Omega    Gamma    RCS");
    puts("    -------------------------------------------------------------------");
    printf("    inicio %-18s %7s  %7s  %7s  %6.2f°  %3d\n",
           "-", "-", "-", "-", r->gamma_inicial, r->dificultad->combustible_rcs);
    for (int i = 0; i < r->num_turnos; i++) {
        const TurnoRCS *tr = &r->bitacora[i];
        printf("    T-%-4d %-18s %+6.2f°  %+6.2f°  %+6.2f°  %6.2f°  %3d\n",
               tr->t, NOMBRES_MANIOBRAS[tr->maniobra], tr->delta_pulso,
               tr->deriva, tr->omega_resultante, tr->gamma_resultante, tr->rcs_restante);
    }
}

void ejecutar_mision(const DatosMision *d, const Dificultad *dif, Reporte *rep)
{
    EstadoAngulo estado;

    iniciar_reporte(rep, dif);

    /* ---------- EVENTO 1: alineación de reingreso con RCS ---------- */
    printf("\n===== EVENTO 1: %s =====\n", NOMBRES_EVENTOS[0]);
    estado = minijuego_alineacion_rcs(dif, rep);
    explicar_angulo(estado);

    if (estado == ANGULO_SEGURO) {
        puts("  -> Alineación lograda. EVENTO SUPERADO.");
        rep->eventos[0] = EV_SUPERADO;
    } else {
        puts("  -> La nave llegó a la atmósfera fuera de la ventana: SE PIERDE LA MISIÓN.");
        rep->eventos[0] = EV_FALLIDO;
    }
    pausar();

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
    puts("\n================ REGLAS DEL SIMULADOR ================");
    puts("Sos el director de vuelo del reingreso de la cápsula Orion (Artemis).");
    puts("La misión tiene 3 eventos y se juega por turnos:\n");

    puts("  1) Alineación de reingreso con propulsores RCS:");
    printf("     - Meta en t = 0: ángulo gamma entre %.1f° y %.1f° Y giro estabilizado\n",
           GAMMA_MIN, GAMMA_MAX);
    printf("       (|omega| <= %.2f°/turno). Si entra girando brusco, el escudo falla.\n",
           OMEGA_MAX_SEGURO);
    puts("     - Inercia (1.ª Ley de Newton): la cápsula conserva su velocidad de giro");
    puts("       (omega) de un turno al siguiente. Si acelerás el giro para corregir el");
    puts("       ángulo, deberás frenarlo con el propulsor opuesto antes de t = 0.");
    puts("     - Radar: antes de cada turno te informa la anomalía exacta que empujará");
    puts("       el giro de la nave para que puedas anticiparla.");
    puts("     - Propulsores y enfriamiento:");
    printf("         * Pulso suave  (±%.1f°/t, gasta %d RCS)\n", PULSO_SUAVE, COSTO_SUAVE);
    printf("         * Pulso fuerte (±%.1f°/t, gasta %d RCS - recalienta toberas por 1 turno)\n",
           PULSO_FUERTE, COSTO_FUERTE);
    puts("         * Mantener actitud (0°/t, gasta 0 RCS y enfría toberas)");
    puts("       Prógrado (-) aplana la trayectoria (baja gamma); retrógrado (+) la empina.\n");

    puts("  2) Frenado aerodinámico: se calcula el trabajo del rozamiento y la");
    puts("     desaceleración media. Si supera el límite de g, la tripulación no resiste.\n");

    puts("  3) Paracaídas y amerizaje: se calcula la fuerza promedio del agua.");
    puts("     Si supera el límite, la estructura falla.\n");

    printf("Puntaje: %d por evento (-%d por cada unidad de RCS usada en el Evento 1)\n",
           PUNTOS_EVENTO, PENALIZACION_RCS);
    printf("+ %d de bonus de precisión si completás todo con gamma a ±%.2f° de %.2f°.\n",
           BONUS_PRECISION, TOLERANCIA_BONUS, GAMMA_CENTRO);
    printf("Máximo: %d puntos.\n", NUM_EVENTOS * PUNTOS_EVENTO + BONUS_PRECISION);
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
    printf("Ángulo inicial       : %.2f°\n", r->gamma_inicial);
    printf("Ángulo en t = 0      : %.2f°\n", r->gamma);
    printf("RCS usado            : %d de %d u.\n", r->rcs_usado, r->dificultad->combustible_rcs);
    mostrar_bitacora_rcs(r);
    putchar('\n');

    for (int i = 0; i < NUM_EVENTOS; i++) {
        printf("Evento %d (%s): %s\n", i + 1, NOMBRES_EVENTOS[i], nombre_estado_evento(r->eventos[i]));
    }

    if (r->eventos[1] != EV_PENDIENTE) {
        printf("Trabajo del rozamiento: %.4e J\n", r->trabajo_roz);
        printf("Desaceleración media  : %.2f g\n", r->desaceleracion_g);
    }
    if (r->eventos[2] != EV_PENDIENTE) {
        printf("Fuerza en amerizaje   : %.1f N (%.2fx el peso)\n", r->fuerza_impacto, r->fuerza_en_pesos);
    }

    printf("\nPUNTAJE: %d / %d\n", r->puntaje,
           NUM_EVENTOS * PUNTOS_EVENTO + BONUS_PRECISION);
    puts(r->exito ? "RESULTADO: ¡MISIÓN EXITOSA! La tripulación está a salvo."
                  : "RESULTADO: MISIÓN FALLIDA.");
}