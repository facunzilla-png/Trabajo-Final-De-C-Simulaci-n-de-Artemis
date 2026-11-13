# ---------------------------------------------------------------------------
# Uso:
#   make            compila la version GRAFICA  -> artemis_gui   (necesita raylib)
#   make consola    compila la version CONSOLA  -> artemis_consola (no necesita raylib)
#   make clean      borra los archivos compilados
#   make STATIC=1   (Windows) enlaza raylib de forma ESTATICA: el .exe no necesita
#                   raylib.dll ni glfw3.dll en la carpeta para ejecutarse
#
# Si raylib esta instalada en una carpeta no estandar:
#   make RAYLIB_DIR=/ruta/a/raylib/src
# ---------------------------------------------------------------------------

CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -I.

ifneq ($(RAYLIB_DIR),)
CFLAGS  += -I$(RAYLIB_DIR)
LDFLAGS += -L$(RAYLIB_DIR)
endif

# Con STATIC=1 se usa libraylib.a en vez de la version DLL
ifeq ($(STATIC),1)
LDFLAGS += -static
endif

# En Windows (MSYS2) las librerias graficas son distintas que en Linux
ifneq ($(strip $(OS)$(MSYSTEM)),)
    LIBS_GUI = -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    EXE      = .exe
else
    LIBS_GUI = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    EXE      =
endif

LOGICA = artemis.o validaciones.o

# raygui.h es una libreria de terceros: sus avisos de "parametro sin usar" no son
# errores nuestros, asi que se silencian solo al compilar el archivo que la incluye.
interfaz.o: CFLAGS += -Wno-unused-parameter -Wno-unused-but-set-variable

all: gui

gui: artemis_gui$(EXE)
consola: artemis_consola$(EXE)

artemis_gui$(EXE): main.o interfaz.o $(LOGICA)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS_GUI)

artemis_consola$(EXE): main_consola.o $(LOGICA)
	$(CC) $^ -o $@ -lm

main.o: main.c interfaz.h artemis.h
interfaz.o: interfaz.c interfaz.h artemis.h validaciones.h raygui.h
main_consola.o: main_consola.c artemis.h validaciones.h
artemis.o: artemis.c artemis.h validaciones.h
validaciones.o: validaciones.c validaciones.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o artemis_gui artemis_gui.exe artemis_consola artemis_consola.exe

.PHONY: all gui consola clean
