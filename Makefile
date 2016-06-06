EXECS=hello
MPICC?=mpicc

all: ${EXECS}

hello: hello.c
	${MPICC} -o hello hello.c

clean:
	rm ${EXECS}