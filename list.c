#include "list.h"

#include <stdlib.h>
#include <stdio.h>

list_t *create_list() {
  list_t *new_list = NULL;
  new_list = malloc(sizeof(list_t));

  if (new_list == NULL) {
    perror("malloc failed");
    exit(1);
  }

  new_list->start = NULL;
  new_list->end = NULL;

  return new_list;
}

node_t *create_node(char *id, void *data) {
  node_t *new = NULL;
  new = malloc(sizeof(node_t));

  if (new == NULL) {
    perror("malloc failed");
    exit(1);
  }

  new->id = id;
  new->data = data;
  new->next = NULL;

  return new;
}

node_t *append(list_t *list, node_t *new_node) {
  if (list->start == NULL) {
    list->start = new_node;
    list->end = new_node;

    list->size = 1;
  } else {
    list->end = insert_at(list, list->end, new_node);
  } 

  return new_node;
}

/* insert new_node at parent and return new_node */
node_t *insert_at(list_t *list, node_t *parent, node_t *new_node) {
  /* if linked list is empty, just return the new node */
  if (parent == NULL) {
    return new_node;
  }

  new_node->next = parent->next;
  parent->next = new_node;

  list->size++;

  return new_node;
}

/* remove next from parent and return removed */
node_t *remove_next(node_t *parent) {
  if (parent == NULL) {
    return NULL;
  }

  node_t *to_remove = parent->next;

  if (to_remove != NULL) {
    parent->next = to_remove->next;
  }

  return to_remove;
}
