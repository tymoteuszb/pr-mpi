#include <mpi.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>

#include "Logic.h"
#include "Communication.h"

using namespace std;

struct competitionData {
  int arbiters;
  int* status;
};

void *logic(void* data) {
  struct competitionData* initial_data = (struct competitionData*)(data);

  Logic* logic = new Logic(initial_data->arbiters, initial_data->status);
  logic->run();
  delete logic;

  return NULL;
}
 
int main (int argc, char* argv[]) {
  int rank, size, status = 0, arbiters = 5;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  
  if (argv[1] != '\0') {
    arbiters = atoi(argv[1]);
  }

  pthread_t logic_thread, communication_thread;

  struct competitionData initial_data;
  initial_data.arbiters = arbiters;
  initial_data.status = &status;
  
  if (pthread_create(&logic_thread, NULL, logic, &initial_data)) {
    cout << "logic thread create error";
    return 1;
  }

  Communication* communication = new Communication(arbiters, &status, rank, size);
  communication->run();
  delete communication;

  MPI_Finalize();

  return 0;
}
