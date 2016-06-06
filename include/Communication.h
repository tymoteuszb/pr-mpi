#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <mpi.h>

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
    MPI_Datatype mpi_single_participant_type;
    MPI_Datatype mpi_participants_type;
};

#endif
