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
            a = GRIDSIZE-1;
            b = GRIDSIZE-1;
        }while (lookCharAt(a,b) != '-');
		putCharTo(a, b, FOOD);
	}
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
            a = 0;
            b = 0;
        }while (lookCharAt(a,b) != '-');
		
		putCharTo(a, b, ANT);
		//~ printf("a: %d %d\n", a,b);
		Coordinate current = coordinateCreator(a,b);
		coordinates[i] = current;
	}
	return coordinates;
}

void initializeSemaphores()
{
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

char* neighborBringer(const int* x, const int* y)
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
		*x = (*x) -1;
		*y = (*y) -1;
	}
	else if (destination == 1)
	{
		*y = (*y) -1;
	}
	else if (destination == 2)
	{
		*x = (*x) +1;
		*y = (*y) -1;
	}
	
	// middle coordinates
	else if (destination == 3)
	{
		*x = (*x) -1;
	}
	else if (destination == 4)
	{
		*x = (*x) +1;
	}
	
	// below coordinates
	else if (destination == 5)
	{
		*x = (*x) -1;
		*y = (*y) +1;
	}
	else if (destination == 6)
	{
		*y = (*y) +1;
	}
	else if (destination == 7)
	{
		*x = (*x) +1;
		*y = (*y) +1;
	}
}
	

void statusANT(char* status, int* x, int* y)
{
	char* neighbors = neighborBringer(x, y);
	bool foundFood = false;
	for (int i=0; i < 8; i++)
	{
		if (neighbors[i] == FOOD)
		{
			//~ putCharTo(*x,*y, EMPTY);
			coordinateUpdaterByDestination(i,y,x);
			putCharTo(*x,*y, ANTwFOOD);
			*status = ANTwFOOD;
			foundFood = true;
			break;
		}
	}
	
	// cannot find food, migrate to empty cell if possible
	if (!foundFood)
	{
		for (int i=0; i < 8; i++)
		{
			if (neighbors[i] == EMPTY)
			{
				putCharTo(*x,*y, ANT);
				coordinateUpdaterByDestination(i,y,x);
				putCharTo(*x,*y, ANT);
				//~ *status = ANTwSLEEP;
				break;
			}
		}
	}
	
	free(neighbors);
}

void* antRoutine(void* void_ptr)
{
	// starting status
	Coordinate* coordinatePointer = (Coordinate*) void_ptr;
	Coordinate coord = *coordinatePointer;
	int x = coord.x ;
	int y = coord.y;
	char status = ANT;
	
	while (1)
	{
		//~ printf("%d %d\n", x, y);
		usleep(20000);
		if (status == ANT)
			statusANT(&status, &x, &y);
	}
	
	
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    
    readFromCommandLine(argv);

    ////////////////////////////////
    // Fills the grid randomly to have somthing to draw on screen.
    // Empty spaces have to be -.
    // You should get the number of ants and foods from the arguments 
    // and make sure that a food and an ant does not placed at the same cell.
    // You must use putCharTo() and lookCharAt() to access/change the grid.
    // You should be delegating ants to separate threads
    
    initializeSemaphores();
    initializeGridEmpty();
    Coordinate* antCoordinates = initializeAnts(1);
    initializeFoods(1);
    

    pthread_t antThreads[numberOfAnts];
    //~ int antIDs[numberOfAnts];
    
    for(int i=0; i<1; i++)
    {
        pthread_create(&antThreads[i], NULL, antRoutine, &antCoordinates[i] );
        //~ printf("%d\n", i);
    }
    //////////////////////////////

    // you have to have following command to initialize ncurses.
    startCurses();
    
    // You can use following loop in your program. But pay attention to 
    // the function calls, they do not have any access control, you 
    // have to ensure that.
    char c;
    while (TRUE) {
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
            setSleeperN(getSleeperN()+1);
        }
        if (c == '/') {
            setSleeperN(getSleeperN()-1);
        }
        usleep(50000);
        
        // each ant thread have to sleep with code similar to this
        //usleep(getDelay() * 1000 + (rand() % 5000));
    }
    
    // do not forget freeing the resources you get
    endCurses();
    
    return 0;
}
