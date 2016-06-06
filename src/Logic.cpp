#include "Logic.h"
#include <iostream>
#include <unistd.h>

using namespace std;

Logic::Logic(int arbiters, int* status) {
  this->arbiters = arbiters;
  this->status = status;
}

void Logic::run() {
  while(1) {
    usleep(1000000);
    *this->status -= 1;
    cout << "logic status " << *this->status << endl;
  }
}

Logic::~Logic() {

}