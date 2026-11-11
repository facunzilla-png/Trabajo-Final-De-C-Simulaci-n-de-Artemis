/*
 * validaciones.h
 * Validación de datos ingresados por el usuario.
 *
 *  - validar_double(): revisa un TEXTO ya escrito (lo usa la interfaz gráfica
 *    con los cuadros de texto). No espera nada del teclado.
 *  - leer_*(): versión de consola. Piden el dato y repiten hasta que sea válido.
 */
#ifndef VALIDACIONES_H
#define VALIDACIONES_H

/*
 * Valida que 'texto' sea un número real dentro de [minimo, maximo].
 * Acepta "6.5" y "6,5". Devuelve 1 si es válido y deja el valor en *salida.
 * Si no es válido devuelve 0 y escribe el motivo en 'error' (hasta tam_error
 * caracteres). 'error' puede ser NULL si no interesa el motivo.
 */
int validar_double(const char *texto, double minimo, double maximo,
                   double *salida, char *error, int tam_error);

/* Lee un entero en el rango [minimo, maximo]. Repite hasta que sea válido. */
int leer_entero(const char *mensaje, int minimo, int maximo);

/* Lee un número real en el rango [minimo, maximo]. Acepta "6.5" y "6,5". */
double leer_double(const char *mensaje, double minimo, double maximo);

/* Lee un único carácter que esté dentro de la cadena 'validos' (ej: "sn").
 * Devuelve el carácter en minúscula. */
char leer_caracter(const char *mensaje, const char *validos);

/* Muestra un mensaje de advertencia con formato uniforme. */
void mostrar_advertencia(const char *texto);

/* Espera que el usuario presione ENTER. */
void pausar(void);

/* Borra la pantalla de la consola (solo versión de consola). */
void limpiar_pantalla(void);

#endif /* VALIDACIONES_H */
