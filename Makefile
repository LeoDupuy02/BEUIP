CC = gcc
CFLAGS = -Wall -Wextra -g -I. 
LDFLAGS = -lreadline -lpthread

# Executable final
EXEC = biceps

SRC = biceps.c gescom.c creme.c
OBJ = biceps.o gescom.o creme.o

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

biceps.o: biceps.c gescom.h version.h
	$(CC) $(CFLAGS) -c biceps.c -o biceps.o

gescom.o: gescom.c gescom.h creme.h version.h
	$(CC) $(CFLAGS) -c gescom.c -o gescom.o

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c -o creme.o

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean