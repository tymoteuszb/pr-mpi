#include "Communication.h"
#include <iostream>
#include <unistd.h>

#define TAG_OPEN_REQUEST 0
#define TAG_OPEN_RESPONSE 1
#define TAG_CLOSE_REQUEST 2
#define TAG_CLOSE_RESPONSE 3
#define TAG_DIE_REQUEST 4
#define TAG_DIE_RESPONSE 5
#define TAG_CLOSE_FREE 6
#define TAG_OPEN_FREE 7

using namespace std;

Communication::Communication(int arbiters, int* status, int* myLamport, int mpiRank, int mpiSize) {
	this->arbiters = arbiters;
	this->status = status;
	this->myLamport = myLamport;
	this->mpiRank = mpiRank;
	this->mpiSize = mpiSize;
	this->localStatus = *this->status;
	this->waitingForArbiter = false;

	// Specjalne typy wiadomości

	// Typ dla zwykłego żądania
	int participant_blocklengths[2] = {1, 1};
	MPI_Datatype participant_types[2] = {MPI::INT, MPI::INT};
	MPI_Aint participant_offsets[2];

	participant_offsets[0] = offsetof(singleParticipantData, id);
	participant_offsets[1] = offsetof(singleParticipantData, lamport);

	MPI_Type_create_struct(2, participant_blocklengths, participant_offsets, participant_types, &this->mpi_single_participant_type);
	MPI_Type_commit(&this->mpi_single_participant_type);

	// Typ dla żądania wraz z listą uczestników
	int participants_blocklengths[3] = {1, 1, maxNumParticipants};
	MPI_Datatype participants_types[3] = {MPI::INT, MPI::INT, MPI::BOOL};
	MPI_Aint participants_offsets[3];

	participants_offsets[0] = offsetof(participantsData, id);
	participants_offsets[1] = offsetof(participantsData, lamport);
	participants_offsets[2] = offsetof(participantsData, participants);

	MPI_Type_create_struct(3, participants_blocklengths, participants_offsets, participants_types, &this->mpi_participants_type);
	MPI_Type_commit(&this->mpi_participants_type);
}

void Communication::run() {
	int i;

	MPI_Request sendRequest, recvRequestWithParticipants;
	MPI_Request recvRequests[7];
	struct singleParticipantData recvData[7];
	struct participantsData recvDataWithParticipants;

	char procname[1000];
	int a;
	MPI_Get_processor_name(procname, &a);

	for(int j = 0; j < maxNumParticipants; j++)
		this->outdated[j] = 0;


	// Ustaw odbieranie wiadomości każdego typu
	for (i = 0; i < 7; i++) {
		MPI_Irecv(&recvData[i], 1, this->mpi_single_participant_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);
	}
	MPI_Irecv(&recvDataWithParticipants, 1, this->mpi_participants_type, MPI_ANY_SOURCE, TAG_OPEN_FREE, MPI_COMM_WORLD, &recvRequestWithParticipants);

	while(1) {
		if (*this->status != this->localStatus) {

			// Żądanie zawsze wygląda tak samo, zmienia się tag MPI
			int lamportCopy = *this->myLamport;
			struct singleParticipantData myRequest;
			myRequest.id = this->mpiRank;
			myRequest.lamport = lamportCopy;

			if (*this->status == 1) {
				// Wyślij żądanie do wszystkich procesów (sekcja OPEN), wstaw wszystkie IDki procesów do mojego awaitingAnswerList
				for (i = 0; i < this->mpiSize; i++) {
					if (i != this->mpiRank) {
						//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość OPEN do " << i << endl;
						MPI_Send(&myRequest, 1, this->mpi_single_participant_type, i, TAG_OPEN_REQUEST, MPI_COMM_WORLD);
						this->awaitingAnswerList.push_back(i);
					}
				}

				// Dodaj moje żądanie na lokalną kolejkę żądań
				this->openRequestsQueue.push(myRequest);

			} else if (*this->status == 3) { // Zgłaszam, że padłem
				// Czy zostałem jako jedyny w grupie?
				if (this->MyGroupEmpty()) {

					this->closeRequestsQueue.push(myRequest);
					cout << this->mpiRank << " ( " << *this->myLamport << " ) bede rozwiazywac grupe, wszyscy padli i jestem w niej sam. " << endl;
					// Wyślij żądanie do wszystkich procesów (sekcja CLOSE), wstaw wszystkie IDki procesów do mojego awaitingAnswerList
					for (i = 0; i < this->mpiSize; i++) {
						if (i != this->mpiRank) {
							//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość CLOSE do " << i << endl;
							MPI_Send(&myRequest, 1, this->mpi_single_participant_type, i, TAG_CLOSE_REQUEST, MPI_COMM_WORLD);
							this->awaitingAnswerList.push_back(i);
						}
					}

					*this->status = 0;

				} else {

					// Wyślij do wszystkich członków grupy (tych co w niej nadal są), że chcę opuścić grupę
					for (i = 0; i < maxNumParticipants; i++) {
						if (i != this->mpiRank && this->myGroup[i] == true) {
							//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość DIE do " << i << endl;
							MPI_Send(&myRequest, 1, this->mpi_single_participant_type, i, TAG_DIE_REQUEST, MPI_COMM_WORLD);
							this->awaitingAnswerList.push_back(i);
						}
					}

					cout << this->mpiRank << " ( " << *this->myLamport << " ) informuje ";
					for(list<int>::iterator i = awaitingAnswerList.begin(); i!=awaitingAnswerList.end(); i++)
					{
						cout << *i << " ";
					}
					cout<<"ze chce opuscic grupe." << endl;

				}

			}

			// Zwiększ mój zegar Lamporta
			*this->myLamport += 1;
			this->localStatus = *this->status;

		} else { // Wiadomość do odbioru

			int ready = 0;
			bool anyready = false;

			// Sprawdź czy któryś z typów jest gotowy do odbioru
			for (i = 0; i < 7; i++) {
				//cout << "[" << this->mpiRank << "] " << " sprawdzam wiadomości typu " << i << " status: " << ready << endl;
				MPI_Test(&recvRequests[i], &ready, MPI_STATUS_IGNORE);
				if (ready) {
					// Ustaw mój zegar Lamporta
					*this->myLamport = max(recvData[i].lamport, *this->myLamport) + 1;

					// Obsłuż wiadomość i przygotuj się do odbioru kolejnej wiadomości
					this->HandleMessage(i, &recvData[i]);
					anyready = true;
					MPI_Irecv(&recvData[i], 1, this->mpi_single_participant_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);
				}
			}

			// Sprawdź też dla gupiego typu z dodatkową tablicą
			MPI_Test(&recvRequestWithParticipants, &ready, MPI_STATUS_IGNORE);
			if (ready) {
				// Ustaw mój zegar Lamporta
				*this->myLamport = max(recvDataWithParticipants.lamport, *this->myLamport) + 1;

				// Obsłuż wiadomość i przygotuj się do odbioru kolejnej wiadomości
				this->HandleMessageWithParticipants(&recvDataWithParticipants);
				anyready = true;
				MPI_Irecv(&recvDataWithParticipants, 1, this->mpi_participants_type, MPI_ANY_SOURCE, TAG_OPEN_FREE, MPI_COMM_WORLD, &recvRequestWithParticipants);
			}

			if (!anyready) {
				usleep(500000);
			}

		}
	}
}

Communication::~Communication() {
	MPI_Type_free(&this->mpi_single_participant_type);
}

bool Communication::MyGroupEmpty() {
	int i;

	for (i = 0; i < maxNumParticipants; i++) {
		if (i != this->mpiRank && this->myGroup[i] == true) {
			return false;
		}
	}

	return true;
}

void Communication::HandleMessage(int tag, struct singleParticipantData* data) {
	struct singleParticipantData sender = *data;

	int i;

	MPI_Request mpiRequest;

	int lamportCopy = *this->myLamport;
	struct singleParticipantData myRequest;
	myRequest.id = this->mpiRank;
	myRequest.lamport = lamportCopy;

	switch(tag) {

		case TAG_OPEN_RESPONSE:
			// Usuń nadawcę z kolejki oczekiwań
			this->awaitingAnswerList.remove(sender.id);

			// Czy chce i czy mogę wejść do sekcji?
			if (this->localStatus == 1 && this->awaitingAnswerList.empty() && this->openRequestsQueue.top().id == this->mpiRank) {
				std::priority_queue<singleParticipantData> tempQueue;
				// cout << this->mpiRank << "na gorze kolejki (rozmiar " << this->openRequestsQueue.size() << " ) request (" << this->openRequestsQueue.top().lamport << ", " << this->openRequestsQueue.top().id << ")" << endl;
				cout << this->mpiRank << " ( "<< *this->myLamport << " ) zaraz sprobuje utworzyc grupe,  moja kolejka przed wejsciem do sekcji to : ";
				while (!this->openRequestsQueue.empty()) {
					cout << this->openRequestsQueue.top().id << " (" << this->openRequestsQueue.top().lamport << ") - ";
					tempQueue.push(this->openRequestsQueue.top());
					this->openRequestsQueue.pop();
				}
				while (!tempQueue.empty()) {
					this->openRequestsQueue.push(tempQueue.top());
					tempQueue.pop();
				}
				cout << endl;
				this->waitingForArbiter = !this->tryToCreateGroup();
			}
			break;




		case TAG_CLOSE_RESPONSE:
			// Usuń nadawcę z kolejki oczekiwań
			this->awaitingAnswerList.remove(sender.id);

			//cout << this->mpiRank << " Otrzymalem zgoda na CLOSE od " << sender.id << ", czkeam na " << this->awaitingAnswerList.size() << "odpowiedzi, ";
			//cout << "na gorze kolejki ("<< this->closeRequestsQueue.top().lamport << ", " << this->closeRequestsQueue.top().id << "), warunek rozwiazywania grupy: ";
			//cout << (this->awaitingAnswerList.empty() && !this->closeRequestsQueue.empty() && this->closeRequestsQueue.top().id == this->mpiRank) << endl;

			// Czy mogę wejść do sekcji?      
			if (this->awaitingAnswerList.empty() && !this->closeRequestsQueue.empty() && this->closeRequestsQueue.top().id == this->mpiRank) {
				this->resolveGroup();
			}
			break;




		case TAG_OPEN_REQUEST:
			// Dodaj do lokalnej kolejki zadanie nadawcy, o ile nie jest przedawnione (wiemy juz, ze nadwaca po wyslaniu wiadomosci zostal przydzielony do grupy i zrezygnowal z sekcji)
			if (sender.lamport >= this->outdated[sender.id]){
				cout<< this->mpiRank << " ( " << *this->myLamport << " ) proces " << sender.id << " ( " << sender.lamport << " ) chce wziac udzial w zawodach" << endl;
				this->openRequestsQueue.push(sender);
			}
			else
				cout << this->mpiRank << " ( " << *this->myLamport << " ) otrzymalem przedawnionego requesta ("<<sender.lamport<<" < " << this->outdated[sender.id]  <<") informujacego o checi wziecia udzialu od " << sender.id << endl;

			// Odpowiedz swoim znacznikiem czasowym
			MPI_Send(&myRequest, 1, this->mpi_single_participant_type, sender.id, TAG_OPEN_RESPONSE, MPI_COMM_WORLD);

			// Zwiększ mój zegar lamporta
			*this->myLamport += 1;
			break;




		case TAG_CLOSE_REQUEST:
			// Dodaj do lokalnej kolejki nadawcę
			this->closeRequestsQueue.push(sender);

			// Odpowiedz swoim znacznikiem czasowym
			MPI_Send(&myRequest, 1, this->mpi_single_participant_type, sender.id, TAG_CLOSE_RESPONSE, MPI_COMM_WORLD);

			// Zwiększ mój zegar lamporta
			*this->myLamport += 1;
			break;




		case TAG_DIE_RESPONSE:
			if (this->myGroup[this->mpiRank] && this->myGroup[sender.id]) {
				// Już jestem pewien, że mogę opuścić grupę
				this->awaitingAnswerList.clear();

				// Zmień status lokalny i współdzielony
				this->localStatus = 0;
				*this->status = 0;

				// Wyczyść skład grupy
				for (i = 0; i < this->mpiSize; i++) {
					this->myGroup[i] = false;
				}
			}
			break;




		case TAG_DIE_REQUEST:

			if (this->localStatus == 2) {
				// Wyrzuć wysyłającego z grupy
				this->myGroup[sender.id] = false;
				cout << this->mpiRank << " ( " << *this->myLamport << " ) ja wciaz pije, a moja grupe opuscil " << sender.id << endl;	
				// Potwierdź, że wysyłający może opuścić grupę
				MPI_Send(&myRequest, 1, this->mpi_single_participant_type, sender.id, TAG_DIE_RESPONSE, MPI_COMM_WORLD);

				// Zwiększ mój zegar lamporta
				*this->myLamport += 1;
			} 

			else if (this->localStatus == 3) {
				if (this->mpiRank > sender.id) {
					cout << this->mpiRank << " ( " << *this->myLamport << " ) padlem juz, a moja grupe opuscil " << sender.id;
					// Wyrzuć wysyłającego z grupy
					this->myGroup[sender.id] = false;
					//cout << "[ " << this->mpiRank << "] wywaliłem " << sender.id << endl;

					// Potwierdź, że wysyłający może opuścić grupę
					MPI_Send(&myRequest, 1, this->mpi_single_participant_type, sender.id, TAG_DIE_RESPONSE, MPI_COMM_WORLD);

					// Zwiększ mój zegar lamporta
					*this->myLamport += 1;

					cout << ", czy zostalem sam w grupie? " << this->MyGroupEmpty() << endl;
					if (this->MyGroupEmpty()) {
						cout<< this->mpiRank << " ( " << *this->myLamport << " ) pozwolilem pozostalym opuscic grupe, zostalem w niej sam, zaraz ja rozwiaze." << endl;
						this->awaitingAnswerList.clear();
						this->closeRequestsQueue.push(myRequest);
						// Do wszystkich oprocz siebie wysyłam żądanie o sekcję CLOSE
						for (i = 0; i < this->mpiSize; i++) {
							if (i != this->mpiRank) {
								//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość CLOSE REQUEST do " << i << endl;
								MPI_Send(&myRequest, 1, this->mpi_single_participant_type, i, TAG_CLOSE_REQUEST, MPI_COMM_WORLD);
								this->awaitingAnswerList.push_back(i);
							}
						}
					}
				}
				else
					cout << this->mpiRank << " ( " << *this->myLamport << " ) padlem juz, moja grupe chce opuscic " << sender.id << " ale ja mu nie wysylam zgody!" << endl;
			}
			break;




		case TAG_CLOSE_FREE:
			// Usuń nadawcę z początku kolejki oczekujących na sekcję krytyczną CLOSE
			//cout << this->mpiRank << " ( " << *this->myLamport << " ) wyrzucam request (" << closeRequestsQueue.top().lamport << ", " << closeRequestsQueue.top().id << ") z kolejki. " << endl;
			this->closeRequestsQueue.pop();

			// Zwolnij arbitra
			this->arbiters++;

			cout << this->mpiRank << " ( " << *this->myLamport << " ) proces " << sender.id << " zwolnil arbitera, czy czekam? " <<  this->waitingForArbiter << endl;

			// Jeśli czekałem na arbitra, spróbuję jeszcze raz utworzyć grupę
			if (this->waitingForArbiter) {
				this->waitingForArbiter = !this->tryToCreateGroup();
			}
			break;

	}
}

//Funkcja obslugi wiadomosci informujacej, ze inny proces utworzyl grupe
void Communication::HandleMessageWithParticipants(struct participantsData* data) {
	struct participantsData sender = *data;
	//int tempId = this->openRequestsQueue.top().id;
	bool participate = sender.participants[this->mpiRank];


	// Jeśli jestem w grupie, to informuję wątek logiczny, że może zacząć pić
	if (participate) {
		this->localStatus = 2;
		*this->status = 2; 
	}

	// Dla wszystkich członków nowo powstałej grupy usuń ich żądania dostępu do sekcji krytycznej OPEN
	while(!this->openRequestsQueue.empty() && sender.participants[ this->openRequestsQueue.top().id ] )
	{
		this->openRequestsQueue.pop();
		//tempId = this->openRequestsQueue.top().id;
	}

	cout << this->mpiRank << " ( " <<*this->myLamport << " ) proces " << sender.id << " utworzyl grupe o skladzie: ";
	// Dodatkowo, jeśli sam jestem w tej grupie, dodaję wszystkich członków do mojej lokalnej zmiennej określającej grupę	
	for(int i = 0; i < 100; i++)
	{
		if(sender.participants[i])
		{
			cout << i << " ";
			outdated[i] = *this->myLamport;
			if(participate)
				this->myGroup[i] = true;
		}
	}

	if(participate)
		cout << ", jestem w niej! (moj status: " << this->localStatus << ")." << endl;
	else
	{
		cout << ", nie ma mnie w niej. (moj status: " << this->localStatus << ")." << endl;
	}

	this->arbiters -= 1;

	// Jeżeli nie jestem w nowo utworzonej grupie, ale chcę wziąć udział w zawodach
	// to sprawdzam czy jestem pierwszy na swojej kolejce żądań oraz czy nie oczekuję na żadną odpowiedź
	// jeżeli te warunki są spełnione - próbuję stworzyć nową grupę
	if (!participate && this->localStatus == 1 && this->awaitingAnswerList.empty() && this->openRequestsQueue.top().id == this->mpiRank) {
		cout << this->mpiRank << " (" << *this->myLamport << ")  nie zostalem przydzielony do nowo utworzonej grupy, probuje utworzyc nowa." << endl;
		this->waitingForArbiter = !this->tryToCreateGroup();
	}
	//cout << this->mpiRank << " ( " << *(this->myLamport) << " )" << " arbiters: " << this->arbiters << endl;
}

bool Communication::tryToCreateGroup() {
	int id, i;
	MPI_Request mpiRequest;

	// Czy mamy jeszcze arbitrów?
	if (this->arbiters > 0) {

		int lamportCopy = *this->myLamport;
		struct participantsData myRequest;
		myRequest.id = this->mpiRank;
		myRequest.lamport = lamportCopy;

		// Utwórz grupę
		while (!this->openRequestsQueue.empty()) {
			id = this->openRequestsQueue.top().id;
			myRequest.participants[id] = true;
			this->myGroup[id] = true;
			this->openRequestsQueue.pop();
		}

		cout << this->mpiRank << " ( " << *(this->myLamport) << " ) jest " << this->arbiters << " arbitrow, tworze grupe o skladzie: ";
		for (i = 0; i < 100; i++) 
			if (this->myGroup[i])
				cout << i << " ";   	
		cout << endl;

		// Wyślij wiadomość do wszystkich procesów
		for (i = 0; i < this->mpiSize; i++) {
			if (i != this->mpiRank) {
				//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość OPEN FREE do " << i << endl;
				MPI_Send(&myRequest, 1, this->mpi_participants_type, i, TAG_OPEN_FREE, MPI_COMM_WORLD);
			}
		}

		this->arbiters -= 1;

		this->localStatus = 2;
		*this->status = 2;
		*this->myLamport += 1;
		return true;

	} else {
		cout << this->mpiRank << " ( " << *this->myLamport << " ) próbowałem utworzyć grupę, ale brakuje arbitrów." << endl;
		return false;
	}
}

void Communication::resolveGroup() {
	int i;
	MPI_Request mpiRequest;


	this->closeRequestsQueue.pop();

	// Oddajemy arbitra
	this->arbiters += 1;

	// Przygotuj wiadomość
	int lamportCopy = *this->myLamport;
	struct singleParticipantData myRequest;
	myRequest.id = this->mpiRank;
	myRequest.lamport = lamportCopy;

	// Do wszystkich procesów wysyłam wiadomość, że zwalniam sekcję krytyczną #b
	for (i = 0; i < this->mpiSize; i++) {
		if (i != this->mpiRank) {
			//cout << "[" << this->mpiRank << "] " << " wysyłam wiadomość CLOSE_FREE do " << i << endl;
			MPI_Send(&myRequest, 1, this->mpi_single_participant_type, i, TAG_CLOSE_FREE, MPI_COMM_WORLD);
		}
	}

	cout << this->mpiRank << " ( " << *this->myLamport << " ) rozwiazalem grupe i zwolnilem arbitra." << endl;

	// Wyczyść skład grupy
	for (i = 0; i < this->mpiSize; i++) {
		this->myGroup[i] = false;
	}

	// Zwiększ zegar lamporta, wróć do początkowego statusu
	this->localStatus = 0;
	*this->status = 0;
	*this->myLamport += 1;
}


void Communication::printMe()
{
	cout << this->mpiRank << " ( " << *this->myLamport << " ) ";
}

void Communication::broadcastToAll(int tag)
{
	//Przygotowanie requesta do wyslania	
	int lamportCopy = *this->myLamport;
	struct singleParticipantData myMsg;
	myMsg.id = this->mpiRank;
	myMsg.lamport = lamportCopy;

	//Wysylanie requesta do wszytskich (poza soba)
	for(int i = 0; i < this->mpiSize; i++)
		if(this->mpiRank != i)
			MPI_Send(&myMsg, 1, this->mpi_single_participant_type, i, tag, MPI_COMM_WORLD);
}

void Communication::broadcastToMyGroup(int tag)
{
	//Przygotowanie requesta do wyslania	
	int lamportCopy = *this->myLamport;
	struct singleParticipantData myMsg;
	myMsg.id = this->mpiRank;
	myMsg.lamport = lamportCopy;

	//Wysylanie requesta do wszytskich ktorzy nadal sa ze mna w grupie (poza soba)
	for(int i = 0; i < this->mpiSize; i++)
		if(this->mpiRank != i && this->myGroup[i])
			MPI_Send(&myMsg, 1, this->mpi_single_participant_type, i, tag, MPI_COMM_WORLD);
}
