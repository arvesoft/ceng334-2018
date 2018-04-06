#include "do_not_submit.h"
#include <pthread.h>
#include <semaphore.h>
#define EMPTY '-'
#define FOOD 'o'
#define ANT '1'
#define ANTwFOOD 'P'
#define ANTwSLEEP 'S'
#define ANTwFOODwSLEEP '$'


int numberOfAnts;
int numberOfFoods;
int maxTimeInSeconds;

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
            a = rand() % GRIDSIZE;
            b = rand() % GRIDSIZE;
        }while (lookCharAt(a,b) != '-');
		putCharTo(a, b, FOOD);
	}
}

void initializeAnts(int antCount)
{
	int a,b;
	for (int i = 0; i < antCount; i++)
	{
		do {
            a = rand() % GRIDSIZE;
            b = rand() % GRIDSIZE;
        }while (lookCharAt(a,b) != '-');
		putCharTo(a, b, ANT);
	}
}

void* antRoutine(void* void_ptr)
{
	// dummy function for now
	long int a = 1;
	while (a < 1000000000)
	{
		a++;
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
    
    initializeGridEmpty();
    initializeAnts(numberOfAnts);
    initializeFoods(numberOfFoods);
    
    pthread_t antThreads[numberOfAnts];
    int antIDs[numberOfAnts];
    
    for(int i=0; i<numberOfAnts; i++)
    {
        antIDs[i] = i;
        pthread_create(&antThreads[i], NULL, antRoutine, &antIDs[i] );
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
