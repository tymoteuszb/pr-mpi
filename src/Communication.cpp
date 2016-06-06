#include "Communication.h"
#include <iostream>
#include <unistd.h>
#include <mpi.h>

using namespace std;

Communication::Communication(int arbiters, int* status) {
  this->arbiters = arbiters;
  this->status = status;
}

void Communication::run() {
  int localStatus = *this->status;

  while(1) {
    if (*this->status == localStatus) {

      if (*this->status == 1) {

      } else if (*this->status == 3) {

      }

    } else if (wiadomosc do odbioru) {



    } else {
      usleep(500000);
    }
  }
}

Communication::~Communication() {

}