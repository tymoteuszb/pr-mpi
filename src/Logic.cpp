#include "Logic.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand */
#include <time.h>       /* time */

using namespace std;

Logic::Logic(int arbiters, int* status) {
  this->arbiters = arbiters;
  this->status = status;
}

void Logic::run() {
	const int second = 1000000;
	srand (time(NULL));
	unsigned int waitingTime;

  	while(1) {

		//Czekanie aż student zgłosi chęć udziału w zawodach (min 1 sekunda, max 6 sekund)
		waitingTime = (rand() % 50) * second/10 + second;
	    usleep(waitingTime);

	    //Ustawienie statusu 1, informującego drugi wątek, że proces chce wystartować w zawodach
	    cout<<"Chce wziac udzial w zawodach!"<<endl;
	    *this->status = 1;

	    //Czekanie, aż proces zostanie przydzielony do grupy i będzie brał udział w zawodach 
	    while(*this->status != 2)
	    	usleep(second);

	    cout<<"Zaczynam pic!"<<endl;
	    
	    //Picie aż padnę (min 1 sekunda, max 6 sekund)
		waitingTime = (rand() % 50) * second/10 + second;
	    usleep(waitingTime);

	    //Informacja dla drugiego wątku, że padłem i chcę opuścić grupę
	    *this->status = 3;
	    cout<<"Padlem :("<<endl;

	    //Czekanie, aż student opuści grupę i nie będzie brał już udziału w zawodach
	    while(*this->status != 0)
	    	usleep(second);
	}
}

Logic::~Logic() {

}