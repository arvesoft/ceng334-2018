#include "do_not_submit.h"
#include <pthread.h>
#include <semaphore.h>
#define EMPTY '-'
#define FOOD 'o'
#define ANT '1'
#define ANTwFOOD 'P'
#define ANTwSLEEP 'S'
#define ANTwFOODwSLEEP '$'
#define IMPOSSIBLE 'Z'
#define TIRED 'U'
typedef sem_t Semaphore;



struct Coordinate
{
	int x;
	int y;
} typedef Coordinate;


int numberOfAnts;
int numberOfFoods;
int maxTimeInSeconds;
Semaphore cellGuards[GRIDSIZE][GRIDSIZE];
pthread_t* antThreads;
int* antIDs;
Coordinate* antCoordinates;
size_t termination_time;
bool isRunning = true;

pthread_mutex_t sleeping_mutex;
pthread_cond_t sleeping_condition;

void readFromCommandLine(char* argv[])
{
	numberOfAnts = atoi(argv[1]);
	numberOfFoods = atoi(argv[2]);
	maxTimeInSeconds = atoi(argv[3]);
	printf("%d %d %d\n", numberOfAnts, numberOfFoods, maxTimeInSeconds);
}

void initializeGridEmpty()
{
	int i,j;
    for (i = 0; i < GRIDSIZE; i++)
    {
        for (j = 0; j < GRIDSIZE; j++)
        {
            putCharTo(i, j, EMPTY);
        }
    }
}

void initializeFoods(int foodCount)
{
	int a,b;
	for (int i = 0; i < foodCount; i++)
	{
		do {
            a = rand() % (GRIDSIZE-1);
            b = rand() % (GRIDSIZE-1);
        }while (lookCharAt(a,b) != '-');
		putCharTo(a, b, FOOD);
	}
	
	//~ putCharTo(0, 0, FOOD);
	//~ putCharTo(0, 1, FOOD);
	//~ putCharTo(0, 2, FOOD);
	
	//~ putCharTo(1, 0, FOOD);
	//~ putCharTo(1, 2, FOOD);
	//~ putCharTo(1, 2, FOOD);
	
	//~ putCharTo(2, 0, FOOD);
	//~ putCharTo(2, 1, FOOD);
	//~ putCharTo(2, 2, FOOD);
	//~ putCharTo(3, 3, FOOD);
	//~ putCharTo(4, 4, FOOD);
	
	//~ putCharTo(0, 0, FOOD);
	//~ putCharTo(0, 1, FOOD);
	//~ putCharTo(0, 2, FOOD);
	//~ putCharTo(0, 3, FOOD);
	//~ putCharTo(0, 4, FOOD);
	//~ putCharTo(0, 5, FOOD);
	//~ putCharTo(0, 6, FOOD);
	
	//~ putCharTo(1, 7, FOOD);
	//~ putCharTo(2, 8, FOOD);
	//~ putCharTo(3, 9, FOOD);
	
	//~ putCharTo(4, 9, FOOD);
	//~ putCharTo(5, 9, FOOD);
	//~ putCharTo(6, 9, FOOD);
	
	//~ putCharTo(7, 8, FOOD);
	//~ putCharTo(8, 7, FOOD);
	//~ putCharTo(9, 6, FOOD);
	
	//~ putCharTo(9, 5, FOOD);
	//~ putCharTo(9, 4, FOOD);
}

Coordinate coordinateCreator(int x, int y)
{
	Coordinate result;
	result.x = x;
	result.y = y;
	return result;
}

Coordinate* initializeAnts(int antCount)
{
	Coordinate* coordinates;
	coordinates = malloc(sizeof(Coordinate) * antCount);
	int a,b;
	for (int i = 0; i < antCount; i++)
	{
		do {
            a = rand() % (GRIDSIZE-1);
            b = rand() % (GRIDSIZE-1);
        }while (lookCharAt(a,b) != '-');
		
		//~ putCharTo(a, b, ANT);
		Coordinate current = coordinateCreator(a,b);
		coordinates[i] = current;
	}
	return coordinates;
}

void initializeSemaphores(int numberOfAnts)
{
	pthread_mutex_init(&sleeping_mutex, NULL);
	pthread_cond_init(&sleeping_condition, NULL);
	for (int i=0; i < GRIDSIZE; i++)
	{
		for (int j=0; j < GRIDSIZE; j++)
		{
			sem_init(&cellGuards[i][j], 1, 1);
		}
	}
}

bool canMoveTo(const int* x, const int* y)
{
	int xVal = *x;
	int yVal = *y;
	if (xVal >= 0 && yVal >= 0 && xVal<GRIDSIZE && yVal<GRIDSIZE)
		return true;
	else
		return false;
}

char* neighborBringer(const int* y, const int* x)
{
	int xVal = *y;
	int yVal = *x;
	int leftX = xVal-1;
	int rightX = xVal+1;
	int upperY = yVal-1;
	int lowerY = yVal+1;
	
	// eight neighbors
	char* neighbors = malloc(sizeof(char)*8);
	int i = 0;
	char obj;
	
	// upperNeighbors
	obj = IMPOSSIBLE;
	if ( canMoveTo(&upperY, &leftX) )
		obj = lookCharAt(upperY, leftX);
	neighbors[i++] = obj;
		
	obj = IMPOSSIBLE;
	if ( canMoveTo(&upperY, &xVal) )
		obj = lookCharAt(upperY, xVal);
	neighbors[i++] = obj;
	
	obj = IMPOSSIBLE;
	if ( canMoveTo(&upperY, &rightX) )
		obj = lookCharAt(upperY, rightX);
	neighbors[i++] = obj;
	
	// middle neighbors
	obj = IMPOSSIBLE;
	if ( canMoveTo(&yVal, &leftX) )
		obj = lookCharAt(yVal, leftX);
	neighbors[i++] = obj;
	
	obj = IMPOSSIBLE;
	if ( canMoveTo(&yVal, &rightX) )
		obj = lookCharAt(yVal, rightX);
	neighbors[i++] = obj;
		
	// bottom neighbors
	obj = IMPOSSIBLE;
	if ( canMoveTo(&lowerY, &leftX) )
		obj = lookCharAt(lowerY, leftX);
	neighbors[i++] = obj;
	
	obj = IMPOSSIBLE;
	if ( canMoveTo(&lowerY, &xVal) )
		obj = lookCharAt(lowerY, xVal);
	neighbors[i++] = obj;
	
	obj = IMPOSSIBLE;
	if ( canMoveTo(&lowerY, &rightX) )
		obj = lookCharAt(lowerY, rightX);
	neighbors[i++] = obj;
	
	return neighbors;
}


void coordinateUpdaterByDestination(int destination, int* x, int* y)
{
	// upper coordinates
	if (destination == 0)
	{
		*x = (*x) +1;
		*y = (*y) +1;
	}
	else if (destination == 1)
	{
		*y = (*y) +1;
	}
	else if (destination == 2)
	{
		*x = (*x) -1;
		*y = (*y) +1;
	}
	
	// middle coordinates
	else if (destination == 3)
	{
		*x = (*x) +1;
	}
	else if (destination == 4)
	{
		*x = (*x) -1;
	}
	
	// below coordinates
	else if (destination == 5)
	{
		*x = (*x) +1;
		*y = (*y) -1;
	}
	else if (destination == 6)
	{
		*y = (*y) -1;
	}
	else if (destination == 7)
	{
		*x = (*x) -1;
		*y = (*y) -1;
	}
}


bool actionApplier(int* x, int* y, int code, char wantedCharacter, char leftOverCharacter, char updatedStatus, bool updateFlag)
{	
	int destX = *x;
	int destY = *y;
	int reverseDirection = 7-code;
	coordinateUpdaterByDestination(reverseDirection, &destY, &destX);
	
	// early termination, exit if cannot move there
	if ( !canMoveTo(&destY, &destX) )
		return false;
		
	sem_wait(&cellGuards[destY][destX]);
	
	bool flag = false;
	if ( canMoveTo(&destY, &destX) )
	{
		//~ printf("A%d ", code);
		char obj = lookCharAt(destY, destX);
		if ( obj == wantedCharacter )
		{
			if (updateFlag)
			{
				//~ printf("current: %d %d\n", *x, *y);
				putCharTo(*y,*x, leftOverCharacter);
				*x = destX;
				*y = destY;
				//~ printf("updated: %d %d\n", *x, *y);
				putCharTo(*y,*x, updatedStatus);
			}
			flag = true;
		}
	}
	sem_post(&cellGuards[destY][destX]);
	
	return flag;
}

bool randomActionTaker(int* x, int* y, char wantedCharacter, char leftOverCharacter, char updatedStatus)
{
	int index = 0;
	int list[] = {-1,-1,-1,-1,-1,-1,-1,-1};
	while(index < 8)
	{
		int a = rand() % 8;
		bool flag = true;
		for (int j=0; j<8; j++)
		{
			if (list[j] == a)
				flag = false;
		}
		if (flag == true)
		{
			list[index] = a;
			index ++;
		}
	}
	for (int i=0; i<8 ; i++)
	{
		int direction = list[i];
		if ( actionApplier(x,y,direction, wantedCharacter, leftOverCharacter, updatedStatus, true) )
			return true;
	}
	return false;
}

bool actionTaker(int* x, int* y, char wantedCharacter, char leftOverCharacter, char updatedStatus)
{	
	for (int i=0; i<8; i++)
	{
		if ( actionApplier(x, y, i, wantedCharacter, leftOverCharacter, updatedStatus, true) )
			return true;
	}
	return false;
}

bool doIHaveFoodNeighbor(int x, int y)
{
	char wantedCharacter = FOOD;
	char leftOverCharacter = FOOD;
	char updatedStatus = FOOD;
	for (int i=0; i<8; i++)
	{
		if ( actionApplier(&x, &y, i, wantedCharacter, leftOverCharacter, updatedStatus, false) )
			return true;
	}
	return false;
}

void statusANT(char* status, int* x, int* y)
{
	bool foundFood = false;
	// ANT want FOOD, found ---> ANTwFOOD
	// find FOOD
	// change previous place with EMPTY
	// replace FOODs place with ANT
	foundFood = actionTaker(x,y, FOOD, EMPTY, ANTwFOOD );
	if (foundFood)
		*status = ANTwFOOD;
	
	if (!foundFood)
		randomActionTaker(x,y, EMPTY, EMPTY, ANT);
}



void statusANTwFOOD(char* status, int* x, int* y)
{
	bool foundFood = false;
	int xDummy = *x;
	int yDummy = *y;
	// only check for food around, do not move yourself
	foundFood = doIHaveFoodNeighbor(xDummy, yDummy);
	if (foundFood)
	{
		// now move yourself
		randomActionTaker(x,y, EMPTY, FOOD, ANT);
		*status = TIRED;
	}
	
	if (!foundFood)
	{
		randomActionTaker(x,y, EMPTY, EMPTY, ANTwFOOD );
	}
}

void statusTIRED(char* status, int* x, int* y)
{
	randomActionTaker(x,y, EMPTY, EMPTY, ANT );
	*status = ANT;
}

int getRandomDelay()
{
	int baseDelay = getDelay();
	int randomDelay = rand() % 10;
	int total = baseDelay + randomDelay;
	return total*1000;
}

void* antRoutine(void* void_ptr)
{
	// starting status
	int myID = *(int*) void_ptr;
	Coordinate coord = antCoordinates[myID];
	int x = coord.x ;
	int y = coord.y;
	putCharTo(y,x, ANT);
	char status = ANT;
	bool sleepFlag;
	while (isRunning)
	{	
		sleepFlag = true;
		while ( sleepFlag )
		{
			pthread_mutex_lock(&sleeping_mutex);
			int currentSleeping = getSleeperN();
			pthread_mutex_unlock(&sleeping_mutex);
			
			if (myID < currentSleeping)
			{
				pthread_mutex_lock(&sleeping_mutex);
				if (status == ANT)
					putCharTo(y,x, ANTwSLEEP);
				else
					putCharTo(y,x, ANTwFOODwSLEEP);
				pthread_cond_wait(&sleeping_condition, &sleeping_mutex);
				pthread_mutex_unlock(&sleeping_mutex);
			}
			else
			{
				putCharTo(y,x, status);
				sleepFlag = false;
			}
		}
			
		if (status == ANT)
			statusANT(&status, &x, &y);
			
		else if (status == ANTwFOOD)
			statusANTwFOOD(&status, &x, &y);
			
		else if (status == TIRED)
		{
			statusTIRED(&status, &x, &y);
		}
		
		usleep(getDelay() * 1000 + (rand() % 5000));
	}
	
	
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    
    readFromCommandLine(argv);
    initializeSemaphores(numberOfAnts);
    initializeGridEmpty();
    initializeFoods(numberOfFoods);
	antCoordinates = initializeAnts(numberOfAnts);
    
    

    antThreads = malloc(sizeof(pthread_t)*numberOfAnts);
	antIDs = malloc(sizeof(int)*numberOfAnts);
    
    for(int i=0; i<numberOfAnts; i++)
    {
		antIDs[i] = i;
        pthread_create(&antThreads[i], NULL, antRoutine, &antIDs[i] );
    }

	struct timespec time_start;
	clock_gettime(CLOCK_MONOTONIC, &time_start);

	bool debug = 0;
	if (!debug)
	{
		startCurses();
		char c;
		while (TRUE) {
			
			struct timespec time_now;
			clock_gettime(CLOCK_MONOTONIC, &time_now);
			double elapsed = (time_now.tv_sec - time_start.tv_sec) +
				(time_now.tv_nsec - time_start.tv_nsec) / 1.0e9;
			if (elapsed > maxTimeInSeconds)
				break;
			drawWindow();
			
			c = 0;
			c = getch();

			if (c == 'q' || c == ESC) break;
			if (c == '+') {
				setDelay(getDelay()+10);
			}
			if (c == '-') {
				setDelay(getDelay()-10);
			}
			if (c == '*') {
				pthread_mutex_lock(&sleeping_mutex);
				setSleeperN(getSleeperN()+1);
				pthread_mutex_unlock(&sleeping_mutex);
			}
			if (c == '/') {
				pthread_mutex_lock(&sleeping_mutex);
				setSleeperN(getSleeperN()-1);
				pthread_mutex_unlock(&sleeping_mutex);
				pthread_cond_broadcast(&sleeping_condition);
			}
			
			pthread_mutex_lock(&sleeping_mutex);
			int currentSleeping = getSleeperN();
			pthread_mutex_unlock(&sleeping_mutex);
			
			usleep(50000);			
		}
		endCurses();
    }
    
    isRunning = false;
    usleep(50000);
	
	for (size_t i = 0; i < numberOfAnts; i++) {
		while (pthread_join(antThreads[i], NULL))
			;
	}	
	
    free(antCoordinates);
    free(antThreads);
    free(antIDs);
    return 0;
}
