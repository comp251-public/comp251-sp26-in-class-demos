#ifndef LIST_H
#define LIST_H

typedef struct node_s {
  char *id;
  void *data;
  struct node_s *next;
} node_t;

typedef struct {
  node_t *start;
  node_t *end;
  int size;
} list_t;

list_t *create_list();

node_t *create_node(char *id, void *data);
node_t *append(list_t *list, node_t *new_node);
node_t *insert_at(list_t *list, node_t *parent, node_t *new_node);
node_t *remove_next(node_t *parent);

#endif
