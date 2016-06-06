#include "Communication.h"
#include <iostream>
#include <unistd.h>

#define TAG_OPEN_REQUEST 0
#define TAG_OPEN_RESPONSE 1
#define TAG_CLOSE_REQUEST 2
#define TAG_CLOSE_RESPONSE 3
#define TAG_DIE_REQUEST 4
#define TAG_DIE_RESPONSE 5
#define TAG_PARTICIPANTS 6

using namespace std;

const int maxNumParticipants = 100;

struct singleParticipantData {
  int id;
  int lamport;
};

struct participantsData {
  int id;
  int lamport;
  bool participants[maxNumParticipants] = {false};
};

Communication::Communication(int arbiters, int* status, int* myLamport, int mpiRank, int mpiSize) {
  this->arbiters = arbiters;
  this->status = status;
  this->myLamport = myLamport;
  this->mpiRank = mpiRank;
  this->mpiSize = mpiSize;

  // Declare custom types

  // Single participant type
  int participant_blocklengths[2] = {1, 1};
  MPI_Datatype participant_types[2] = {MPI::INT, MPI::INT};
  MPI_Aint participant_offsets[2];

  participant_offsets[0] = offsetof(singleParticipantData, id);
  participant_offsets[1] = offsetof(singleParticipantData, lamport);

  MPI_Type_create_struct(2, participant_blocklengths, participant_offsets, participant_types, &this->mpi_single_participant_type);
  MPI_Type_commit(&this->mpi_single_participant_type);

  // Multiple participants type
  int participants_blocklengths[3] = {1, 1, maxNumParticipants};
  MPI_Datatype participants_types[3] = {MPI::INT, MPI::INT, MPI::BOOL};
  MPI_Aint participants_offsets[3];

  participants_offsets[0] = offsetof(participantsData, id);
  participants_offsets[1] = offsetof(participantsData, lamport);
  participants_offsets[2] = offsetof(participantsData, participants);

  MPI_Type_create_struct(3, participants_blocklengths, participants_offsets, participants_types, &this->mpi_participants_type);
  MPI_Type_commit(&this->mpi_participants_type);
}

void Communication::run() {
  int localStatus = *this->status;

  if (this->mpiRank % 2 == 0) {
    participantsData send;
    send.id = this->mpiRank;
    send.lamport = 10;
    send.participants[0] = true;
    MPI_Send(&send, 1, this->mpi_participants_type, this->mpiRank+1, 1, MPI_COMM_WORLD);
  } else {
    MPI_Status status;
    participantsData recv;
    MPI_Recv(&recv, 1, this->mpi_participants_type, this->mpiRank-1, 1, MPI_COMM_WORLD, &status);
    cout << "odebrałem  od " << recv.id << endl;
    cout << "participant " << recv.participants[0] << endl;
  }

  while(1) {
    if (*this->status == localStatus) {

      if (*this->status == 1) {

      } else if (*this->status == 3) {

      }

    } else if (1) { // wiadomosc do odbioru



    } else {
      usleep(500000);
    }
  }
}

Communication::~Communication() {
  MPI_Type_free(&this->mpi_single_participant_type);
}