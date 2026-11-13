CC     = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -I.

ifneq ($(RAYLIB_DIR),)
CFLAGS  += -I$(RAYLIB_DIR)
LDFLAGS += -L$(RAYLIB_DIR)
endif


ifneq ($(strip $(OS)$(MSYSTEM)),)
    LIBS_GUI = -lraylib -lopengl32 -lgdi32 -lwinmm -lm
    EXE      = .exe
else
    LIBS_GUI = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    EXE      =
endif

LOGICA = artemis.o validaciones.o
interfaz.o: CFLAGS += -Wno-unused-parameter -Wno-unused-but-set-variable

all: gui

gui: artemis$(EXE)


artemis$(EXE): main.o interfaz.o $(LOGICA)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS_GUI)


main.o: main.c interfaz.h artemis.h
interfaz.o: interfaz.c interfaz.h artemis.h validaciones.h raygui.h
artemis.o: artemis.c artemis.h validaciones.h
validaciones.o: validaciones.c validaciones.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o artemis artemis.exe 

.PHONY: all gui clean
