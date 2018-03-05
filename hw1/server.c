#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "definitions.h"
#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

int map_width;
int map_height;
int number_of_obstacles;
coordinate* obstacles;
int number_of_hunters;
Client* hunters;
int number_of_preys;
Client* preys;
char** map;
int nfds;

void ReadInput() {
  scanf("%d%d", &map_width, &map_height);
  scanf("%d", &number_of_obstacles);
  map = (char**)malloc(sizeof(char*) * map_height);
  for (int i = 0; i < map_height; i++) {
    map[i] = (char*)malloc(sizeof(char) * map_width);
    memset(map[i], ' ', map_width);
  }
  obstacles = (coordinate*)malloc(sizeof(coordinate) * number_of_obstacles);
  for (int i = 0; i < number_of_obstacles; i++) {
    scanf("%d%d", &obstacles[i].x, &obstacles[i].y);
    map[obstacles[i].x][obstacles[i].y] = 'X';
  }
  scanf("%d", &number_of_hunters);
  hunters = (Client*)malloc(sizeof(Client) * number_of_hunters);
  for (int i = 0; i < number_of_hunters; i++) {
    scanf("%d%d%d", &hunters[i].pos.x, &hunters[i].pos.y, &hunters[i].energy);
    hunters[i].client_status = ALIVE;
    map[hunters[i].pos.x][hunters[i].pos.y] = 'H';
  }
  scanf("%d", &number_of_preys);
  preys = (Client*)malloc(sizeof(Client) * number_of_preys);
  for (int i = 0; i < number_of_preys; i++) {
    scanf("%d%d%d", &preys[i].pos.x, &preys[i].pos.y, &preys[i].energy);
    preys[i].client_status = ALIVE;
    map[preys[i].pos.x][preys[i].pos.y] = 'P';
  }
}

void Finish() {
  free(preys);
  free(hunters);
  free(obstacles);
}

void PrintFooter() {
  putchar('+');
  for (int i = 0; i < map_width; i++) putchar('-');
  putchar('+');
  putchar('\n');
}

void PrintMap() {
  PrintFooter();
  for (int i = 0; i < map_height; i++) {
    putchar('|');
    for (int j = 0; j < map_width; j++) putchar(map[i][j]);
    putchar('|');
    putchar('\n');
  }
  PrintFooter();
}

void FillNeighbours(server_message* sm, const char type) {
  sm->object_count = 0;
  for (int i = 0; i < 4; i++) {
    const coordinate neighbour = Move(sm->pos, i);
    if (InRange(neighbour, map_width, map_height)) {
      const char elem = map[neighbour.x][neighbour.y];
      if (elem == type || elem == 'X') {
        sm->object_pos[sm->object_count++] = neighbour;
      }
    }
  }
}

void InformPrey(int idx) {
  Client* const prey = preys + idx;
  server_message sm;
  sm.pos = prey->pos;
  sm.adv_pos = hunters[0].pos;
  int min_distance = mhDistance(sm.pos, sm.adv_pos);
  for (int i = 1; i < number_of_hunters; i++) {
    const int distance = mhDistance(sm.pos, hunters[i].pos);
    if (min_distance > distance) {
      min_distance = distance;
      sm.adv_pos = hunters[i].pos;
    }
  }
  FillNeighbours(&sm, 'P');

  write(prey->pipe_fd[SERVER_FD], &sm, sizeof(server_message));
}

void InformHunter(int idx) {
  Client* hunter = hunters + idx;
  server_message sm;
  sm.pos = hunter->pos;
  sm.adv_pos = preys[0].pos;
  int min_distance = mhDistance(sm.pos, sm.adv_pos);
  for (int i = 1; i < number_of_preys; i++) {
    const int distance = mhDistance(sm.pos, preys[i].pos);
    if (min_distance > distance) {
      min_distance = distance;
      sm.adv_pos = preys[i].pos;
    }
  }
  FillNeighbours(&sm, 'H');
  write(hunter->pipe_fd[SERVER_FD], &sm, sizeof(server_message));
}

void InitializeClients() {
  char width[15];
  char height[15];
  sprintf(width, "%d", map_width);
  sprintf(height, "%d", map_height);
  char* const argv[] = {width, height, NULL};
  for (int i = 0; i < number_of_preys; i++) {
    PIPE(preys[i].pipe_fd);
    preys[i].pid = fork();
    if (!preys[i].pid) {
      dup2(preys[i].pipe_fd[CLIENT_FD], STDIN_FILENO);
      dup2(preys[i].pipe_fd[CLIENT_FD], STDOUT_FILENO);
      close(preys[i].pipe_fd[SERVER_FD]);
      execv("./prey", argv);
    }
    close(preys[i].pipe_fd[CLIENT_FD]);
    nfds = max(nfds, preys[i].pipe_fd[SERVER_FD]+1);
    InformPrey(i);
  }
  for (int i = 0; i < number_of_hunters; i++) {
    PIPE(hunters[i].pipe_fd);
    hunters[i].pid = fork();
    if (!hunters[i].pid) {
      dup2(hunters[i].pipe_fd[CLIENT_FD], STDIN_FILENO);
      dup2(hunters[i].pipe_fd[CLIENT_FD], STDOUT_FILENO);
      close(hunters[i].pipe_fd[SERVER_FD]);
      execv("./hunter", argv);
    }
    close(hunters[i].pipe_fd[CLIENT_FD]);
    nfds = max(nfds, hunters[i].pipe_fd[SERVER_FD]+1);
    InformHunter(i);
  }
}

int BlockedBy(coordinate a, char type) {
  const char elem = map[a.x][a.y];
  if (elem == 'X') {
    return 2; // Blocked By Obstacle
  } else if (elem == type) {
    return 3; // Blocked By Friendly Unit
  } else if (elem == ' ') {
    return 0; // Not Blocked
  } else {
    return 1; // Not Blocked Buy Adversary
  }
}

void Kill(Client* client) {
  //kill(client->pid, SIGTERM);
  //waitpid(client->pid, NULL, 0);
  client->client_status = DEAD;
}

void UpdateMap() {
  for(int i=0; i<map_height; i++) {
    memset(map[i], ' ', map_width);
  }
  for(int i=0; i<number_of_obstacles; i++) {
    map[obstacles[i].x][obstacles[i].y] = 'X';
  }
  for(int i=0; i<number_of_preys; i++) {
    if(preys[i].client_status==ALIVE) {
      map[preys[i].pos.x][preys[i].pos.y] = 'P';
    }
  }
  for(int i=0; i<number_of_hunters; i++) {
    if(hunters[i].client_status==ALIVE) {
      map[hunters[i].pos.x][hunters[i].pos.y] = 'H';
    }
  }
}

void GameLoop() {
  int remaining_hunters = number_of_hunters;
  int remaining_preys = number_of_preys;
  fd_set read_fds;
  FD_ZERO(&read_fds);
  for (int i = 0; i < number_of_preys; i++) {
    FD_SET(preys[i].pipe_fd[SERVER_FD], &read_fds);
  }
  for (int i = 0; i < number_of_hunters; i++) {
    FD_SET(hunters[i].pipe_fd[SERVER_FD], &read_fds);
  }

  while (remaining_hunters && remaining_preys) {
    sleep(1); //REMOVE THIS LATER
    printf("H: %d P: %d \n", remaining_hunters, remaining_preys);
    const int retval = select(nfds, &read_fds, NULL, NULL, NULL);
    if (retval == -1) {
      perror("select failed");
    } else if (!retval) {
      continue;
    }
    int is_map_updated = 0;
    for (int i = 0; i < number_of_preys; i++) {
      if (preys[i].client_status==ALIVE && FD_ISSET(preys[i].pipe_fd[SERVER_FD], &read_fds)) {
        ph_message phm;
        read(preys[i].pipe_fd[SERVER_FD], &phm, sizeof(phm));
        coordinate request = phm.move_request;
        int blocking_unit = BlockedBy(request, 'P');
        if(blocking_unit == 0 || blocking_unit == 1) {
          preys[i].pos.x = request.x;
          preys[i].pos.y = request.y;
          is_map_updated = 1;
        }
        InformPrey(i);
      }
    }
    for (int i = 0; i < number_of_hunters; i++) {
      if (hunters[i].client_status==ALIVE && FD_ISSET(hunters[i].pipe_fd[SERVER_FD], &read_fds)) {
        ph_message phm;
        read(hunters[i].pipe_fd[SERVER_FD], &phm, sizeof(phm));
        coordinate request = phm.move_request;
        hunters[i].energy--;
        printf("Hunter %d Energy: %d\n", i, hunters[i].energy);
        int blocking_unit = BlockedBy(request, 'H');
        if(blocking_unit == 0 || blocking_unit == 1) {
          hunters[i].pos.x = request.x;
          hunters[i].pos.y = request.y;
          is_map_updated = 1;
          if (blocking_unit == 1) { // If Hunter moved onto a prey
            for(int j=0; j<number_of_preys; j++) {
              if(preys[j].client_status == ALIVE && preys[j].pos.x == request.x && preys[j].pos.y == request.y) {
                hunters[i].energy += preys[j].energy;
                Kill(preys+j);
                remaining_preys--;
                break;
              }
            } 
          }
        }
        if(hunters[i].energy <= 0) {
          Kill(hunters+i);
          remaining_hunters--;
          is_map_updated = 1;
        }
        InformHunter(i);
      }
    }

    if(is_map_updated) {
      UpdateMap();
      PrintMap();
    }
  }
}

int main() {
  ReadInput();
  PrintMap();
  InitializeClients();
  GameLoop();
  Finish();
  return 0;
}
