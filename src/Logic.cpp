#include "Logic.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>      
#include <stdlib.h>     
#include <time.h>       

using namespace std;

Logic::Logic(int* status, int rank, int* myLamport) {
  this->status = status;
  this->rank = rank;
  this->myLamport = myLamport;
}

void Logic::run() {
	const int second = 1000000;
	srand (time(NULL));
	unsigned int waitingTime;

  	while(1) {

		//Czekanie aż student zgłosi chęć udziału w zawodach (2-7 sekund)
		waitingTime = (rand() % 50) * second/10; // + 2*second
	    usleep(waitingTime);

	    //Ustawienie statusu 1, informującego drugi wątek, że proces chce wystartować w zawodach
	    cout << rank << " ( " << *myLamport << " ) : Chce wziac udzial w zawodach!" << endl;
	    *this->status = 1;

	    //Czekanie, aż proces zostanie przydzielony do grupy i będzie brał udział w zawodach 
	    while(*this->status != 2)
	    	usleep(second);

	    cout << rank << " ( " << *myLamport << " ) : Zaczynam pic!" <<endl;
	    
	    //Picie aż padnę (2-7 sekund)
		waitingTime = (rand() % 50) * second/10;// + 2*second;
	    usleep(waitingTime);

	    //Informacja dla drugiego wątku, że padłem i chcę opuścić grupę
	    cout << rank << " ( " << *myLamport << " ) : Padlem :(" << endl;
	    *this->status = 3;

	    //Czekanie, aż student opuści grupę i nie będzie brał już udziału w zawodach
	    while(*this->status != 0)
	    	usleep(second);

	    cout << rank << " ( " << *myLamport << " ) : Opuscilem grupe - nie biore udzialu w zawodach." << endl;
	}
}

Logic::~Logic() {

}