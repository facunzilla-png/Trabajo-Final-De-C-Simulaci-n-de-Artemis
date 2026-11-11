/*
 * validaciones.c
 * Ingreso seguro de datos. Técnica usada: leer SIEMPRE una línea completa con
 * fgets() y luego convertirla con strtol()/strtod() verificando errores.
 * (scanf deja basura en el buffer y se rompe con letras: por eso no se usa).
 *
 * validar_double() concentra la validación de números reales: la usan tanto
 * leer_double() (consola) como los cuadros de texto de la interfaz gráfica.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include "validaciones.h"

#define TAM_BUFFER 64
#define TAM_ERROR  96

void mostrar_advertencia(const char *texto)
{
    printf("  [!] %s\n", texto);
}

/*
 * Lee una línea en 'buffer' (máximo tam-1 caracteres, sin el '\n').
 * Devuelve 1 si la línea entró completa y 0 si era demasiado larga
 * (en ese caso descarta el resto para no dejar basura en stdin).
 * Si stdin se cierra (EOF) termina el programa: evita bucles infinitos.
 */
static int leer_linea(char *buffer, int tam)
{
    size_t largo;
    int c;

    if (fgets(buffer, tam, stdin) == NULL) {
        printf("\n  [!] Se cerró la entrada de datos. Fin del programa.\n");
        exit(EXIT_FAILURE);
    }

    largo = strlen(buffer);
    if (largo > 0 && buffer[largo - 1] == '\n') {
        buffer[largo - 1] = '\0';
        return 1;
    }
    if (largo < (size_t)tam - 1) {
        return 1;                 /* última línea sin '\n' (fin de archivo) */
    }

    while ((c = getchar()) != '\n' && c != EOF) {
        /* descartar el resto de la línea */
    }
    return 0;
}

/* Devuelve 1 si desde 'p' solo quedan espacios hasta el final. */
static int solo_espacios(const char *p)
{
    while (isspace((unsigned char)*p)) {
        p++;
    }
    return *p == '\0';
}

/* Devuelve 1 si la cadena solo contiene caracteres posibles en un real. */
static int caracteres_numericos(const char *p)
{
    for (; *p != '\0'; p++) {
        if (!isdigit((unsigned char)*p) && !isspace((unsigned char)*p) &&
            strchr("+-.eE", *p) == NULL) {
            return 0;
        }
    }
    return 1;
}

/* Copia un motivo de error en 'error' si el llamador pidió recibirlo. */
static void poner_error(char *error, int tam_error, const char *motivo)
{
    if (error != NULL && tam_error > 0) {
        snprintf(error, (size_t)tam_error, "%s", motivo);
    }
}

int validar_double(const char *texto, double minimo, double maximo,
                   double *salida, char *error, int tam_error)
{
    char copia[TAM_BUFFER];
    char *fin;
    double valor;
    size_t i;

    if (strlen(texto) >= TAM_BUFFER) {
        poner_error(error, tam_error, "Entrada demasiado larga.");
        return 0;
    }
    strcpy(copia, texto);            /* el largo ya se verificó arriba */

    /* Aceptar coma decimal: "6,5" -> "6.5" */
    for (i = 0; copia[i] != '\0'; i++) {
        if (copia[i] == ',') {
            copia[i] = '.';
        }
    }

    if (!caracteres_numericos(copia)) {
        poner_error(error, tam_error, "Ingresá solo un número (ejemplo: 6.5).");
        return 0;
    }

    errno = 0;
    valor = strtod(copia, &fin);

    if (fin == copia || !solo_espacios(fin)) {
        poner_error(error, tam_error, "Ingresá un número real válido (ejemplo: 6.5).");
        return 0;
    }
    if (errno == ERANGE || !isfinite(valor)) {
        poner_error(error, tam_error, "Número fuera de rango representable.");
        return 0;
    }
    if (valor < minimo || valor > maximo) {
        if (error != NULL && tam_error > 0) {
            snprintf(error, (size_t)tam_error,
                     "El valor debe estar entre %g y %g.", minimo, maximo);
        }
        return 0;
    }

    *salida = valor;
    return 1;
}

int leer_entero(const char *mensaje, int minimo, int maximo)
{
    char buffer[TAM_BUFFER];
    char *fin;
    long valor;

    while (1) {
        printf("%s [%d a %d]: ", mensaje, minimo, maximo);

        if (!leer_linea(buffer, TAM_BUFFER)) {
            mostrar_advertencia("Entrada demasiado larga.");
            continue;
        }

        errno = 0;
        valor = strtol(buffer, &fin, 10);

        if (fin == buffer || !solo_espacios(fin)) {
            mostrar_advertencia("Ingresá solo un número entero (sin letras ni símbolos).");
            continue;
        }
        if (errno == ERANGE || valor < minimo || valor > maximo) {
            printf("  [!] El valor debe estar entre %d y %d.\n", minimo, maximo);
            continue;
        }
        return (int)valor;
    }
}

double leer_double(const char *mensaje, double minimo, double maximo)
{
    char buffer[TAM_BUFFER];
    char error[TAM_ERROR];
    double valor;

    while (1) {
        printf("%s [%g a %g]: ", mensaje, minimo, maximo);

        if (!leer_linea(buffer, TAM_BUFFER)) {
            mostrar_advertencia("Entrada demasiado larga.");
            continue;
        }
        if (validar_double(buffer, minimo, maximo, &valor, error, TAM_ERROR)) {
            return valor;
        }
        mostrar_advertencia(error);
    }
}

char leer_caracter(const char *mensaje, const char *validos)
{
    char buffer[TAM_BUFFER];
    char *p;
    char c;

    while (1) {
        printf("%s [%s]: ", mensaje, validos);

        if (!leer_linea(buffer, TAM_BUFFER)) {
            mostrar_advertencia("Entrada demasiado larga.");
            continue;
        }

        p = buffer;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0' || !solo_espacios(p + 1)) {
            mostrar_advertencia("Ingresá un único carácter.");
            continue;
        }

        c = (char)tolower((unsigned char)*p);
        if (strchr(validos, c) == NULL) {
            printf("  [!] Opción inválida. Opciones posibles: %s\n", validos);
            continue;
        }
        return c;
    }
}

void pausar(void)
{
    char buffer[TAM_BUFFER];

    printf("\nPresioná ENTER para continuar...");
    leer_linea(buffer, TAM_BUFFER);
}

void limpiar_pantalla(void)
{
    printf("\033[2J\033[H");   /* secuencia ANSI: borrar pantalla y volver arriba */
    fflush(stdout);
}
