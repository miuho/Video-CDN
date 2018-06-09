SHELL=/bin/sh

C_FILES = $(wildcard *.c)
H_FILES = $(C_FILES:.c=.h)
OBJS = $(C_FILES:.c=.o)

all: $(OBJS)

%.o: %.c %.h
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@echo "cleaning" $(OBJS)
	@rm --force $(OBJS)
