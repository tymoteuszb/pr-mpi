EXECS=main
MPICC?=mpic++

all: ${EXECS}

main: main.cpp
	${MPICC} -o main main.cpp

clean:
	rm ${EXECS}