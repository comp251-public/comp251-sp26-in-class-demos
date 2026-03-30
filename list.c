#include "list.h"

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
