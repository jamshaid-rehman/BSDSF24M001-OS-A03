CC = gcc
CFLAGS = -Wall -Iinclude
SRC = src/main.c src/shell.c src/execute.c
OBJ = $(SRC:src/%.c=obj/%.o)
BIN = bin/myshell

all: $(BIN)

$(BIN): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(BIN)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin
