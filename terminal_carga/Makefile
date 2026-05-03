CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g
TARGET  = terminal_carga
SRCS    = main.c terminal.c planificador.c cola.c estadisticas.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean re

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c terminal.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) operaciones.log

re: clean all