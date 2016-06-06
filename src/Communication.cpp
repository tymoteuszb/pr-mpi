#include "Communication.h"
#include <iostream>
#include <unistd.h>

using namespace std;

struct singleParticipantData {
  int id;
  int lamport;
};

struct participantsData {
  int id;
  int lamport;
  int participants[100];
};

Communication::Communication(int arbiters, int* status, int mpiRank, int mpiSize) {
  this->arbiters = arbiters;
  this->status = status;
  this->mpiRank = mpiRank;
  this->mpiSize = mpiSize;

  // Declare custom types

  // Single participant
  const int nitems = 2;
  int blocklengths[2] = {1, 1};
  MPI_Datatype types[2] = {MPI_INT, MPI_INT};
  MPI_Aint offsets[2];

  offsets[0] = offsetof(singleParticipantData, id);
  offsets[1] = offsetof(singleParticipantData, lamport);

  MPI_Type_create_struct(nitems, blocklengths, offsets, types, &this->mpi_single_participant_type);
  MPI_Type_commit(&this->mpi_single_participant_type);
}

void Communication::run() {
  int localStatus = *this->status;

  if (this->mpiRank % 2 == 0) {
    singleParticipantData send;
    send.id = this->mpiRank;
    send.lamport = 10;
    MPI_Send(&send, 1, this->mpi_single_participant_type, this->mpiRank+1, 1, MPI_COMM_WORLD);
  } else {
    MPI_Status status;
    singleParticipantData recv;
    MPI_Recv(&recv, 1, this->mpi_single_participant_type, this->mpiRank-1, 1, MPI_COMM_WORLD, &status);
    cout << "odebrałem  od " << recv.id << endl;
  }

  // while(1) {
  //   if (*this->status == localStatus) {

  //     if (*this->status == 1) {

  //     } else if (*this->status == 3) {

  //     }

  //   } else if (1) { // wiadomosc do odbioru



  //   } else {
  //     usleep(500000);
  //   }
  // }
}

Communication::~Communication() {
  MPI_Type_free(&this->mpi_single_participant_type);
}