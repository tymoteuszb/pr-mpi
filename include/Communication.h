#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <mpi.h>
#include <queue>
#include <list>
#include <string>

const int maxNumParticipants = 100;

struct requestData {
  int id;
  int lamport;

  bool operator<(const requestData& other) const {
    if(lamport == other.lamport)
      return id < other.id;
    else
      return lamport > other.lamport;
  }
};

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
    std::priority_queue<requestData> openRequestsQueue;
    std::list<int> awaitingAnswerList;
    MPI_Datatype mpi_request_type;
    MPI_Datatype mpi_group_info_type;

	void printMe();
	void printMyGroup();
	void printQueue();
    bool MyGroupEmpty();
    bool tryToCreateGroup();
    void resolveGroup();
    void broadcastToAll(int tag);
	void broadcastToMyGroup(int tag);
	void sendMessage(int sendTo, int tag);
	void addAllToAwaitingAnswerList();
	void addMyGroupToAwaitingAnswerList();
	bool firstOnQueue();
	void removeParticipantsFromQueue(bool participants[]);
	void determineGroupMembers();
	bool inMyGroup(int id);
	void HandleMessageWithParticipants(struct groupInfoData* data);
    void HandleMessage(int tag, struct requestData* data);
};

#endif
