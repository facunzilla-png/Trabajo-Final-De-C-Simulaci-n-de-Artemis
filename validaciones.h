/*
 * validaciones.h
 * Funciones de ingreso seguro por consola y mensajes de advertencia.
 * Ninguna función de este módulo devuelve hasta que el dato sea válido.
 */
#ifndef VALIDACIONES_H
#define VALIDACIONES_H

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

#endif /* VALIDACIONES_H */
