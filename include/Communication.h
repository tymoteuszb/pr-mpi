#ifndef COMMUNICATION_H
#define COMMUNICATION_H

class Communication {
  public:
    Communication(int arbiters, int* status, int* myTimer);
    void run();
    virtual ~Communication();
  private:
    int arbiters;
    int* status;
    int* myTimer;
};

#endif
