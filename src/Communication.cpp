#include "Communication.h"
#include <iostream>
#include <unistd.h>

using namespace std;

Communication::Communication(int arbiters, int* status) {
  this->arbiters = arbiters;
  this->status = status;
}

void Communication::run() {
  while(1) {
    usleep(1000000);
    *this->status -= 1;
    cout << "comm status " << *this->status << endl;
  }
}

Communication::~Communication() {

}