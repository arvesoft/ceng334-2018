/* v0.1 -> added headers, basic functions declarations */
/* headers of the previous year HW1, with new headers of the current HW1 added */

#include "definitions.h"

void hunterProcess();
void preyProcess();

int mapWidth;
int mapHeight;
int numberOfObstacles;
int numberOfHunters;
int numberOfPreys;

char* grid;

void printGrid()
{
	char* sus = malloc(sizeof(char)* (mapWidth+2) );
	sus[0] = sus[mapWidth+1] = '+';
	
	for(int i=0; i<mapWidth; i++)
		sus[i+1] = '-';
	printf("%s\n", sus);
	for (int i=0; i<mapHeight; i++)
	{
		printf("|");
		for(int j=0; j<mapWidth; j++)
		{
			printf("%c", grid[i*mapWidth + j]);
		}
		printf("|\n");
	}
	printf("%s\n", sus);
	
	free(sus);
}
		

//~ int createHunter()
//~ {
	//~ int hunterPID = fork();
	//~ if (hunterPID == 0)
		//~ return 0;
	//~ else
	//~ {
		//~ hunterPIDS[hunterPIDIndex] = hunterPID;
		//~ hunterPIDIndex++;
		//~ return hunterPID;
	//~ }
//~ }

//~ int createPrey()
//~ {
	//~ int preyPID = fork();
	//~ if (preyPID == 0)
		//~ return 0;
	//~ else
	//~ {
		//~ preyPIDS[preyPIDIndex] = preyPID;
		//~ preyPIDIndex++;
		//~ return preyPID;
	//~ }
//~ }

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("no file input provided");
		return -1;
	}
	
	FILE* inputFile = fopen( argv[1] , "r");
	if ( !inputFile )
	{
		printf("%s", "file cannot be read\n");
		return -2;
	}
	else
	{
		char* line;
		size_t lineLength = 0;
		
		// read grid size
		int result;
		result = fscanf(inputFile, "%d %d", &mapWidth, &mapHeight);
		printf("%d %d\n", mapWidth, mapHeight);
		
		// allocate grid
		grid = malloc(sizeof(char) * mapWidth * mapHeight);
		for (int i=0; i<mapHeight; i++)
		{
			for(int j=0; j<mapWidth; j++)
				grid[i*mapWidth + j]= ' ';
		}
		printGrid();
		
		
		result = fscanf(inputFile, "%d", &numberOfObstacles);
		printf("#Obstacles: %d \n", numberOfObstacles);
		
		for (int i=0; i<numberOfObstacles; i++)
		{
			int x, y;
			result = fscanf(inputFile, "%d %d", &x, &y);
			printf("%d %d\n", x, y);
		}
		
		result = fscanf(inputFile, "%d", &numberOfHunters);
		printf("#Hunters: %d\n", numberOfHunters);
		for (int i=0; i<numberOfHunters; i++)
		{
			int x, y, energy;
			result = fscanf(inputFile, "%d %d %d", &x, &y, &energy);
			printf("%d %d %d\n", x, y, energy);
		}
		
		result = fscanf(inputFile, "%d", &numberOfPreys);
		printf("#Preys: %d\n", numberOfPreys);
		for (int i=0; i<numberOfPreys; i++)
		{
			int x, y;
			result = fscanf(inputFile, "%d %d", &x, &y);
			printf("%d %d\n", x, y);
		}
	}
	
	
	//~ int childID = createPrey();
	//~ if ( childID == 0 )
	//~ {
		//~ printf("%s\n", "child");
		//~ return 0;
	//~ }
	//~ else
	//~ {
		//~ printf("%s\n", "parent");
		//~ while (1)
		//~ {
			//~ ;
		//~ }
	//~ }
}
