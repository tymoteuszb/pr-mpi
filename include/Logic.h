#ifndef LOGIC_H
#define LOGIC_H

class Logic {
  public:
    Logic(int arbiters, int* status);
    void run();
    virtual ~Logic();
};

#endif
