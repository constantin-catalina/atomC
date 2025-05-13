CC = gcc
CFLAGS = -g -Wall
SRC = lexer.c utils.c parser.c ad.c at.c vm.c gc.c main.c
CC = gcc
CFLAGS = -g -Wall
SRC = lexer.c utils.c parser.c ad.c at.c vm.c gc.c main.c
OBJ = lexer.o utils.o parser.o ad.o at.o vm.o gc.o main.o
EXE = prog

all: $(EXE)
	@$(MAKE) -s clean

$(EXE): $(OBJ)
	@$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@del /Q $(OBJ) $(EXE) 2>nul