#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <mpi.h>
#include <queue>
#include <list>
#include <string>

const int maxNumParticipants = 100;

struct singleParticipantData {
  int id;
  int lamport;

  bool operator<(const singleParticipantData& other) const {
    if(lamport == other.lamport)
      return id < other.id;
    else
      return lamport < other.lamport;
  }
};

struct participantsData {
  int id;
  int lamport;
  bool participants[maxNumParticipants] = {false};
};

class Communication {
  public:
    Communication(int arbiters, int* status, int* myLamport, int mpiRank, int mpiSize);
    void run();
    virtual ~Communication();
  private:
    int arbiters;
    int* status;
    int* myLamport;
    int mpiRank;
    int mpiSize;
    int localStatus;
    bool waitingForArbiter;
    bool myGroup[maxNumParticipants];
    std::priority_queue<singleParticipantData> openRequestsQueue;
    std::priority_queue<singleParticipantData> closeRequestsQueue;
    std::list<int> awaitingAnswerList;
    MPI_Datatype mpi_single_participant_type;
    MPI_Datatype mpi_participants_type;

    bool MyGroupEmpty();
    bool tryToCreateGroup();
    void resolveGroup();
    void HandleMessageWithParticipants(struct participantsData* data);
    void HandleMessage(int tag, struct singleParticipantData* data);
};

#endif
