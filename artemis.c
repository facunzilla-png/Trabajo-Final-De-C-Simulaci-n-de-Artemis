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
        rep->rcs_recalentado = 1; 
    } else {
        m = MANIOBRA_MANTENER;
        rep->rcs_recalentado = 0;
    }

    rep->rcs_usado += costo;
    turno->t           = dif->turnos_rcs - rep->num_turnos;
    turno->maniobra    = m;
    turno->delta_pulso = delta;
    turno->deriva      = rep->deriva_proxima;

    
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

    
    rep->deriva_proxima = round(generar_deriva(dif->deriva_max) * 100.0) / 100.0;

    if (rep->num_turnos >= dif->turnos_rcs) {
        int angulo_ok = (clasificar_angulo(rep->gamma) == ANGULO_SEGURO);
        int giro_ok   = (fabs(rep->omega) <= OMEGA_MAX_SEGURO);
        rep->eventos[0] = (angulo_ok && giro_ok) ? EV_SUPERADO : EV_FALLIDO;
    }
}

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

    
    rep->densidad_proxima = round((0.85 + ((double)rand() / RAND_MAX) * 0.30) * 100.0) / 100.0;
}


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

    
    double a_fase = a_base * f_perfil * rep->densidad_proxima;
    double g_fase = a_fase / G_TERRESTRE;

    double v_ant   = rep->tabla[rep->fase_frenado].velocidad;
    double v_nueva = v_ant - a_fase * dt_fase;
    if (v_nueva < d->v_paracaidas * 0.5) {
        v_nueva = d->v_paracaidas * 0.5;
    }

  
    rep->calor_escudo += d_calor * rep->densidad_proxima;
    if (rep->calor_escudo < 10.0) {
        rep->calor_escudo = 10.0;
    }
    if (rep->calor_escudo > rep->calor_max_alcanzado) {
        rep->calor_max_alcanzado = rep->calor_escudo;
    }

    
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

    
    double h_actual = d->h_inicial - (d->h_inicial - d->h_paracaidas)
                    * ((double)rep->fase_frenado / FASES_FRENADO);
    rep->energia_final = energia_mecanica(d->masa, v_nueva, h_actual);
    rep->trabajo_roz   = rep->energia_final - rep->energia_inicial;

    
    rep->densidad_proxima = round((0.85 + ((double)rand() / RAND_MAX) * 0.30) * 100.0) / 100.0;

   
    if (g_fase > dif->limite_g_frenado) {
        rep->eventos[1] = EV_FALLIDO;
        rep->motivo_fallo_frenado = 1;
        return;
    }
    if (rep->calor_escudo > CALOR_MAXIMO) {
        rep->eventos[1] = EV_FALLIDO;
        rep->motivo_fallo_frenado = 2;
        return;
    }

    
    if (rep->fase_frenado >= FASES_FRENADO) {
        double v_max_parac = d->v_paracaidas + MARGEN_VEL_PARAC;
        if (v_nueva <= v_max_parac) {
            rep->eventos[1] = EV_SUPERADO;
        } else {
            rep->eventos[1] = EV_FALLIDO;
            rep->motivo_fallo_frenado = 3; 
        }
    }
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

    
    rep->factor_viento = round((0.85 + ((double)rand() / RAND_MAX) * 0.35) * 100.0) / 100.0;
}


void elegir_apertura_paracaidas(const DatosMision *d, Reporte *rep, int opcion)
{
    if (rep->etapa_amerizaje != 0 || rep->eventos[2] != EV_PENDIENTE) {
        return;
    }

    rep->opcion_paracaidas = opcion;
    double v_entrada = (rep->fase_frenado > 0)
                     ? rep->tabla[rep->fase_frenado].velocidad
                     : d->v_paracaidas;

    
    if (opcion == 1 && (rep->factor_viento > 1.08 || v_entrada > d->v_paracaidas + 35.0)) {
        rep->eventos[2] = EV_FALLIDO;
        rep->motivo_fallo_amerizaje = 1; 
        rep->etapa_amerizaje = 2;
        return;
    }

    double f_vel = (opcion == 1) ? FACTOR_VEL_TEMPRANA
                 : (opcion == 3) ? FACTOR_VEL_TARDIA
                                 : FACTOR_VEL_NOMINAL;

    rep->v_impacto_real  = d->v_impacto * f_vel * (0.90 + 0.10 * rep->factor_viento);
    rep->etapa_amerizaje = 1; 
}


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

    
    if (opcion == 1 && rep->factor_viento > 1.12) {
        f_hund = 0.95;
    }

    rep->d_hundimiento_real = d->d_hundimiento * f_hund;


    double v2 = rep->v_impacto_real * rep->v_impacto_real;
    rep->fuerza_impacto  = d->masa * (G_TERRESTRE + v2 / (2.0 * rep->d_hundimiento_real));
    rep->fuerza_en_pesos = rep->fuerza_impacto / (d->masa * G_TERRESTRE);
    rep->etapa_amerizaje = 2;

    if (rep->fuerza_en_pesos <= dif->limite_g_impacto) {
        rep->eventos[2] = EV_SUPERADO;
    } else {
        rep->eventos[2] = EV_FALLIDO;
        rep->motivo_fallo_amerizaje = 2; 
    }
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
