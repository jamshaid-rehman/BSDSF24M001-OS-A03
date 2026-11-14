CC = gcc
CFLAGS = -Wall -Iinclude
LDFLAGS = -lreadline
SRC = src/main.c src/shell.c src/execute.c
OBJ = $(SRC:src/%.c=obj/%.o)
BIN = bin/myshell

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN) $(LDFLAGS)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): include/shell.h

clean:
	rm -rf obj bin
