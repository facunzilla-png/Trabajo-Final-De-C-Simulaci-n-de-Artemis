CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -g
LDFLAGS = -lm
TARGET  = artemis
OBJS    = main.o artemis.o validaciones.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

main.o: main.c artemis.h validaciones.h
artemis.o: artemis.c artemis.h validaciones.h
validaciones.o: validaciones.c validaciones.h

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
