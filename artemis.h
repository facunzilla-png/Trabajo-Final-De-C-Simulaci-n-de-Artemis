/*
 * artemis.h
 * Situación-Problema 4: Reingreso atmosférico, frenado aerodinámico y amerizaje.
 * Contiene: constantes, tipos de datos y prototipos de funciones públicas.
 *
 * Las funciones de la sección "LÓGICA DE LA MISIÓN" NO imprimen ni leen teclado:
 * reciben datos y devuelven resultados. Por eso las usan tanto la versión de
 * consola como la interfaz gráfica.
 */
#ifndef ARTEMIS_H
#define ARTEMIS_H

/* ===================== CONSTANTES FÍSICAS ===================== */
#define G_TERRESTRE        9.80      /* m/s^2 */

/* ============ DATOS POR DEFECTO (tomados del enunciado) ============ */
#define MASA_CAPSULA       10400.0   /* kg                              */
#define V_REINGRESO        11000.0   /* m/s  (vi)                       */
#define H_REINGRESO        120000.0  /* m                               */
#define V_PARACAIDAS       25.0      /* m/s  (vf al abrir paracaídas)   */
#define H_PARACAIDAS       2500.0    /* m                               */
#define V_IMPACTO          8.0       /* m/s  (amerizaje)                */
#define DT_FRENADO         400.0     /* s    (fase de mayor desaceleración) */
#define D_HUNDIMIENTO      1.50      /* m    (profundidad de hundimiento)   */

/* ====================== REGLAS DEL JUEGO ====================== */
#define GAMMA_MIN          5.5       /* grados: por debajo -> rebote    */
#define GAMMA_MAX          7.0       /* grados: por encima -> excesivo  */
#define NUM_PASOS_FRENADO  8         /* filas de la tabla de frenado    */
#define TAM_TABLA          (NUM_PASOS_FRENADO + 1)  /* incluye t = 0    */
#define NUM_EVENTOS        3
#define NUM_DIFICULTADES   3
#define PUNTOS_EVENTO      300
#define PENALIZACION_CORR  100
#define BONUS_SIN_CORREC   100
#define TAM_MENSAJE        128       /* largo máximo de un mensaje de error */

/* ========================== TIPOS DE DATOS ========================== */

/* Datos de entrada de la misión (editables por el usuario). */
typedef struct {
    double masa;            /* kg */
    double v_inicial;       /* m/s */
    double h_inicial;       /* m */
    double v_paracaidas;    /* m/s */
    double h_paracaidas;    /* m */
    double v_impacto;       /* m/s */
    double dt_frenado;      /* s */
    double d_hundimiento;   /* m */
} DatosMision;

/* Resultado de evaluar el ángulo de reingreso. */
typedef enum {
    ANGULO_REBOTE,
    ANGULO_SEGURO,
    ANGULO_EXCESIVO
} EstadoAngulo;

/* Estado de cada evento de la misión. */
typedef enum {
    EV_PENDIENTE,           /* todavía no se jugó */
    EV_SUPERADO,
    EV_FALLIDO
} EstadoEvento;

/* Parámetros que cambian según la dificultad elegida. */
typedef struct {
    const char *nombre;
    double limite_g_frenado;    /* g máximos tolerables en el frenado    */
    double limite_g_impacto;    /* fuerza máx. de amerizaje (x el peso)  */
    int    correcciones;        /* correcciones de trayectoria (RCS)     */
} Dificultad;

/* Una fila de la tabla de frenado. */
typedef struct {
    double tiempo;              /* s   */
    double velocidad;           /* m/s */
    double energia_cinetica;    /* J   */
} PasoFrenado;

/* Todo lo que ocurrió en una misión (se usa para el reporte final). */
typedef struct {
    const Dificultad *dificultad;
    double gamma;                       /* grados */
    int    correcciones_usadas;
    EstadoEvento eventos[NUM_EVENTOS];

    double energia_inicial;             /* J */
    double energia_final;               /* J */
    double trabajo_roz;                 /* J */
    double desaceleracion;              /* m/s^2 */
    double desaceleracion_g;            /* múltiplos de g */
    PasoFrenado tabla[TAM_TABLA];

    double fuerza_impacto;              /* N */
    double fuerza_en_pesos;             /* múltiplos del peso */

    int puntaje;
    int exito;
} Reporte;

/* ======================= FUNCIONES DE FÍSICA ======================= */
double energia_cinetica(double masa, double velocidad);
double energia_potencial(double masa, double altura);
double energia_mecanica(double masa, double velocidad, double altura);

double calcular_trabajo_rozamiento(const DatosMision *d);
double calcular_desaceleracion_media(const DatosMision *d);
double calcular_fuerza_amerizaje(const DatosMision *d);
EstadoAngulo clasificar_angulo(double gamma);
void generar_tabla_frenado(const DatosMision *d, PasoFrenado tabla[], int n);

/* ============ LÓGICA DE LA MISIÓN (sin printf ni teclado) ============ */
const Dificultad *obtener_dificultad(int indice);      /* 0 .. NUM_DIFICULTADES-1 */
const char *nombre_evento(int indice);                 /* 0 .. NUM_EVENTOS-1      */
const char *nombre_estado_evento(EstadoEvento estado);

/* Devuelve 1 si los datos son coherentes entre sí. Si no, devuelve 0 y deja
 * el motivo en 'mensaje' (en vez de imprimirlo). */
int datos_coherentes(const DatosMision *d, char *mensaje, int tam);

void iniciar_reporte(Reporte *rep, const Dificultad *dif);

/* Evento 1: evalúa el ángulo. Actualiza rep->gamma, las correcciones usadas y
 * rep->eventos[0] (SUPERADO si es seguro, FALLIDO si no quedan correcciones). */
EstadoAngulo resolver_angulo(Reporte *rep, const Dificultad *dif, double gamma);

/* Evento 2: calcula trabajo, desaceleración y tabla. Marca rep->eventos[1]. */
void resolver_frenado(const DatosMision *d, const Dificultad *dif, Reporte *rep);

/* Evento 3: calcula la fuerza de amerizaje. Marca rep->eventos[2]. */
void resolver_amerizaje(const DatosMision *d, const Dificultad *dif, Reporte *rep);

/* Calcula puntaje y resultado final (rep->puntaje, rep->exito). */
void calcular_puntaje(Reporte *rep);

/* ================= VERSIÓN DE CONSOLA (usa printf y teclado) ================= */
void cargar_datos_por_defecto(DatosMision *d);
void ingresar_datos_mision(DatosMision *d);
const Dificultad *seleccionar_dificultad(void);
void ejecutar_mision(const DatosMision *d, const Dificultad *dif, Reporte *rep);
void mostrar_reglas(void);
void mostrar_datos(const DatosMision *d);
void mostrar_analisis(const DatosMision *d, double gamma);
void mostrar_reporte(const Reporte *rep);

#endif /* ARTEMIS_H */
