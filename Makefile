INC=-Iinclude
MPICC?=mpic++

all: main

main: main.o src/Logic.o src/Communication.o
	$(MPICC) -o main main.o src/Logic.o src/Communication.o

main.o: main.cpp
	$(MPICC) $(INC) -c main.cpp -o main.o

src/Logic.o: src/Logic.cpp
	$(MPICC) $(INC) -c src/Logic.cpp -o src/Logic.o

src/Communication.o: src/Communication.cpp
	$(MPICC) $(INC) -c src/Communication.cpp -o src/Communication.o

clean:
	rm -f *.o && rm -f src/*.o && rm main