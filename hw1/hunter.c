#include <stdio.h>
#include <unistd.h>
#include "definitions.h"

int IsBlocked(const coordinate a, const int count, const coordinate* blocked_coordinates) {
  for(int i=0; i<count; i++) {
    if(a.x == blocked_coordinates[i].x && a.y == blocked_coordinates[i].y) {
        return 1;
    }
  }
  return 0;
}

void FillMessage(ph_message* const phm, const coordinate cur_loc ,const int move_count, const coordinate* possible_moves) {
  if(move_count > 0) {
    phm->move_request = possible_moves[0];
  } else {
    phm->move_request = cur_loc;
  }
}

void FindMoves(coordinate* possible_moves, int* move_count, const server_message* sm, const int map_width, const int map_height) {
  int max_distance = mhDistance(sm->pos, sm->adv_pos);
  for(int i=0; i<4; i++) {
    coordinate dest = Move(sm->pos, i);
    if(InRange(dest, map_width, map_height) && !IsBlocked(dest, sm->object_count, sm->object_pos))
    {
      int dest_distance = mhDistance(dest, sm->adv_pos);
      if(dest_distance < max_distance) {
        *(possible_moves+(*move_count)) = dest;
        (*move_count)++;
      }
    }
  }
}

int main(int argc, char* argv[])
{
  int map_width = atoi(argv[0]);
  int map_height = atoi(argv[1]);
  coordinate possible_moves[4];
  int move_count=0;
  ph_message phm;
  server_message sm;
  while(1) {
    move_count = 0;
    read(STDIN_FILENO, &sm, sizeof(sm));
    FindMoves(possible_moves, &move_count, &sm, map_width, map_height);
    FillMessage(&phm, sm.pos, move_count, possible_moves);
    write(STDOUT_FILENO, &phm, sizeof(phm));
    usleep(10000*(1+rand()%9));
  }
  return 0;
}