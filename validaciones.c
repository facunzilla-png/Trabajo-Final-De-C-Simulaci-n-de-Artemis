#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include "validaciones.h"

#define TAM_BUFFER 64
#define TAM_ERROR  96

static int solo_espacios(const char *p)
{
    while (isspace((unsigned char)*p)) p++;
    return *p == '\0';
}

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
    strcpy(copia, texto);           

    
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
