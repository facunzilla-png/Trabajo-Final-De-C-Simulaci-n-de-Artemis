#ifndef ARTEMIS_H
#define ARTEMIS_H

/* ===================== CONSTANTES FÍSICAS ===================== */
#define G_TERRESTRE        9.80      /* m/s^2 */

#define MASA_CAPSULA       10400.0   
#define V_REINGRESO        11000.0   
#define H_REINGRESO        120000.0  
#define V_PARACAIDAS       25.0      
#define H_PARACAIDAS       2500.0    
#define V_IMPACTO          8.0       
#define DT_FRENADO         400.0     
#define D_HUNDIMIENTO      1.50      

#define GAMMA_MIN          5.5      
#define GAMMA_MAX          7.0      
#define GAMMA_CENTRO       ((GAMMA_MIN + GAMMA_MAX) / 2.0)
#define NUM_PASOS_FRENADO  4        
#define TAM_TABLA          (NUM_PASOS_FRENADO + 1)  
#define NUM_EVENTOS        3
#define NUM_DIFICULTADES   3
#define PUNTOS_EVENTO      300
#define PENALIZACION_RCS   25        
#define BONUS_PRECISION    100    
#define TOLERANCIA_BONUS   0.25     
#define TAM_MENSAJE        128      

#define MAX_TURNOS_RCS     5         
#define GAMMA_INI_BAJO_MIN 3.6       
#define GAMMA_INI_BAJO_MAX 4.7
#define GAMMA_INI_ALTO_MIN 7.8       
#define GAMMA_INI_ALTO_MAX 8.9
#define PULSO_SUAVE        0.4       
#define PULSO_FUERTE       1.0      
#define COSTO_SUAVE        1         
#define COSTO_FUERTE       2
#define OMEGA_MAX_SEGURO   0.50      
#define INDICADOR_MIN      2.0       
#define INDICADOR_MAX      10.0
#define INDICADOR_ANCHO    41        

#define FASES_FRENADO      4         
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
    double masa;          
    double v_inicial;      
    double h_inicial;       
    double v_paracaidas;   
    double h_paracaidas;   
    double v_impacto;      
    double dt_frenado;     
    double d_hundimiento;  
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
    double limite_g_frenado;    
    double limite_g_impacto;    
    int    combustible_rcs;     
    int    turnos_rcs;         
    double deriva_max;         
} Dificultad;

typedef enum {
    MANIOBRA_SUAVE = 1,        
    MANIOBRA_FUERTE,          
    MANIOBRA_MANTENER          
} TipoManiobra;


typedef struct {
    int          t;                 
    TipoManiobra maniobra;
    double       delta_pulso;      
    double       deriva;           
    double       omega_resultante;  
    double       gamma_resultante;  
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

    int    etapa_amerizaje;            
    int    opcion_paracaidas;        
    int    opcion_actitud_agua;       
    double factor_viento;             
    double v_impacto_real;           
    double d_hundimiento_real;         
    double fuerza_impacto;             
    double fuerza_en_pesos;            
    int    motivo_fallo_amerizaje;           

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
void calcular_puntaje(Reporte *rep);
const char *nombre_opcion_paracaidas(int op);
const char *nombre_opcion_actitud(int op);
void iniciar_amerizaje(const DatosMision *d, Reporte *rep);
void elegir_apertura_paracaidas(const DatosMision *d, Reporte *rep, int opcion);
void elegir_actitud_impacto(const DatosMision *d, const Dificultad *dif,Reporte *rep, int opcion);
void cargar_datos_por_defecto(DatosMision *d);

#endif 