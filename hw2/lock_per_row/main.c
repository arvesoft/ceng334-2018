#define EMPTY_CELL '-'
#define FOOD_CELL 'o'
#define ANT '1'
#define ANTwFOOD 'P'
#define ANTwSLEEP 'S'
#define ANTwFOODwSLEEP '$'

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "do_not_submit.h"

typedef struct _Ant {
  size_t id;
  size_t cur_x;
  size_t cur_y;
  pthread_t thread_id;
} Ant;

size_t number_of_ants;
size_t number_of_foods;
size_t termination_time;
int is_running;

pthread_mutex_t row_lock[GRIDSIZE];
pthread_mutex_t delay_mutex;
pthread_mutex_t sleeping_mutex;

pthread_cond_t sleeping_condition;

Ant* ants;

Ant GenerateAnt(size_t id, size_t cur_x, size_t cur_y) {
  Ant ant;
  ant.id = id;
  ant.cur_x = cur_x;
  ant.cur_y = cur_y;
  return ant;
}

void PutOnEmptyCells(char what, size_t count, Ant* ants) {
  for (size_t i = 0; i < count; i++) {
    size_t x = rand() % GRIDSIZE;
    size_t y = rand() % GRIDSIZE;
    while (lookCharAt(x, y) != EMPTY_CELL) {
      if (x > 0) {
        x--;
      } else if (y > 0) {
        y--;
      } else {
        x = rand() % GRIDSIZE;
        y = rand() % GRIDSIZE;
      }
    }
    putCharTo(x, y, what);
    if (ants != NULL) {
      ants[i] = GenerateAnt(i, x, y);
    }
  }
}

int IsValid(int x, int y) {
  return x >= 0 && y >= 0 && x < GRIDSIZE && y < GRIDSIZE;
}

int dx[] = {-1, 0, 1, -1, 1, -1, 0, 1};
int dy[] = {-1, -1, -1, 0, 0, 1, 1, 1};
void* AntRoutine(void* args) {
  const size_t ant_id = (size_t)args;
  Ant ant = ants[ant_id];
  int is_tired = 0;
  while (is_running) {
    const char status = lookCharAt(ant.cur_x, ant.cur_y);
    pthread_mutex_lock(&sleeping_mutex);
    size_t n_sleepers = getSleeperN();
    pthread_mutex_unlock(&sleeping_mutex);
    if (ant_id >= n_sleepers) {
      int found_food = 0;
      int has_moved = 0;
      for (size_t i = 0; i < 8 && !has_moved && !is_tired; i++) {
        const int newx = ant.cur_x + dx[i];
        const int newy = ant.cur_y + dy[i];
        if (!IsValid(newx, newy)) continue;
        pthread_mutex_lock(row_lock + newx);
        const size_t cell = lookCharAt(newx, newy);
        if (cell == FOOD_CELL) {
          if (status == ANT) {
            putCharTo(ant.cur_x, ant.cur_y, EMPTY_CELL);
            ant.cur_x = newx;
            ant.cur_y = newy;
            putCharTo(ant.cur_x, ant.cur_y, ANTwFOOD);
            has_moved = 1;
          } else if (status == ANTwFOOD) {
            found_food = 1;
          }
        }
        pthread_mutex_unlock(row_lock + newx);
      }
      for (size_t i = 0, r = rand() % 8; i < 8 && !has_moved; i++) {
        const int newx = ant.cur_x + dx[i + r];
        const int newy = ant.cur_y + dy[i + r];
        if (!IsValid(newx, newy)) continue;
        pthread_mutex_lock(row_lock + newx);
        const size_t cell = lookCharAt(newx, newy);
        if (cell == EMPTY_CELL) {
          if (is_tired) {
            putCharTo(ant.cur_x, ant.cur_y, EMPTY_CELL);
            ant.cur_x = newx;
            ant.cur_y = newy;
            putCharTo(ant.cur_x, ant.cur_y, ANT);
            has_moved = 1;
            is_tired = 0;
          } else if (status == ANT) {
            putCharTo(ant.cur_x, ant.cur_y, EMPTY_CELL);
            ant.cur_x = newx;
            ant.cur_y = newy;
            putCharTo(ant.cur_x, ant.cur_y, ANT);
            has_moved = 1;
          } else if (status == ANTwFOOD) {
            char new_status = EMPTY_CELL;
            if (found_food) {
              new_status = FOOD_CELL;
            }
            putCharTo(ant.cur_x, ant.cur_y, new_status);
            ant.cur_x = newx;
            ant.cur_y = newy;
            new_status = ANTwFOOD;
            if (found_food) {
              new_status = ANT;
              is_tired = 1;
            }
            putCharTo(ant.cur_x, ant.cur_y, new_status);
            found_food = 0;
            has_moved = 1;
          }
        }
        pthread_mutex_unlock(row_lock + newx);
      }
    } else if (status != ANTwSLEEP && status != ANTwFOODwSLEEP) {
      char new_status = ANTwSLEEP;
      if (status == ANTwFOOD) new_status = ANTwFOODwSLEEP;
      putCharTo(ant.cur_x, ant.cur_y, new_status);
      pthread_mutex_lock(&sleeping_mutex);
      while (ant_id < n_sleepers) {
        pthread_cond_wait(&sleeping_condition, &sleeping_mutex);
        n_sleepers = getSleeperN();
      }
      pthread_mutex_unlock(&sleeping_mutex);
      putCharTo(ant.cur_x, ant.cur_y, status);
    }
    pthread_mutex_lock(&delay_mutex);
    const size_t wait_time = getDelay() + (rand() % 10);
    pthread_mutex_unlock(&delay_mutex);
    usleep(wait_time * 1000);
  }

  return NULL;
}

void Initialize() {
  if (pthread_mutex_init(&delay_mutex, NULL)) {
    perror("Mutex init failed.");
  }
  if (pthread_mutex_init(&sleeping_mutex, NULL)) {
    perror("Mutex init failed.");
  }
  if (pthread_cond_init(&sleeping_condition, NULL)) {
    perror("Condition variable init failed.");
  }
  for (size_t i = 0; i < GRIDSIZE; i++) {
    if (pthread_mutex_init(row_lock + i, NULL)) {
      perror("Mutex init failed.");
    }
    for (size_t j = 0; j < GRIDSIZE; j++) {
      putCharTo(i, j, EMPTY_CELL);
    }
  }

  ants = (Ant*)malloc(sizeof(Ant) * number_of_ants);
  PutOnEmptyCells(ANT, number_of_ants, ants);
  PutOnEmptyCells(FOOD_CELL, number_of_foods, NULL);

  is_running = 1;

  for (size_t i = 0; i < number_of_ants; i++) {
    pthread_create(&ants[i].thread_id, NULL, AntRoutine, (void*)i);
  }
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    perror("Wrong usage!");
  }
  srand(time(NULL));

  number_of_ants = strtol(argv[1], NULL, 10);
  number_of_foods = strtol(argv[2], NULL, 10);
  termination_time = strtol(argv[3], NULL, 10);

  Initialize();

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
      pthread_mutex_lock(&delay_mutex);
      setDelay(getDelay() + 10);
      pthread_mutex_unlock(&delay_mutex);
    }
    if (c == '-') {
      pthread_mutex_lock(&delay_mutex);
      setDelay(getDelay() - 10);
      pthread_mutex_unlock(&delay_mutex);
    }
    if (c == '*') {
      pthread_mutex_lock(&sleeping_mutex);
      setSleeperN(getSleeperN() + 1);
      pthread_mutex_unlock(&sleeping_mutex);
    }
    if (c == '/') {
      pthread_mutex_lock(&sleeping_mutex);
      setSleeperN(getSleeperN() - 1);
      pthread_mutex_unlock(&sleeping_mutex);
      pthread_cond_broadcast(&sleeping_condition);
    }
    usleep(50000);

    // each ant thread have to sleep with code similar to this
    // usleep(getDelay() * 1000 + (rand() % 5000));
  }

  // do not forget freeing the resources you get
  endCurses();

  free(ants);

  return 0;
}
