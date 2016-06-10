#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <mpi.h>
#include <list>
#include <string>
#include "Queue.h"

const int maxNumParticipants = 100;

struct groupInfoData {
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
    int timeOfDeath;
	bool waitingForArbiter;
	bool waitingForParticipants;
    bool myGroup[maxNumParticipants];
    int outdated[maxNumParticipants];
    Queue requestsQueue;
    std::list<int> awaitingAnswerList;
    MPI_Datatype mpi_request_type;
    MPI_Datatype mpi_group_info_type;

	void printMe();
	void printMyGroup();
    bool MyGroupEmpty();
    bool tryToCreateGroup();
    void resolveGroup();
    void broadcastToAll(int tag);
	void broadcastToMyGroup(int tag);
	void sendMessage(int sendTo, int tag);
	void addAllToAwaitingAnswerList();
	void addMyGroupToAwaitingAnswerList();
	bool firstOnQueue();
	void determineGroupMembers();
	bool inMyGroup(int id);
	void HandleGroupInfoMessage(struct groupInfoData* data);
    void HandleRequestMessage(int tag, struct requestData* data);
};

#endif
