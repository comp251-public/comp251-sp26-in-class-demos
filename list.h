#ifndef LIST_H
#define LIST_H

typedef struct node_s {
  char *id;
  void *data;
  struct node_s *next;
} node_t;

step_t *insert_at(step_t *parent, step_t *new_step);
step_t *remove_next(step_t *parent);


#endif
