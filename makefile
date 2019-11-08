CC = gcc
FLAGS = -Wall -pthread
LDFLAGS = -lrt
OBJECTS = funcoes.o main.o
TARGET = main

all: ${TARGET}

${TARGET}:	${OBJECTS}
			${CC} ${FLAGS} ${OBJECTS} -o ${TARGET} ${LDFLAGS}
