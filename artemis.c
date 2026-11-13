<<<<<<< Updated upstream

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
=======

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
static const char *NOMBRES_PERFILES[] = {
    "",
    "Sustentación Alta",
    "Perfil Nominal",
    "Inmersión Densa"
};

const char *nombre_perfil_frenado(PerfilFrenado p)
{
    if (p < PERFIL_SUST_ALTA || p > PERFIL_INM_DENSA) {
        return "-";
    }
    return NOMBRES_PERFILES[p];
}
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
/* Prepara el estado inicial (t = 0) antes de jugar las 4 fases del Evento 2. */
void iniciar_frenado(const DatosMision *d, Reporte *rep)
{
    rep->energia_inicial      = energia_mecanica(d->masa, d->v_inicial, d->h_inicial);
    rep->energia_final        = rep->energia_inicial;
    rep->trabajo_roz          = 0.0;
    rep->desaceleracion       = 0.0;
    rep->desaceleracion_g     = 0.0;
    rep->calor_escudo         = CALOR_INICIAL;
    rep->calor_max_alcanzado  = CALOR_INICIAL;
    rep->fase_frenado         = 0;
    rep->motivo_fallo_frenado = 0;

    rep->tabla[0].tiempo           = 0.0;
    rep->tabla[0].velocidad        = d->v_inicial;
    rep->tabla[0].energia_cinetica = energia_cinetica(d->masa, d->v_inicial);
    rep->tabla[0].g_fase           = 0.0;
    rep->tabla[0].calor_escudo     = CALOR_INICIAL;
    rep->tabla[0].perfil           = 0;

    /* Variación atmosférica de ±15% para la primera fase */
    rep->densidad_proxima = round((0.85 + ((double)rand() / RAND_MAX) * 0.30) * 100.0) / 100.0;
}

/* Ejecuta una fase de 100 s del frenado aerodinámico. */
void aplicar_turno_frenado(const DatosMision *d, const Dificultad *dif,
                           Reporte *rep, PerfilFrenado p)
{
    if (rep->fase_frenado >= FASES_FRENADO || rep->eventos[1] != EV_PENDIENTE) {
        return;
    }

    double dt_fase = d->dt_frenado / FASES_FRENADO;
    double a_base  = calcular_desaceleracion_media(d);
    double f_perfil = (p == PERFIL_SUST_ALTA) ? FACTOR_SUST_ALTA
                    : (p == PERFIL_INM_DENSA) ? FACTOR_INM_DENSA
                                              : FACTOR_NOMINAL;
    double d_calor  = (p == PERFIL_SUST_ALTA) ? CALOR_SUST_ALTA
                    : (p == PERFIL_INM_DENSA) ? CALOR_INM_DENSA
                                              : CALOR_NOMINAL;

    /* Desaceleración real del tramo según perfil elegido y densidad de la capa */
    double a_fase = a_base * f_perfil * rep->densidad_proxima;
    double g_fase = a_fase / G_TERRESTRE;

    double v_ant   = rep->tabla[rep->fase_frenado].velocidad;
    double v_nueva = v_ant - a_fase * dt_fase;
    if (v_nueva < d->v_paracaidas * 0.5) {
        v_nueva = d->v_paracaidas * 0.5;
    }

    /* Actualización térmica del escudo */
    rep->calor_escudo += d_calor * rep->densidad_proxima;
    if (rep->calor_escudo < 10.0) {
        rep->calor_escudo = 10.0;
    }
    if (rep->calor_escudo > rep->calor_max_alcanzado) {
        rep->calor_max_alcanzado = rep->calor_escudo;
    }

    /* Registro del pico máximo de fuerzas g soportado por la tripulación */
    if (g_fase > rep->desaceleracion_g) {
        rep->desaceleracion_g = g_fase;
        rep->desaceleracion   = a_fase;
    }

    rep->fase_frenado++;
    PasoFrenado *paso      = &rep->tabla[rep->fase_frenado];
    paso->tiempo           = dt_fase * rep->fase_frenado;
    paso->velocidad        = v_nueva;
    paso->energia_cinetica = energia_cinetica(d->masa, v_nueva);
    paso->g_fase           = g_fase;
    paso->calor_escudo     = rep->calor_escudo;
    paso->perfil           = p;

    /* Altura interpolada para actualizar el trabajo de rozamiento acumulado */
    double h_actual = d->h_inicial - (d->h_inicial - d->h_paracaidas)
                    * ((double)rep->fase_frenado / FASES_FRENADO);
    rep->energia_final = energia_mecanica(d->masa, v_nueva, h_actual);
    rep->trabajo_roz   = rep->energia_final - rep->energia_inicial;

    /* Pronóstico de densidad para la próxima capa */
    rep->densidad_proxima = round((0.85 + ((double)rand() / RAND_MAX) * 0.30) * 100.0) / 100.0;

    /* Verificación de fallo inmediato por g o por escudo fundido */
    if (g_fase > dif->limite_g_frenado) {
        rep->eventos[1] = EV_FALLIDO;
        rep->motivo_fallo_frenado = 1; /* Exceso de fuerzas g */
        return;
    }
    if (rep->calor_escudo > CALOR_MAXIMO) {
        rep->eventos[1] = EV_FALLIDO;
        rep->motivo_fallo_frenado = 2; /* Escudo térmico colapsado */
        return;
    }

    /* Al completar las 4 fases, verificar si frenó lo suficiente para abrir paracaídas */
    if (rep->fase_frenado >= FASES_FRENADO) {
        double v_max_parac = d->v_paracaidas + MARGEN_VEL_PARAC;
        if (v_nueva <= v_max_parac) {
            rep->eventos[1] = EV_SUPERADO;
        } else {
            rep->eventos[1] = EV_FALLIDO;
            rep->motivo_fallo_frenado = 3; /* Exceso de velocidad para paracaídas */
        }
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
const char *nombre_opcion_paracaidas(int op)
{
    switch (op) {
    case 1:  return "Temprana (Alta cota)";
    case 2:  return "Nominal (2500 m)";
    case 3:  return "Tardía (Baja cota)";
    default: return "-";
    }
}

const char *nombre_opcion_actitud(int op)
{
    switch (op) {
    case 1:  return "En Cuña (Amortiguada)";
    case 2:  return "Nominal (Estándar)";
    case 3:  return "Plana (Frenado seco)";
    default: return "-";
    }
}

void iniciar_amerizaje(const DatosMision *d, Reporte *rep)
{
    rep->etapa_amerizaje        = 0;
    rep->opcion_paracaidas      = 0;
    rep->opcion_actitud_agua    = 0;
    rep->v_impacto_real         = d->v_impacto;
    rep->d_hundimiento_real     = d->d_hundimiento;
    rep->fuerza_impacto         = 0.0;
    rep->fuerza_en_pesos        = 0.0;
    rep->motivo_fallo_amerizaje = 0;

    /* Condición meteorológica (ráfagas en altura y oleaje en el mar: 0.85 a 1.20) */
    rep->factor_viento = round((0.85 + ((double)rand() / RAND_MAX) * 0.35) * 100.0) / 100.0;
}

/* Decisión 1: Apertura de paracaídas. Calcula v_impacto_real o falla si se rompen. */
void elegir_apertura_paracaidas(const DatosMision *d, Reporte *rep, int opcion)
{
    if (rep->etapa_amerizaje != 0 || rep->eventos[2] != EV_PENDIENTE) {
        return;
    }

    rep->opcion_paracaidas = opcion;
    double v_entrada = (rep->fase_frenado > 0)
                     ? rep->tabla[rep->fase_frenado].velocidad
                     : d->v_paracaidas;

    /* Si abre temprano con ráfagas fuertes (> 1.08) o viniendo muy rápido, el tirón corta las cuerdas */
    if (opcion == 1 && (rep->factor_viento > 1.08 || v_entrada > d->v_paracaidas + 35.0)) {
        rep->eventos[2] = EV_FALLIDO;
        rep->motivo_fallo_amerizaje = 1; /* Desgarro de paracaídas por tirón excesivo */
        rep->etapa_amerizaje = 2;
        return;
    }

    double f_vel = (opcion == 1) ? FACTOR_VEL_TEMPRANA
                 : (opcion == 3) ? FACTOR_VEL_TARDIA
                                 : FACTOR_VEL_NOMINAL;

    rep->v_impacto_real  = d->v_impacto * f_vel * (0.90 + 0.10 * rep->factor_viento);
    rep->etapa_amerizaje = 1; /* Pasa a la Decisión 2 */
}

/* Decisión 2: Actitud de entrada al agua. Calcula d_hundimiento_real y la fuerza F. */
void elegir_actitud_impacto(const DatosMision *d, const Dificultad *dif,
                            Reporte *rep, int opcion)
{
    if (rep->etapa_amerizaje != 1 || rep->eventos[2] != EV_PENDIENTE) {
        return;
    }

    rep->opcion_actitud_agua = opcion;
    double f_hund = (opcion == 1) ? FACTOR_HUND_CUNA
                  : (opcion == 3) ? FACTOR_HUND_PLANA
                                  : FACTOR_HUND_NOMINAL;

    /* Si el mar está muy picado (> 1.12) y entra en cuña agresiva, la ola reduce la ventaja */
    if (opcion == 1 && rep->factor_viento > 1.12) {
        f_hund = 0.95;
    }

    rep->d_hundimiento_real = d->d_hundimiento * f_hund;

    /* Teorema trabajo-energía con la velocidad y hundimiento reales: F = m(g + v^2 / 2d) */
    double v2 = rep->v_impacto_real * rep->v_impacto_real;
    rep->fuerza_impacto  = d->masa * (G_TERRESTRE + v2 / (2.0 * rep->d_hundimiento_real));
    rep->fuerza_en_pesos = rep->fuerza_impacto / (d->masa * G_TERRESTRE);
    rep->etapa_amerizaje = 2;

    if (rep->fuerza_en_pesos <= dif->limite_g_impacto) {
        rep->eventos[2] = EV_SUPERADO;
    } else {
        rep->eventos[2] = EV_FALLIDO;
        rep->motivo_fallo_amerizaje = 2; /* La estructura no soporta la fuerza del agua */
    }
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
    puts("\n   Fase   t [s]    v [m/s]      Ec [J]       Pico g   Escudo   Maniobra");
    puts("  ------------------------------------------------------------------------------");
    for (int i = 0; i < n; i++) {
        printf("    %d   %6.0f   %8.1f   %11.3e   %5.2f g   %5.1f%%   %s\n",
               i, tabla[i].tiempo, tabla[i].velocidad, tabla[i].energia_cinetica,
               tabla[i].g_fase, tabla[i].calor_escudo,
               (i == 0) ? "(Entrada)" : nombre_perfil_frenado(tabla[i].perfil));
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

    /* ---------- EVENTO 2: frenado aerodinámico por fases ---------- */
    printf("\n===== EVENTO 2: %s =====\n", NOMBRES_EVENTOS[1]);
    iniciar_frenado(d, rep);
    double v_meta = d->v_paracaidas + MARGEN_VEL_PARAC;

    printf("Meta en t = %.0f s: bajar velocidad a <= %.0f m/s sin superar %.1f g ni 100%% de calor.\n",
           d->dt_frenado, v_meta, dif->limite_g_frenado);

    while (rep->eventos[1] == EV_PENDIENTE && rep->fase_frenado < FASES_FRENADO) {
        int fase_act = rep->fase_frenado + 1;
        double v_act = rep->tabla[rep->fase_frenado].velocidad;
        double g_nom = (calcular_desaceleracion_media(d) * rep->densidad_proxima) / G_TERRESTRE;

        printf("\n----------- Fase %d de %d (t = %.0f s a %.0f s) -----------\n",
               fase_act, FASES_FRENADO,
               rep->tabla[rep->fase_frenado].tiempo,
               rep->tabla[rep->fase_frenado].tiempo + d->dt_frenado / FASES_FRENADO);
        printf("  Velocidad actual: %.1f m/s  |  Carga térmica escudo: %.1f%% / 100%%\n",
               v_act, rep->calor_escudo);
        printf("  [RADAR ATMOSFÉRICO] Densidad de la capa: %.0f%% (G estimado en Nominal: %.2f g / máx %.1f g)\n",
               rep->densidad_proxima * 100.0, g_nom, dif->limite_g_frenado);

        puts("\n  Perfiles de sustentación (rotación de cápsula):");
        printf("    1) Sustentación Alta (frena 0.78x | ~%.2f g | enfría escudo)\n", g_nom * FACTOR_SUST_ALTA);
        printf("    2) Perfil Nominal    (frena 1.00x | ~%.2f g | calor moderado)\n", g_nom * FACTOR_NOMINAL);
        printf("    3) Inmersión Densa   (frena 1.25x | ~%.2f g | calienta escudo)\n", g_nom * FACTOR_INM_DENSA);

        PerfilFrenado p = (PerfilFrenado)leer_entero("  Elegí el perfil para esta fase", 1, 3);
        aplicar_turno_frenado(d, dif, rep, p);

        const PasoFrenado *ult = &rep->tabla[rep->fase_frenado];
        printf("\n  -> Fin Fase %d: v = %.1f m/s | Pico = %.2f g | Escudo = %.1f%%\n",
               rep->fase_frenado, ult->velocidad, ult->g_fase, ult->calor_escudo);
    }

    mostrar_tabla_frenado(rep->tabla, rep->fase_frenado + 1);
    printf("\n  Trabajo del rozamiento acumulado: %.4e J\n", rep->trabajo_roz);
    printf("  Pico máximo de desaceleración   : %.2f g (límite %.1f g)\n",
           rep->desaceleracion_g, dif->limite_g_frenado);

    if (rep->eventos[1] == EV_SUPERADO) {
        puts("  -> Frenado completado y velocidad segura para paracaídas. EVENTO SUPERADO.");
        pausar();
    } else {
        if (rep->motivo_fallo_frenado == 1) {
            puts("  -> Carga de g excesiva para la tripulación. EVENTO FALLIDO.");
        } else if (rep->motivo_fallo_frenado == 2) {
            puts("  -> El escudo térmico superó el 100% de su capacidad. EVENTO FALLIDO.");
        } else {
            puts("  -> Velocidad final demasiado alta: los paracaídas se desgarran. EVENTO FALLIDO.");
        }
        calcular_puntaje(rep);
        return;
    }

    /* ---------- EVENTO 3: paracaídas y amerizaje ---------- */
    printf("\n===== EVENTO 3: %s =====\n", NOMBRES_EVENTOS[2]);
    iniciar_amerizaje(d, rep);

    printf("  [METEOROLOGÍA] Ráfagas y oleaje en zona de amerizaje: %.0f%% de intensidad.\n",
           rep->factor_viento * 100.0);
    puts("\n  Decisión 1 - Altitud de despliegue de paracaídas:");
    puts("    1) Apertura Temprana (frena mejor: 0.92x vel, pero se rompe si hay ráfagas > 108%)");
    printf("    2) Apertura Nominal  (a %.0f m: apunta a %.1f m/s estándar)\n", d->h_paracaidas, d->v_impacto);
    puts("    3) Apertura Tardía   (segura ante ráfagas, pero golpea el agua 1.45x más rápido)");

    int op_par = leer_entero("  Elegí el despliegue de paracaídas", 1, 3);
    elegir_apertura_paracaidas(d, rep, op_par);

    if (rep->eventos[2] == EV_FALLIDO) {
        puts("\n  -> ¡El tirón en aire turbulento desgarró los paracaídas! EVENTO FALLIDO.");
        calcular_puntaje(rep);
        return;
    }

    printf("\n  -> Paracaídas desplegados. Velocidad de descenso estabilizada en: %.2f m/s.\n",
           rep->v_impacto_real);
    puts("\n  Decisión 2 - Ángulo de entrada al agua (modifica el hundimiento d en F = m(g + v^2/2d)):");
    printf("    1) Entrada en Cuña  (amortigua: d = %.2f m, salvo oleaje extremo > 112%%)\n",
           d->d_hundimiento * FACTOR_HUND_CUNA);
    printf("    2) Entrada Nominal  (estándar : d = %.2f m)\n", d->d_hundimiento);
    printf("    3) Entrada Plana    (en seco  : d = %.2f m, duplica el pico de fuerza)\n",
           d->d_hundimiento * FACTOR_HUND_PLANA);

    int op_act = leer_entero("  Elegí la actitud de impacto", 1, 3);
    elegir_actitud_impacto(d, dif, rep, op_act);

    printf("\n  Impacto a %.2f m/s con hundimiento de %.2f m.\n",
           rep->v_impacto_real, rep->d_hundimiento_real);
    printf("  Fuerza promedio del agua: %.1f N = %.2f veces el peso (límite %.1fx)\n",
           rep->fuerza_impacto, rep->fuerza_en_pesos, dif->limite_g_impacto);

    if (rep->eventos[2] == EV_SUPERADO) {
        puts("  -> La estructura resiste el amerizaje. EVENTO SUPERADO.");
    } else {
        puts("  -> La estructura no resiste el impacto contra el agua. EVENTO FALLIDO.");
    }

    calcular_puntaje(rep);
}
void mostrar_reglas(void)
{
    puts("\n================ REGLAS DEL SIMULADOR ================");
    puts("Sos el director de vuelo del reingreso de la cápsula Orion (Artemis).");
    puts("La misión consta de 3 eventos interactivos por turnos:\n");

    puts("  1) EVENTO 1 - Alineación de reingreso con propulsores RCS:");
    printf("     - Meta en t = 0: llegar con ángulo gamma entre %.1f° y %.1f° Y giro\n",
           GAMMA_MIN, GAMMA_MAX);
    printf("       estabilizado (|omega| <= %.2f°/turno).\n", OMEGA_MAX_SEGURO);
    puts("     - Inercia (1.ª Ley de Newton): la cápsula conserva su velocidad de giro");
    puts("       (omega) entre turnos. Si acelerás el giro para corregir el ángulo,");
    puts("       tenés que frenarlo con el propulsor opuesto antes de tocar la atmósfera.");
    puts("     - Radar y Propulsores: el radar anticipa la deriva del turno. Elegís entre:");
    printf("         * Pulso suave  (±%.1f°/t, gasta %d RCS)\n", PULSO_SUAVE, COSTO_SUAVE);
    printf("         * Pulso fuerte (±%.1f°/t, gasta %d RCS - recalienta toberas por 1 turno)\n",
           PULSO_FUERTE, COSTO_FUERTE);
    puts("         * Mantener actitud (0°/t, gasta 0 RCS y enfría toberas)");
    puts("       Prógrado (-) aplana la trayectoria (baja gamma); retrógrado (+) la empina.\n");

    puts("  2) EVENTO 2 - Frenado aerodinámico (4 fases):");
    puts("     - En cada fase el radar informa la densidad de la capa atmosférica.");
    puts("     - Elegís la rotación del escudo térmico (bank angle):");
    puts("         * Sustentación Alta: sube a aire tenue (frena 0.78x, baja g, enfría escudo).");
    puts("         * Perfil Nominal   : trayectoria estándar (frena 1.00x, calor moderado).");
    puts("         * Inmersión Densa  : baja a aire denso (frena 1.25x, sube g y calor).");
    puts("     - Meta: llegar al final de la fase 4 con velocidad segura para los paracaídas");
    puts("       sin superar el límite de fuerzas g de la tripulación ni el 100% de calor.\n");

    puts("  3) EVENTO 3 - Paracaídas y amerizaje (2 decisiones críticas):");
    puts("     - Decisión 1 (Despliegue de paracaídas): modifica la velocidad de impacto v.");
    puts("       Temprana frena mejor (0.92x) pero puede desgarrarse si hay ráfagas fuertes;");
    puts("       Nominal mantiene el diseño; Tardía es segura ante ráfagas pero golpea a 1.45x.");
    puts("     - Decisión 2 (Actitud de impacto en el mar): modifica el hundimiento d.");
    puts("       En Cuña amortigua (d = 1.20x, salvo mar muy picado); Nominal usa d estándar;");
    puts("       Plana frena en seco (d = 0.60x) aumentando el pico de fuerza.");
    puts("     - Meta: que la fuerza promedio del agua F = m(g + v^2/2d) no supere el límite.\n");

    printf("Puntaje: %d pts por evento superado (-%d pts por cada unidad de RCS usada)\n",
           PUNTOS_EVENTO, PENALIZACION_RCS);
    printf("+ %d pts de bonus de precisión si completás todo con gamma a ±%.2f° de %.2f°.\n",
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
>>>>>>> Stashed changes
}