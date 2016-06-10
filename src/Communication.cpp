#include "Communication.h"
#include <iostream>
#include <unistd.h>

#define TAG_START_REQUEST 0
#define TAG_START_RESPONSE 1
#define TAG_DIE_REQUEST 2
#define TAG_DIE_RESPONSE 3
#define TAG_GROUP_CREATED 5
#define TAG_GROUP_RESOLVED 4

#define PASSIVE 0
#define WAITING 1
#define DRINKING 2
#define DEAD 3

using namespace std;

Communication::Communication(int arbiters, int* status, int* myLamport, int mpiRank, int mpiSize) {
	this->arbiters = arbiters;
	this->status = status;
	this->myLamport = myLamport;
	this->mpiRank = mpiRank;
	this->mpiSize = mpiSize;
	this->localStatus = *this->status;
	this->waitingForArbiter = false;

	for(int j = 0; j < maxNumParticipants; j++)
	{
			this->outdated[j] = 0;
			this->myGroup[j] = false;
	}

	// Specjalne typy wiadomości

	// Typ dla zwykłego żądania
	int request_blocklengths[2] = {1, 1};
	MPI_Datatype request_types[2] = {MPI::INT, MPI::INT};
	MPI_Aint request_offsets[2];

	request_offsets[0] = offsetof(requestData, id);
	request_offsets[1] = offsetof(requestData, lamport);

	MPI_Type_create_struct(2, request_blocklengths, request_offsets, request_types, &this->mpi_request_type);
	MPI_Type_commit(&this->mpi_request_type);

	// Typ dla żądania wraz z listą uczestników
	int groupInfo_blocklengths[3] = {1, 1, maxNumParticipants};
	MPI_Datatype groupInfo_types[3] = {MPI::INT, MPI::INT, MPI::BOOL};
	MPI_Aint groupInfo_offsets[3];

	groupInfo_offsets[0] = offsetof(groupInfoData, id);
	groupInfo_offsets[1] = offsetof(groupInfoData, lamport);
	groupInfo_offsets[2] = offsetof(groupInfoData, participants);

	MPI_Type_create_struct(3, groupInfo_blocklengths, groupInfo_offsets, groupInfo_types, &this->mpi_group_info_type);
	MPI_Type_commit(&this->mpi_group_info_type);
}

void Communication::run() {
	int i;
	MPI_Request recvGroupInfo;
	MPI_Request recvRequests[5];
	struct requestData recvRequestData[5];
	struct groupInfoData recvGroupInfoData;
	
	// Ustaw odbieranie wiadomości każdego typu
	for (i = 0; i < 5; i++)
		MPI_Irecv(&recvRequestData[i], 1, this->mpi_request_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);

	MPI_Irecv(&recvGroupInfoData, 1, this->mpi_group_info_type, MPI_ANY_SOURCE, TAG_GROUP_CREATED, MPI_COMM_WORLD, &recvGroupInfo);


	while(1) 
	{
		//Jeżeli wątek logiczny zgłasza, że zmienił się jego stan
		if (*this->status != this->localStatus) 
		{
			//Student wlaśnie postanowił, że chce wziąć udział w zawodach
			if (*this->status == WAITING) 
			{
				// Wyślij żądanie do wszystkich procesów (informujące że chcę zacząć pić czyli zająć arbitra) 
				this->broadcastToAll(TAG_START_REQUEST);
				//Dodaj wszystkie procesy na liste procesów, od ktorych oczekuje odpowiedzi (potwierdzenia przyjęcia żądania)
				this->addAllToAwaitingAnswerList();
				// Dodaj moje żądanie na lokalną kolejkę żądań
			} 
			//Student właśnie padł, i chce opuścić grupę
			else if (*this->status == DEAD) 
			{ 
				//Jeżeli jest w niej sam, to ją rozwiązuje
				if (this->MyGroupEmpty()) 
				{
					cout << this->mpiRank << " ( " << *this->myLamport << " ) wszyscy padli i jestem sam w grupie, rozwiazuje ja i zwalniam arbitra. " << endl;
					//Poinformuj wszystkich, ze rozwiazuje grupę i zwalniam arbitra
					this->broadcastToAll(TAG_GROUP_RESOLVED);
					//Zwalniam arbitra
					this->arbiters += 1;
					//Informuje watek logiczny ze nie uczestniczy juz w zawodach
					*this->status = PASSIVE;
				} 
				//Jeżeli nie jestem sam w grupie, to informuje pozostałych, że chcę ją opuścić
				else 
				{
					//Zapamietaj, kiedy padles
					this->timeOfDeath = *this->myLamport;
					// Wyślij do wszystkich członków grupy (tych co w niej nadal są), że chcę opuścić grupę
					this->broadcastToMyGroup(TAG_DIE_REQUEST);
					//Dodaj wszystkie procesy na liste procesów, od ktorych oczekuje odpowiedzi (potwierdzenia przyjęcia żądania)
					this->addMyGroupToAwaitingAnswerList();
					this->printMe(); cout << "informuje ";
					for(list<int>::iterator i = awaitingAnswerList.begin(); i!=awaitingAnswerList.end(); i++)
						cout << *i << " "; 
					cout<<"ze chce opuscic grupe." << endl;
				}
			}
			//Zaktualizuj status studenta, zeby wiedziec, że zmiana została już obsłużona
			this->localStatus = *this->status;
		} 
		//Jezeli wątek logiczny nie zgłosił zmiany stanu studenta, to sprawdzamy, czy nie ma wiadomości do obsłużenia
		else 
		{
			int ready = 0;
			bool anyready = false;
			// Sprawdź czy któryś z typów jest gotowy do odbioru
			for (i = 0; i < 5; i++) 
			{
				MPI_Test(&recvRequests[i], &ready, MPI_STATUS_IGNORE);
				if (ready) 
				{
					// Ustaw mój zegar Lamporta
					*this->myLamport = max(recvRequestData[i].lamport, *this->myLamport) + 1;
					// Obsłuż wiadomość i przygotuj się do odbioru kolejnej wiadomości
					this->HandleRequestMessage(i, &recvRequestData[i]);
					anyready = true;
					MPI_Irecv(&recvRequestData[i], 1, this->mpi_request_type, MPI_ANY_SOURCE, i, MPI_COMM_WORLD, &recvRequests[i]);
				}
			}
			// Sprawdź też dla gupiego typu z dodatkową tablicą
			MPI_Test(&recvGroupInfo, &ready, MPI_STATUS_IGNORE);
			if (ready) 
			{
				// Ustaw mój zegar Lamporta
				*this->myLamport = max(recvGroupInfoData.lamport, *this->myLamport) + 1;
				// Obsłuż wiadomość i przygotuj się do odbioru kolejnej wiadomości
				this->HandleGroupInfoMessage(&recvGroupInfoData);
				anyready = true;
				MPI_Irecv(&recvGroupInfoData, 1, this->mpi_group_info_type, MPI_ANY_SOURCE, TAG_GROUP_CREATED, MPI_COMM_WORLD, &recvGroupInfo);
			}
			if (!anyready)
				usleep(500000);
		}
	}
}


void Communication::HandleRequestMessage(int tag, struct requestData* data) {
	struct requestData sender = *data;

	switch(tag) {
		//Potwierdzenie od innego procesu, że otrzymał moje żądanie do sekcji
		case TAG_START_RESPONSE:
			// Usuń nadawcę z kolejki oczekiwań
			this->awaitingAnswerList.remove(sender.id);
			//Jeżeli chcę i mogę wejść do sekcji po arbitra, to wchodzę i próbuję utworzyć grupę
			if (this->localStatus == WAITING && this->awaitingAnswerList.empty() && this->firstOnQueue()) 
			{
				//Jeżeli jest conajmniej dwoch chetnych (łącznie ze mną_ to próbuję utworzyć grupę
				if (this->requestsQueue.size() >= 2)
				{
					this->printMe(); cout << "zaraz sprobuje utworzyc grupe,  moja kolejka przed wejsciem do sekcji to : "; this->requestsQueue.print(); cout << endl;
					this->waitingForArbiter = !this->tryToCreateGroup();
				}
				//Jeżeli tylko ja chcę pić, to muszę poczekać na więcej chętnych przed utworzeniem grupy
				else
				{
					this->printMe(); cout << "tylko ja chcę pić, muszę poczekać na więcej chętnych." << endl;
					this->waitingForParticipants = true;
				}
			}
			break;

		//Żądanie innego procesu do sekcji po arbitra
		case TAG_START_REQUEST:
			// Dodaj do lokalnej kolejki żądanie nadawcy, o ile nie jest przedawnione (czyli gdy wiemy juz, ze nadwaca po wyslaniu wiadomosci zostal przydzielony do grupy i zrezygnowal z sekcji)
			if (sender.lamport >= this->outdated[sender.id])
			{
				//Dodaj nowe żądanie na kolejkę
				this->requestsQueue.push(sender);
				//Jeżeli czekałem z utworzeniem grupy na kolejnego chętnego, to spróbuję utworzyć grupę
				if (this->waitingForParticipants)
				{
					this->waitingForParticipants = false;
					this->printMe(); cout << "jest juz kolejny chętny, próbuję utworzyć grupę!" << endl;
					this->waitingForArbiter = !this->tryToCreateGroup();
				}
				//jeżeli nie, to wysyłam potwierdzenie odebrania żądania
				else
				{
					//this->printMe(); cout << "proces " << sender.id << " ( " << sender.lamport << " ) chce wziac udzial w zawodach" << endl;
					this->sendMessage(sender.id, TAG_START_RESPONSE);
				}
			}
			else
			{
				//this->printMe(); cout << "otrzymalem przedawnionego requesta ("<<sender.lamport<<" < " << this->outdated[sender.id]  <<") informujacego o checi wziecia udzialu od " << sender.id << endl;
			}
			break;

		//Zgoda od innego procesu na opuszczenie grupy
		case TAG_DIE_RESPONSE:
			//Otrzymałem zgode na opuszczenie grupy, sprawdzam czy nadal jestem z nadwaca w grupie (mogłem już ją wcześniej opuścić)
			if (this->myGroup[this->mpiRank] && this->myGroup[sender.id]) 
			{
				this->printMe(); cout << "proces " << sender.id << " pozwolił mi opuścić grupę. " << endl;
				// Już jestem pewien, że mogę opuścić grupę, nie muszę czekać na inne zgody
				this->awaitingAnswerList.clear();
				// Wyczyść skład grupy
				for (int i = 0; i < this->mpiSize; i++) 
					this->myGroup[i] = false;
				//Informuje watek logiczny ze nie uczestniczy juz w zawodach
				this->localStatus = PASSIVE;
				*this->status = PASSIVE;
			}
			break;

		//Prośba innego procesu o zgodę, aby mógł opuścić grupę
		case TAG_DIE_REQUEST:
			//Jeżeli nadal piję i jestem z tym procesem w grupie
			if (this->localStatus == DRINKING && inMyGroup(sender.id)) 
			{	
				//Wyslij zgodę na opuszczenie grupy
				this->sendMessage(sender.id, TAG_DIE_RESPONSE);
				// Wyrzuć wysyłającego z grupy
				this->myGroup[sender.id] = false;
				//this->printMe(); cout << "wysylam zgodę na opuszczenie grupy procesowi " << sender.id << endl;
			} 
			//Jeżeli padłem już ale nadal jestem z tym procesem w grupie
			else if (this->localStatus == DEAD && inMyGroup(sender.id)) 
			{
				//Jeżeli oboje padliśmy, to odpowiedzialność za zwolnienie arbitra przypada na tego, który później padł, jeżeli padli w tym samym czasie to decyduje mpiRank
				if (this->timeOfDeath > sender.lamport || (this->timeOfDeath == sender.lamport && this->mpiRank > sender.id))
				{
					//Wyslij zgodę na opuszczenie grupy
					this->sendMessage(sender.id, TAG_DIE_RESPONSE);
					// Wyrzuć wysyłającego z grupy
					this->myGroup[sender.id] = false;
					//this->printMe(); cout << "wysylam zgodę na opszczenie grupy procesowi " << sender.id << ", czy zostalem sam w grupie? " << this->MyGroupEmpty() << endl;
					//Jeżeli zostałem sam w grupie to rozwiazuję ją i zwalniam arbitra
					if (this->MyGroupEmpty()) 
					{
						this->printMe(); cout << "pozwolilem pozostalym opuscic grupe, zostalem w niej sam, rozwiazuję ją i zwalniam arbitra" << endl;
						//Poinformuj wszystkich, ze rozwiazuje grupę i zwalniam arbitra
						this->broadcastToAll(TAG_GROUP_RESOLVED);
						//W tym momencie na pewno nie czekam na żadne odpowiedzi/potwierdzenia
						this->awaitingAnswerList.clear();
						//Zwalniam arbitra
						this->arbiters += 1;
						//Informuje watek logiczny ze nie uczestniczy juz w zawodach	
						this->localStatus = PASSIVE;
						*this->status = PASSIVE;
					}
				}
				else
				{
					//this->printMe(); cout << "padlem juz, moja grupe chce opuscic " << sender.id << " ale ja mu nie wysylam zgody!" << endl;
				}
			}
			break;


		//Wiadomość, że inny proces zwolnił arbitra (bo rozwiązał grupę)
		case TAG_GROUP_RESOLVED:
			//Rozwiazano grupę, jest o 1 arbitra więcej
			this->arbiters +=1;
			//this->printMe(); cout << "proces " << sender.id << " zwolnil arbitra jest ich teraz " <<  this->arbiters << " dostepnych." << endl;
			// Jeśli czekałem na arbitra, spróbuję jeszcze raz utworzyć grupę
			if (this->waitingForArbiter) 
				this->waitingForArbiter = !this->tryToCreateGroup();
			break;

	}
}

//Funkcja obslugi wiadomosci informujacej, ze inny proces utworzyl grupe
void Communication::HandleGroupInfoMessage(struct groupInfoData* data)
{
	struct groupInfoData sender = *data;
	bool participate = sender.participants[this->mpiRank];
	// Jeśli jestem w grupie, to informuję wątek logiczny, że może zacząć pić
	if (participate) 
	{
		this->localStatus = DRINKING;
		*this->status = DRINKING; 
		//Skoro właśnie dowiedziałem się, że już jestem w grupie, to nie czekam już na żadne odpowiedzi
		this->awaitingAnswerList.clear();
	}
	// Dla wszystkich członków nowo powstałej grupy usuń ich żądania dostępu do sekcji krytycznej OPEN
	this->requestsQueue.removeParticipants(sender.participants);

	this->printMe(); cout << "proces " << sender.id << " utworzyl grupe o skladzie: ";
	// Ustawianie wartosci outdated dla wszytskich czlonkow grupy, aby uniknac dodania ich spóźnionych żądań do kolejki,
	// Dodatkowo, jeśli sam jestem w tej grupie, dodaję wszystkich członków do mojej lokalnej zmiennej określającej grupę	
	for(int i = 0; i < maxNumParticipants; i++)
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
		cout << ", jestem w niej!";
    cout << endl;
	//Zmniejszenie liczby arbitrów, bo jeden został zajęty przez nową grupę
	this->arbiters -= 1;
	// Jeżeli nie jestem w nowo utworzonej grupie, ale chcę wziąć udział w zawodach
	// to sprawdzam czy jestem pierwszy na swojej kolejce żądań oraz czy nie oczekuję na żadną odpowiedź
	// jeżeli te warunki są spełnione - próbuję stworzyć nową grupę
	if (!participate && this->localStatus == WAITING && this->awaitingAnswerList.empty() && this->firstOnQueue())
   	{		
		//Jeżeli jest conajmniej dwoch chetnych (łącznie ze mną_ to próbuję utworzyć grupę
		if (this->requestsQueue.size() >= 2)
		{
			this->printMe(); cout << "nie zostalem przydzielony do nowej grupy, zaraz sprobuje utworzyc kolejną,  moja kolejka przed wejsciem do sekcji to : "; this->requestsQueue.print(); cout << endl;
			this->waitingForArbiter = !this->tryToCreateGroup();
		}
		//Jeżeli tylko ja chcę pić, to muszę poczekać na więcej chętnych przed utworzeniem grupy
		else
		{
			this->printMe(); cout << "nie zostałem przydzielony do nowej grupy, ale tylko ja chcę pić, muszę poczekać na więcej chętnych." << endl;
			this->waitingForParticipants = true;
		}
	}
}

bool Communication::tryToCreateGroup() 
{
	// Czy sa jacys dostępni arbitrzy?
	if (this->arbiters > 0) 
	{	
		//Ustal kto jest członkiem grupy (wszyscy, ktorych żądania są na kolejce, łącznie ze mną)
		this->determineGroupMembers();
		this->printMe(); cout << "jest " << this->arbiters << " arbitrow, tworze grupe o skladzie: "; this->printMyGroup(); cout << endl;
		//Wyslij do wszystkich, ze utworzylem grupe wraz z informacją, kto jest w jej składzie
		this->broadcastToAll(TAG_GROUP_CREATED);
		//Zajmij jednego arbitra
		this->arbiters -= 1;
		//Poinformuj wątek logiczny że został przydzielony do grupy i moze zacząć pić
		this->localStatus = DRINKING;
		*this->status = DRINKING;
		return true;
	} 
	else 
	{
		this->printMe(); cout << "próbowałem utworzyć grupę, ale brakuje arbitrów." << endl;
		return false;
	}
}


void Communication::printMe()
{
	cout << this->mpiRank << " ( " << *this->myLamport << " ) ";
}

void Communication::printMyGroup()
{
	for (int i = 0; i < maxNumParticipants; i++) 
			if (this->myGroup[i])
				cout << i << " ";  
}

void Communication::broadcastToAll(int tag)
{
	int lamportCopy = *this->myLamport;
	if (tag == TAG_GROUP_CREATED)
	{	
		//Przygotowanie wiadomosci do wysłania, ..
		struct groupInfoData myMsg;
		myMsg.id = this->mpiRank;
		myMsg.lamport = lamportCopy;
		//.. w tym uzupelnienie informacji, kto jest w grupie
		for(int i = 0; i < mpiSize; i++)
				myMsg.participants[i] = inMyGroup(i);
		//Wysylanie wiadomosci o utworzeniu grupy do wszytskich (poza soba)
		for(int i = 0; i < this->mpiSize; i++)
			if(this->mpiRank != i)
				MPI_Send(&myMsg, 1, this->mpi_group_info_type, i, tag, MPI_COMM_WORLD);
	}
	else
	{
		//Przygotowanie requesta do wyslania	
		struct requestData myMsg;
		myMsg.id = this->mpiRank;
		myMsg.lamport = lamportCopy;
		//Wysylanie requesta do wszytskich (poza soba)
		for(int i = 0; i < this->mpiSize; i++)
			if(this->mpiRank != i)
				MPI_Send(&myMsg, 1, this->mpi_request_type, i, tag, MPI_COMM_WORLD);
		//Jeżeli wysyłam żądanie dostępu do sekcji, to muszę je też dodać do swojej kolejki
		if (tag == TAG_START_REQUEST)
			this->requestsQueue.push(myMsg);
	}
	//Zwiększenie zegara Lamporta
	*this->myLamport += 1;
}

void Communication::broadcastToMyGroup(int tag)
{
	//Przygotowanie requesta do wyslania	
	int lamportCopy = *this->myLamport;
	struct requestData myMsg;
	myMsg.id = this->mpiRank;
	myMsg.lamport = lamportCopy;
	//Wysylanie requesta do wszytskich ktorzy nadal sa ze mna w grupie (poza soba)
	for(int i = 0; i < this->mpiSize; i++)
		if(this->mpiRank != i && this->myGroup[i])
			MPI_Send(&myMsg, 1, this->mpi_request_type, i, tag, MPI_COMM_WORLD);
	//Zwiększenie zegara Lamporta
	*this->myLamport += 1;
}

void Communication::sendMessage(int sendTo, int tag)
{
	//Przygotowanie requesta do wyslania	
	int lamportCopy = *this->myLamport;
	struct requestData myMsg;
	myMsg.id = this->mpiRank;
	myMsg.lamport = lamportCopy;
	//Wyslanie wiadomosci do procesu o mpiRank równym sendTo
	MPI_Send(&myMsg, 1, this->mpi_request_type, sendTo, tag, MPI_COMM_WORLD);
	//Zwiększenie zegara Lamporta
	*this->myLamport += 1;
}

void Communication::addAllToAwaitingAnswerList()
{
	//Wyczyść listę
	this->awaitingAnswerList.clear();
	//Dodaj wszystkie procesy (poza sobą samym) na listę procesów, na których odpowiedż czekam
	for(int i = 0; i < this->mpiSize; i++)
		if(i != this->mpiRank)
			this->awaitingAnswerList.push_back(i);
}

void Communication::addMyGroupToAwaitingAnswerList()
{
	//Wyczyść listę
	this->awaitingAnswerList.clear();
	//Dodaj wszystkie procesy z mojej grupy (poza sobą samym) na listę procesów, na których odpowiedż czekam
	for(int i = 0; i < this->mpiSize; i++)
		if(i != this->mpiRank && inMyGroup(i))
			this->awaitingAnswerList.push_back(i);
}

bool Communication::firstOnQueue()
{
	return this->requestsQueue.first(this->mpiRank);
}

void Communication::determineGroupMembers()
{
	int id;
	//Określ, na podstawie zawartości kolejki żądań skład tworzonej grupy (wszystkie procesy oczekujące tworzą grupę)
	while (!this->requestsQueue.empty()) 
	{
		id = this->requestsQueue.top().id;
		this->myGroup[id] = true;
		this->requestsQueue.pop();
	}	
}

bool Communication::MyGroupEmpty() 
{
	for (int i = 0; i < maxNumParticipants; i++) 
		if (i != this->mpiRank && this->myGroup[i]) 
			return false;
	return true;
}

bool Communication::inMyGroup(int id)
{
	return this->myGroup[id];
}

Communication::~Communication() 
{
	MPI_Type_free(&this->mpi_request_type);
}
