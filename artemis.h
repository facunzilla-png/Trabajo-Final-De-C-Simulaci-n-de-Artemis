<<<<<<< Updated upstream
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
#define GAMMA_CENTRO       ((GAMMA_MIN + GAMMA_MAX) / 2.0)
#define NUM_PASOS_FRENADO  8         /* filas de la tabla de frenado    */
#define TAM_TABLA          (NUM_PASOS_FRENADO + 1)  /* incluye t = 0    */
#define NUM_EVENTOS        3
#define NUM_DIFICULTADES   3
#define PUNTOS_EVENTO      300
#define PENALIZACION_RCS   25        /* puntos por cada unidad de RCS gastada */
#define BONUS_PRECISION    100       /* misión exitosa y gamma casi centrado  */
#define TOLERANCIA_BONUS   0.25      /* grados respecto de GAMMA_CENTRO       */
#define TAM_MENSAJE        128       /* largo máximo de un mensaje de error   */

/* ========== MINIJUEGO EVENTO 1: ALINEACIÓN CON PROPULSORES RCS ========== */
#define MAX_TURNOS_RCS     5         /* tamaño máximo de la bitácora          */
#define GAMMA_INI_BAJO_MIN 3.6       /* desvío inicial por debajo (rebote)    */
#define GAMMA_INI_BAJO_MAX 4.7
#define GAMMA_INI_ALTO_MIN 7.8       /* desvío inicial por encima (excesivo)  */
#define GAMMA_INI_ALTO_MAX 8.9
#define PULSO_SUAVE        0.4       /* cambio de vel. angular omega (°/turno)*/
#define PULSO_FUERTE       1.0       /* cambio de vel. angular omega (°/turno)*/
#define COSTO_SUAVE        1         /* unidades de RCS                       */
#define COSTO_FUERTE       2
#define OMEGA_MAX_SEGURO   0.50      /* giro máx. tolerable al entrar (°/t)   */
#define INDICADOR_MIN      2.0       /* rango del indicador gráfico de gamma  */
#define INDICADOR_MAX      10.0
#define INDICADOR_ANCHO    41        /* columnas del indicador (0,2° c/u)     */

/* ========================== TIPOS DE DATOS ========================== */

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

typedef enum {
    ANGULO_REBOTE,
    ANGULO_SEGURO,
    ANGULO_EXCESIVO
} EstadoAngulo;

typedef enum {
    EV_PENDIENTE,
    EV_SUPERADO,
    EV_FALLIDO
} EstadoEvento;

typedef struct {
    const char *nombre;
    double limite_g_frenado;    /* g máximos tolerables en el frenado    */
    double limite_g_impacto;    /* fuerza máx. de amerizaje (x el peso)  */
    int    combustible_rcs;     /* unidades de RCS para el Evento 1      */
    int    turnos_rcs;          /* turnos hasta el borde atmosférico     */
    double deriva_max;          /* perturbación máx. por turno (grados)  */
} Dificultad;

typedef enum {
    MANIOBRA_SUAVE = 1,         /* ±PULSO_SUAVE,  gasta COSTO_SUAVE  */
    MANIOBRA_FUERTE,            /* ±PULSO_FUERTE, gasta COSTO_FUERTE */
    MANIOBRA_MANTENER           /* 0°, no gasta: solo actúa la deriva */
} TipoManiobra;


typedef struct {
    int          t;                 /* turnos que faltaban (T-t)        */
    TipoManiobra maniobra;
    double       delta_pulso;       /* impulso aplicado al giro (°/t)   */
    double       deriva;            /* perturbación del turno (°/t)     */
    double       omega_resultante;  /* velocidad angular final (°/t)    */
    double       gamma_resultante;  /* ángulo al terminar el turno (°)  */
    int          rcs_restante;
} TurnoRCS;


typedef struct {
    double tiempo;              /* s   */
    double velocidad;           /* m/s */
    double energia_cinetica;    /* J   */
} PasoFrenado;

 
typedef struct {
    const Dificultad *dificultad;
    double gamma_inicial;               /* grados (desvío al azar)       */
    double gamma;                       /* grados actuales / en t = 0    */
    double omega;                       /* vel. angular actual (°/turno) */
    double deriva_proxima;              /* lectura del radar p/ el turno */
    int    rcs_recalentado;             /* 1 = pulso fuerte en enfriamiento */
    int    rcs_usado;
    int    num_turnos;
    TurnoRCS bitacora[MAX_TURNOS_RCS];
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
double generar_gamma_inicial(void);
double generar_deriva(double deriva_max);
void generar_tabla_frenado(const DatosMision *d, PasoFrenado tabla[], int n);

/* ============ LÓGICA DE LA MISIÓN (compartida Consola + GUI) ============ */
const Dificultad *obtener_dificultad(int indice);
const char *nombre_evento(int indice);
const char *nombre_estado_evento(EstadoEvento estado);
const char *nombre_maniobra(TipoManiobra m);
int datos_coherentes(const DatosMision *d, char *mensaje, int tam);

void iniciar_reporte(Reporte *rep, const Dificultad *dif);

/* Ejecuta un turno del minijuego RCS (sirve tanto para consola como para GUI).
 * direccion: -1 para prógrado (baja gamma), +1 para retrógrado (sube gamma), 0 mantener. */
void aplicar_turno_rcs(Reporte *rep, TipoManiobra m, int direccion);

void resolver_frenado(const DatosMision *d, const Dificultad *dif, Reporte *rep);
void resolver_amerizaje(const DatosMision *d, const Dificultad *dif, Reporte *rep);
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

=======
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
#define GAMMA_MIN          5.5      
#define GAMMA_MAX          7.0      
#define GAMMA_CENTRO       ((GAMMA_MIN + GAMMA_MAX) / 2.0)
#define NUM_PASOS_FRENADO  4        
#define TAM_TABLA          (NUM_PASOS_FRENADO + 1)  
#define NUM_EVENTOS        3
#define NUM_DIFICULTADES   3
#define PUNTOS_EVENTO      300
#define PENALIZACION_RCS   25        /* puntos por cada unidad de RCS gastada */
#define BONUS_PRECISION    100       /* misión exitosa y gamma casi centrado  */
#define TOLERANCIA_BONUS   0.25      /* grados respecto de GAMMA_CENTRO       */
#define TAM_MENSAJE        128       /* largo máximo de un mensaje de error   */

/* ========== MINIJUEGO EVENTO 1: ALINEACIÓN CON PROPULSORES RCS ========== */
#define MAX_TURNOS_RCS     5         /* tamaño máximo de la bitácora          */
#define GAMMA_INI_BAJO_MIN 3.6       /* desvío inicial por debajo (rebote)    */
#define GAMMA_INI_BAJO_MAX 4.7
#define GAMMA_INI_ALTO_MIN 7.8       /* desvío inicial por encima (excesivo)  */
#define GAMMA_INI_ALTO_MAX 8.9
#define PULSO_SUAVE        0.4       /* cambio de vel. angular omega (°/turno)*/
#define PULSO_FUERTE       1.0       /* cambio de vel. angular omega (°/turno)*/
#define COSTO_SUAVE        1         /* unidades de RCS                       */
#define COSTO_FUERTE       2
#define OMEGA_MAX_SEGURO   0.50      /* giro máx. tolerable al entrar (°/t)   */
#define INDICADOR_MIN      2.0       /* rango del indicador gráfico de gamma  */
#define INDICADOR_MAX      10.0
#define INDICADOR_ANCHO    41        /* columnas del indicador (0,2° c/u)     */
/* ========== MINIJUEGO EVENTO 2: SUSTENTACIÓN Y FRENADO AERODINÁMICO ========== */
#define FASES_FRENADO      4         /* cantidad de turnos del frenado        */
#define FACTOR_SUST_ALTA   0.78     
#define FACTOR_NOMINAL     1.00      
#define FACTOR_INM_DENSA   1.25      
#define CALOR_SUST_ALTA   -15.0      
#define CALOR_NOMINAL      18.0
#define CALOR_INM_DENSA    38.0
#define CALOR_INICIAL      25.0      
#define CALOR_MAXIMO      100.0      
#define MARGEN_VEL_PARAC   60.0      

#define FACTOR_VEL_TEMPRANA  0.92     
#define FACTOR_VEL_NOMINAL   1.00     
#define FACTOR_VEL_TARDIA    1.45     
#define FACTOR_HUND_CUNA     1.20    
#define FACTOR_HUND_NOMINAL  1.00     
#define FACTOR_HUND_PLANA    0.60     

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

typedef enum {
    ANGULO_REBOTE,
    ANGULO_SEGURO,
    ANGULO_EXCESIVO
} EstadoAngulo;

typedef enum {
    EV_PENDIENTE,
    EV_SUPERADO,
    EV_FALLIDO
} EstadoEvento;

typedef struct {
    const char *nombre;
    double limite_g_frenado;    /* g máximos tolerables en el frenado    */
    double limite_g_impacto;    /* fuerza máx. de amerizaje (x el peso)  */
    int    combustible_rcs;     /* unidades de RCS para el Evento 1      */
    int    turnos_rcs;          /* turnos hasta el borde atmosférico     */
    double deriva_max;          /* perturbación máx. por turno (grados)  */
} Dificultad;

typedef enum {
    MANIOBRA_SUAVE = 1,         /* ±PULSO_SUAVE,  gasta COSTO_SUAVE  */
    MANIOBRA_FUERTE,            /* ±PULSO_FUERTE, gasta COSTO_FUERTE */
    MANIOBRA_MANTENER           /* 0°, no gasta: solo actúa la deriva */
} TipoManiobra;


typedef struct {
    int          t;                 /* turnos que faltaban (T-t)        */
    TipoManiobra maniobra;
    double       delta_pulso;       /* impulso aplicado al giro (°/t)   */
    double       deriva;            /* perturbación del turno (°/t)     */
    double       omega_resultante;  /* velocidad angular final (°/t)    */
    double       gamma_resultante;  /* ángulo al terminar el turno (°)  */
    int          rcs_restante;
} TurnoRCS;


typedef enum {
    PERFIL_SUST_ALTA = 1,       
    PERFIL_NOMINAL,             
    PERFIL_INM_DENSA           
} PerfilFrenado;

typedef struct {
    double tiempo;              
    double velocidad;           
    double energia_cinetica;   
    double g_fase;              
    double calor_escudo;        
    PerfilFrenado perfil;       
} PasoFrenado;

 
typedef struct {
    const Dificultad *dificultad;
    double gamma_inicial;               
    double gamma;                       
    double omega;                       
    double deriva_proxima;              
    int    rcs_recalentado;             
    int    rcs_usado;
    int    num_turnos;
    TurnoRCS bitacora[MAX_TURNOS_RCS];
    EstadoEvento eventos[NUM_EVENTOS];

    double energia_inicial;           
    double energia_final;             
    double trabajo_roz;                
    double desaceleracion;            
    double desaceleracion_g;          
    double calor_escudo;               
    double calor_max_alcanzado;       
    double densidad_proxima;          
    int    fase_frenado;                
    int    motivo_fallo_frenado;       
    PasoFrenado tabla[TAM_TABLA];

    int    etapa_amerizaje;             /* 0 = elegir paracaídas, 1 = elegir actitud, 2 = fin */
    int    opcion_paracaidas;           /* 1 = Temprana, 2 = Nominal, 3 = Tardía */
    int    opcion_actitud_agua;         /* 1 = Cuña, 2 = Nominal, 3 = Plana */
    double factor_viento;               /* condición de ráfagas/oleaje (0.85 a 1.20) */
    double v_impacto_real;              /* m/s reales al tocar el agua */
    double d_hundimiento_real;          /* m reales de hundimiento */
    double fuerza_impacto;              /* N */
    double fuerza_en_pesos;             /* múltiplos del peso */
    int    motivo_fallo_amerizaje;      /* 1 = cuerdas rotas, 2 = estructura no resiste */         

    int puntaje;
    int exito;
} Reporte;

double energia_cinetica(double masa, double velocidad);
double energia_potencial(double masa, double altura);
double energia_mecanica(double masa, double velocidad, double altura);

double calcular_trabajo_rozamiento(const DatosMision *d);
double calcular_desaceleracion_media(const DatosMision *d);
double calcular_fuerza_amerizaje(const DatosMision *d);
EstadoAngulo clasificar_angulo(double gamma);
double generar_gamma_inicial(void);
double generar_deriva(double deriva_max);
void generar_tabla_frenado(const DatosMision *d, PasoFrenado tabla[], int n);

const Dificultad *obtener_dificultad(int indice);
const char *nombre_evento(int indice);
const char *nombre_estado_evento(EstadoEvento estado);
const char *nombre_maniobra(TipoManiobra m);
int datos_coherentes(const DatosMision *d, char *mensaje, int tam);

void iniciar_reporte(Reporte *rep, const Dificultad *dif);

void aplicar_turno_rcs(Reporte *rep, TipoManiobra m, int direccion);
const char *nombre_perfil_frenado(PerfilFrenado p);
void iniciar_frenado(const DatosMision *d, Reporte *rep);
void aplicar_turno_frenado(const DatosMision *d, const Dificultad *dif,Reporte *rep, PerfilFrenado p);
void resolver_frenado(const DatosMision *d, const Dificultad *dif, Reporte *rep);
void resolver_amerizaje(const DatosMision *d, const Dificultad *dif, Reporte *rep);
void calcular_puntaje(Reporte *rep);
const char *nombre_opcion_paracaidas(int op);
const char *nombre_opcion_actitud(int op);
void iniciar_amerizaje(const DatosMision *d, Reporte *rep);
void elegir_apertura_paracaidas(const DatosMision *d, Reporte *rep, int opcion);
void elegir_actitud_impacto(const DatosMision *d, const Dificultad *dif,Reporte *rep, int opcion);
void cargar_datos_por_defecto(DatosMision *d);
void ingresar_datos_mision(DatosMision *d);
const Dificultad *seleccionar_dificultad(void);
void ejecutar_mision(const DatosMision *d, const Dificultad *dif, Reporte *rep);
void mostrar_reglas(void);
void mostrar_datos(const DatosMision *d);
void mostrar_analisis(const DatosMision *d, double gamma);
void mostrar_reporte(const Reporte *rep);

>>>>>>> Stashed changes
#endif /* ARTEMIS_H */