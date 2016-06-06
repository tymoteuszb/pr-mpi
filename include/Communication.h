#ifndef COMMUNICATION_H
#define COMMUNICATION_H

class Communication {
  public:
    Communication(int arbiters, int* status);
    void run();
    virtual ~Communication();
};

#endif
