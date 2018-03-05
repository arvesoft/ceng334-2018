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
    hunters[i].client_type = HUNTER;
    map[hunters[i].pos.x][hunters[i].pos.y] = 'H';
  }
  scanf("%d", &number_of_preys);
  preys = (Client*)malloc(sizeof(Client) * number_of_preys);
  for (int i = 0; i < number_of_preys; i++) {
    scanf("%d%d%d", &preys[i].pos.x, &preys[i].pos.y, &preys[i].energy);
    preys[i].client_type = PREY;
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
    nfds = max(nfds, preys[i].pipe_fd[SERVER_FD]);
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
    nfds = max(nfds, preys[i].pipe_fd[SERVER_FD]);
    InformHunter(i);
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
    const int retval = select(nfds, &read_fds, NULL, NULL, NULL);
    if (retval == -1) {
      perror("select failed");
    } else if (!retval) {
      continue;
    }
    for (int i = 0; i < number_of_preys; i++) {
      if (FD_ISSET(preys[i].pipe_fd[SERVER_FD], &read_fds)) {
        // Process preys[i]
      }
    }
    for (int i = 0; i < number_of_hunters; i++) {
      if (FD_ISSET(hunters[i].pipe_fd[SERVER_FD], &read_fds)) {
        // Process hunters[i]
      }
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
