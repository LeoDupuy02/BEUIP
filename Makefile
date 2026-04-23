CC = gcc
CFLAGS = -Wall -Werror -g -I.
LDFLAGS = -lreadline -lpthread

SRC = biceps.c gescom.c creme.c
OBJ = biceps.o gescom.o creme.o

all: biceps

biceps: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

memory-leak: CFLAGS = -Wall -Werror -g -O0 -I.
memory-leak: clean $(OBJ)
	$(CC) $(CFLAGS) -o biceps-memory-leaks $(OBJ) $(LDFLAGS)

biceps.o: biceps.c gescom.h version.h
	$(CC) $(CFLAGS) -c biceps.c -o biceps.o

gescom.o: gescom.c gescom.h creme.h version.h
	$(CC) $(CFLAGS) -c gescom.c -o gescom.o

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c -o creme.o

clean:
	rm -f $(OBJ) biceps biceps-memory-leaks