#include "Communication.h"
#include <iostream>
#include <unistd.h>

using namespace std;

Communication::Communication(int arbiters, int* status, int* myTimer) {
  this->arbiters = arbiters;
  this->status = status;
  this->myTimer = myTimer;
}

void Communication::run() {
  while(1) {
  }
}

Communication::~Communication() {

}