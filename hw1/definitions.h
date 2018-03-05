#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__
#define CLIENT_FD 0
#define SERVER_FD 1
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#include <math.h>

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

typedef struct ph_message {
  coordinate move_request;
} ph_message;

coordinate addCoordinate(coordinate a, coordinate b) {
  coordinate c;
  c.x = a.x + b.x;
  c.y = a.y + b.y;
  return c;
}

int mhDistance(coordinate a, coordinate b) {
  return abs(a.x - b.x) + abs(a.y - b.y);
}

int InRange(coordinate c, int width, int height) {
  return c.x >= 0 && c.y >= 0 && c.x < width && c.y < height;
}

coordinate Move(coordinate cur, int direction) {
  const int dx[] = {1, -1, 0, 0};
  const int dy[] = {0, 0, 1, -1};
  cur.x += dx[direction];
  cur.y += dy[direction];
  return cur;
}

enum ClientType {
  HUNTER = 0,
  PREY = 1,
};

typedef struct {
  coordinate pos;
  int energy;
  enum ClientType client_type;
  pid_t pid;
  int pipe_fd[2];
} Client;

#endif
