#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

void display_list(list_t *list);
node_t *find(list_t *list, char *value);

int main(void) {
  list_t *list = create_list();

  int i_array[5] = {1, 2, 3, 4, 5};

  list->end = append(list, create_node("first", &i_array[0]));
  list->end = append(list, create_node("second", &i_array[1]));
  list->end = append(list, create_node("fourth", &i_array[3]));
  list->end = append(list, create_node("fifth", &i_array[4]));

  node_t *parent = find(list, "second");

  if (parent != NULL) {
    insert_at(list, parent, create_node("third", &i_array[2]));
  }

  display_list(list);

  /* TODO: remove all nodes and free memory */

  return 0;
}

node_t *find(list_t *list, char *value) {
  /* TODO: insert nodes in the middle */
  node_t *parent = NULL;
  node_t *curr = NULL;

  while (curr != NULL) {
    if (strcmp(curr->data, "second") == 0){
      parent = curr;
      break;
    }
  }

  return parent;
}

void display_list(list_t *list) {
  /* displays the linked list from start to end */
  node_t *curr = list->start;

  while (curr != NULL) {
    int *value = curr->data;
    printf("%s = %d\n", curr->id, *value);

    curr = curr->next;
  }

  printf("# elements: %d\n", list->size);
}
