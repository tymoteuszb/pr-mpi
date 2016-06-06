#ifndef LOGIC_H
#define LOGIC_H

class Logic {
  public:
    Logic(int* status, int rank, int* myLamport);
    void run();
    virtual ~Logic();
  private:
    int* status;
    int rank;
    int* myLamport;
};

#endif
