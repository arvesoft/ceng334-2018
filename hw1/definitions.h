#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct server_message {
	coordinate pos;
	coordinate adv_pos;
	int object_count;
	coordinate object_pos[4];
} server_message;

coordinate addCoordinate(coordinate a, coordinate b) {
	coordinate c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
}

int mhDistance(coordinate a, coordinate b) {
	return abs(a.x - b.x) + abs(a.y - b.y);
}

// to write in the process IDs of hunters and preys
pid_t preyPIDS;
pid_t hunterPIDS;
pid_t wantedList;
int* obstacleList;

int preyPIDIndex;
int hunterPIDIndex;

void initializeLists()
{
	//~ // clear the lists to all -1
	//~ for (int i=0; i< numberOfPreys; i++)
	//~ {
		//~ preyPIDS[i] = -1;
	//~ }
	//~ for (int i=0; i < numberOfHunters; i++)
	//~ {
		//~ hunterPIDS[i] = -1;
	//~ }
	//~ for (int i=0; i < totalNumber; i++)
	//~ {
		//~ wantedList[i] = -1;
	//~ }
}
