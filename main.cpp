#include <mpi.h>
#include <stdlib.h>
#include <iostream>

using namespace std;
 
int main (int argc, char* argv[]) {
  int rank, size, arbiters = 5;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  if (argv[1] != '\0') {
  	arbiters = atoi(argv[1]);
  }

  MPI_Finalize();

  return 0;
}
