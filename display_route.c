#include <stdio.h>
#include <string.h>

#include "intersect.h"
#include "list.h"

typedef enum { LEFT, RIGHT, STRAIGHT, STOP } dir_t;

typedef struct step_s {
  intersect_t *intersect;
  dir_t dir;
} step_t;

void display_route(list_t *route);
void display_step(step_t *step);

int main(void) {
  list_t route = {NULL, NULL};

  step_t step_1 = {&A, LEFT};      /* Bailey Ln and University St. */
  step_t step_2 = {&B, RIGHT};     /* University St. and North Parkway */
  step_t step_3 = {&C, STRAIGHT};  /* North Parkway and N. McLean Blvd. */
  step_t step_3a = {&L, STRAIGHT}; /* North Parkway and N. Evergreen St. */
  step_t step_4 = {&D, STRAIGHT};  /* North Parkway and Stonewall St. */
  step_t step_5 = {&E, LEFT};      /* North Parkway and N. Watkins St. */
  step_t step_6 = {&F, STOP};      /* N. Watkins St. and Galloway Ave. */

  return 0;
}

/* display directions for a single step in a route */
void display_step(step_t *step) {
  switch (step->dir) {
  case LEFT:
    printf("Turn left");
    break;
  case RIGHT:
    printf("Turn right");
    break;
  case STRAIGHT:
    printf("Keep going");
    break;
  case STOP:
    printf("You have arrived");
    break;
  default:
    fprintf(stderr, "You done used a bad enum value\n");
    break;
  }

  printf(" at %s\n", step->intersect->desc);
}

void display_route(list_t *route) {
  step_t *curr = start;

  while (curr != NULL) {
    display_step(curr);
    curr = curr->next;
  }
}
