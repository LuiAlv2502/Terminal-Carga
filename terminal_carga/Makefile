CC      = gcc
CFLAGS  = -Wall -Wextra -pthread -g
TARGET  = terminal_carga
SRCS    = main.c terminal.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c terminal.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) operaciones.log

.PHONY: clean