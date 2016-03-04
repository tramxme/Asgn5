GCC = gcc
FLAGS = -Wall -Werror

all: minls minget

minls: minls.c
	$(CC) $(FLAGS) minls.c -o minls

minget: minget.c
	$(CC) $(FLAGS) minget.c -o minget

