CC = gcc
PORT=19382
CFLAGS = -DPORT=$(PORT) -g -Wall
DEPS = battle.h

all: battle 

battle: battle.o battlefunc.o
	${CC} ${CFLAGS} -o $@ battle.o battlefunc.o

%.o: %.c $(DEPS)
	gcc  $(CFLAGS) -c -o $@ $< 

clean:
	rm *.o 