#include <stdio.h>
#include "definitions.h"

server_message readDataFromServer();

int isBlocked(coordinate a, int count, coordinate* blocked_coordinates, int map_width, int map_height) {
    //TODO : add out of map check
    for(int i=0; i<count; i++) {
        if(a.x == blocked_coordinates[i].x &&
           a.y == blocked_coordinates[i].y) {
               return 1;
           }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if(argc != 3) {
        exit(0);
    }
    int map_width = atoi(argv[1]);
    int map_height = atoi(argv[2]);


    coordinate offset_list[] =  { 
                                    {.x = -1, .y = 0},
                                    {.x = 0, .y = -1},
                                    {.x = 1, .y = 0},
                                    {.x = 0, .y = 1}
                                };

    coordinate cur_loc;

    while(1) {
        // Read the data somehow
        server_message data_received = readDataFromServer(); // = read from pipe/socket?
        cur_loc = data_received.pos;
        int adv_distance = mhDistance(cur_loc, data_received.adv_pos);
        coordinate possible_moves[4];
        int move_count=0;

        for(int i=0; i<4; i++) {
            coordinate dest = addCoordinate(cur_loc, offset_list[i]);
            if(!isBlocked(dest, data_received.object_count, data_received.object_pos, map_width, map_height))
            {
                int dest_distance = mhDistance(dest, data_received.adv_pos);
                if(dest_distance > adv_distance) {
                    possible_moves[move_count] = dest;
                    move_count++;
                }
            }
        }


    }






    return 0;
}