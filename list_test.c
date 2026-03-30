#include <stdio.h>
#include <stdlib.h>

#include "list.h"

int main(void) {
  list_t *list = create_list();

  int i_array[5] = {1,2,3,4};

  list->end = append(list, create_node("first", &i_array[0]));
  list->end = append(list, create_node("second", &i_array[1]));
  list->end = append(list, create_node("third", &i_array[2]));
  list->end = append(list, create_node("fourth", &i_array[3]));

  int *value = NULL;
  node_t *curr = list->start;

  while (curr != NULL) {
    value = curr->data;
    printf("%s = %d\n", curr->id, *value);
    curr = curr->next;
  }

  return 0;
}
