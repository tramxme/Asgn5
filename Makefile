CC = gcc
FLAGS = -Wall -Werror -g

all: clean minls minget

minls: minls.o
	$(CC) $(FLAGS) minls.o -o minls

minls.o: minls.c
	$(CC) $(FLAGS) -c minls.c -o minls.o

minget: minget.o
	$(CC) $(FLAGS) minget.o -o minget

minget.o: minget.c
	$(CC) $(FLAGS) -c minget.c -o minget.o

clean:
	rm *.o *.exe
