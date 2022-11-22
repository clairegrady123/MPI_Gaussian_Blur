CC=mpicc

CFLAGS=-Wall -pedantic 
LIBS=-lm

OBJECTS=gaussianLib.o segment.o qdbmp.o 

all: gauss $(OBJECTS)

debug: CFLAGS += -DDEBUG -g
debug: all

gauss: $(OBJECTS) gauss.c
	$(CC) $(CFLAGS) $(OBJECTS) gauss.c -o gauss $(LIBS)

clean:
	rm -f gauss $(OBJECTS)

run:
	mpirun -np 4 ./gauss pencils.bmp pencils_blur.bmp 10
