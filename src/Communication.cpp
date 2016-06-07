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

Communication::Communication(int arbiters, int* status, int* myLamport, int mpiRank, int mpiSize) {
  this->arbiters = arbiters;
  this->status = status;
  this->myLamport = myLamport;
  this->mpiRank = mpiRank;
  this->mpiSize = mpiSize;
  this->localStatus = *this->status;
  this->waitingForArbiter = false;

  // Specjalne typy wiadomości

  // Typ dla zwykłego żądania
  int participant_blocklengths[2] = {1, 1};
  MPI_Datatype participant_types[2] = {MPI::INT, MPI::INT};
  MPI_Aint participant_offsets[2];

  participant_offsets[0] = offsetof(singleParticipantData, id);
  participant_offsets[1] = offsetof(singleParticipantData, lamport);

  MPI_Type_create_struct(2, participant_blocklengths, participant_offsets, participant_types, &this->mpi_single_participant_type);
  MPI_Type_commit(&this->mpi_single_participant_type);

  // Typ dla żądania wraz z listą uczestników
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
  int i;

  MPI_Request sendRequest, recvRequestWithParticipants;
  MPI_Request recvRequests[6];
  struct singleParticipantData recvData[6];
  struct participantsData recvDataWithParticipants;

  // Ustaw odbieranie wiadomości każdego typu
  for (i = 0; i < 6; i++) {
    MPI_Irecv(&recvData[i], 1, this->mpi_single_participant_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);
  }
  MPI_Irecv(&recvDataWithParticipants, 1, this->mpi_participants_type, MPI_ANY_SOURCE, TAG_PARTICIPANTS, MPI_COMM_WORLD, &recvRequestWithParticipants);

  while(1) {
    if (*this->status != this->localStatus) {

      // Żądanie zawsze wygląda tak samo, zmienia się tag MPI
      int lamportCopy = *this->myLamport;
      struct singleParticipantData myRequest;
      myRequest.id = this->mpiRank;
      myRequest.lamport = lamportCopy;

      if (*this->status == 1) {

        // Wyślij żądanie do wszystkich procesów (sekcja OPEN), wstaw wszystkie IDki procesów do mojego awaitingAnswerList
        for (i = 0; i < this->mpiSize; i++) {
          if (i != this->mpiRank) {
            cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość OPEN do " << i << endl;
            MPI_Isend(&myRequest, 1, this->mpi_single_participant_type, i, TAG_OPEN_REQUEST, MPI_COMM_WORLD, &sendRequest);
            awaitingAnswerList.push_back(i);
          }
        }

        // Dodaj moje żądanie na lokalną kolejkę żądań
        this->openRequestsQueue.push(myRequest);

      } else if (*this->status == 3) { // Zgłaszam, że padłem

        // Czy zostałem jako jedyny w grupie?
        if (this->MyGroupEmpty()) {

          // Wyślij żądanie do wszystkich procesów (sekcja CLOSE), wstaw wszystkie IDki procesów do mojego awaitingAnswerList
          for (i = 0; i < this->mpiSize; i++) {
            if (i != this->mpiRank) {
              cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość CLOSE do " << i << endl;
              MPI_Isend(&myRequest, 1, this->mpi_single_participant_type, i, TAG_CLOSE_REQUEST, MPI_COMM_WORLD, &sendRequest);
              awaitingAnswerList.push_back(i);
            }
          }

        } else {

          // Wyślij do wszystkich członków grupy (tych co w niej nadal są), że chcę opuścić grupę
          for (i = 0; i < maxNumParticipants; i++) {
            if (i != this->mpiRank && this->myGroup[i] == true) {
              cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość DIE do " << i << endl;
              MPI_Isend(&myRequest, 1, this->mpi_single_participant_type, i, TAG_DIE_REQUEST, MPI_COMM_WORLD, &sendRequest);
              awaitingAnswerList.push_back(i);
            }
          }

        }

      }

      // Zwiększ mój zegar Lamporta
      *this->myLamport += 1;
      this->localStatus = *this->status;

    } else { // Wiadomość do odbioru

      int ready = 0;
      bool anyready = false;

      // Sprawdź czy któryś z typów jest gotowy do odbioru
      for (i = 0; i < 6; i++) {
        cout << "[" << this->mpiRank << "] " << " sprawdzam wiadomości typu " << i << " status: " << ready << endl;
        MPI_Test(&recvRequests[i], &ready, MPI_STATUS_IGNORE);
        if (ready) {
          this->HandleMessage(i, &recvData[i]);
          anyready = true;
          MPI_Irecv(&recvData[i], 1, this->mpi_single_participant_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);
        }
      }

      // Sprawdź też dla gupiego typu z dodatkową tablicą
      MPI_Test(&recvRequestWithParticipants, &ready, MPI_STATUS_IGNORE);
      if (ready) {
        this->HandleMessageWithParticipants(&recvDataWithParticipants);
        anyready = true;
        MPI_Irecv(&recvDataWithParticipants, 1, this->mpi_participants_type, MPI_ANY_SOURCE, TAG_PARTICIPANTS, MPI_COMM_WORLD, &recvRequestWithParticipants);
      }

      if (!anyready) {
        usleep(500000);
      }
      
    }
  }
}

Communication::~Communication() {
  MPI_Type_free(&this->mpi_single_participant_type);
}

bool Communication::MyGroupEmpty() {
  int i;

  for (i = 0; i < maxNumParticipants; i++) {
    if (i != this->mpiRank && this->myGroup[i] == true) {
      return true;
    }
  }

  return false;
}

// #define TAG_OPEN_REQUEST 0
// #define TAG_OPEN_RESPONSE 1
// #define TAG_CLOSE_REQUEST 2
// #define TAG_CLOSE_RESPONSE 3
// #define TAG_DIE_REQUEST 4
// #define TAG_DIE_RESPONSE 5
// #define TAG_PARTICIPANTS 6

void Communication::HandleMessage(int tag, struct singleParticipantData* data) {
  struct singleParticipantData sender = *data;

  switch(tag) {
    case TAG_OPEN_RESPONSE:
      // Usuń nadawcę z kolejki oczekiwań
      this->awaitingAnswerList.remove(sender.id);

      // Czy mogę wejść do sekcji?
      if (this->awaitingAnswerList.empty() && this->openRequestsQueue.top().id == this->mpiRank) {
        this->waitingForArbiter = !this->tryToCreateGroup();
      }
  }
}

void Communication::HandleMessageWithParticipants(struct participantsData* data) {
  cout << "do obsłużenia wiadomość typu 7" << endl;
}

bool Communication::tryToCreateGroup() {
  int id;

  // Czy mamy jeszcze arbitrów?
  if (this->arbiters > 0) {

    int lamportCopy = *this->myLamport;
    struct participantsData myRequest;
    myRequest.id = this->mpiRank;
    myRequest.lamport = lamportCopy;

    // Utwórz grupę
    while (!this->openRequestsQueue.empty()) {
      id = this->openRequestsQueue.top().id;
      myRequest.participants[id] = true;
      this->myGroup[id] = true;
      this->openRequestsQueue.pop();
    }

    this->localStatus = 2;
    *this->status = 2;
    *this->myLamport += 1;
    return true;

  } else {
    return false;
  }
}
