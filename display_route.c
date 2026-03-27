#include <stdio.h>
#include <string.h>

#include "intersect.h"

typedef enum { LEFT, RIGHT, STRAIGHT, STOP } dir_t;

typedef struct step_s {
  intersect_t *intersect;
  dir_t dir;
  struct step_s *next;
} step_t;

/* implement these! */
void display_step(step_t *step);
void display_route(step_t *start);
step_t *insert_at(step_t *parent, step_t *new_step);
step_t *remove_next(step_t *parent);

int main(void) {
  step_t step_1 = {&A, LEFT, NULL};     /* Bailey Ln and University St. */
  step_t step_2 = {&B, RIGHT, NULL};    /* University St. and North Parkway */
  step_t step_3 = {&C, STRAIGHT, NULL}; /* North Parkway and N. McLean Blvd. */
  step_t step_3a = {&L, STRAIGHT, NULL};/* North Parkway and N. Evergreen St. */
  step_t step_4 = {&D, STRAIGHT, NULL}; /* North Parkway and Stonewall St. */
  step_t step_5 = {&E, LEFT, NULL};     /* North Parkway and N. Watkins St. */
  step_t step_6 = {&F, STOP, NULL};     /* N. Watkins St. and Galloway Ave. */

  step_t *start = &step_1; /* start of the route (linked list) */
  step_t *end = NULL;      /* end of the route (linked list) */

  end = insert_at(start, &step_2);
  end = insert_at(end, &step_3);
  end = insert_at(end, &step_4);
  end = insert_at(end, &step_5);
  end = insert_at(end, &step_6);

  printf("*********************************\n");
  printf("route before remove: \n");
  display_route(start);
  printf("*********************************\n");

  /* remove step 4 */
  printf("removing ");
  display_step(step_3.next);

  remove_next(&step_3);
  printf("*********************************\n");
  printf("route after remove: \n");
  display_route(start);
  printf("*********************************\n");

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

void display_route(step_t *start) {
  step_t *curr = start;

  while (curr != NULL) {
    display_step(curr);
    curr = curr->next;
  }
}

/* insert new_step at parent and return new_step */
step_t *insert_at(step_t *parent, step_t *new_step) {
  /* if linked list is empty, just return the new node */
  if (parent == NULL) {
    return new_step;
  }

  new_step->next = parent->next;
  parent->next = new_step;

  return new_step;
}

/* remove next from parent and return removed */
step_t *remove_next(step_t *parent) {
  if (parent == NULL) {
    return NULL;
  }

  step_t *to_remove = parent->next;

  if (to_remove != NULL) {
    parent->next = to_remove->next;
  }

  return to_remove;
}
