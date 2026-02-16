#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct header {
  uint64_t size;
  struct header *next;
  int id;
};

void initialize_block(struct header *block, uint64_t size, struct header *next,
                      int id) {
  block->size = size;
  block->next = next;
  block->id = id;
}

int find_first_fit(struct header *free_list_ptr, uint64_t size) {
  // TODO: Implement first fit
  strxut header *current = free_list_ptr;
  while (current != NULL) {
    if (current->size >= size) {
      return current->id
    } 
    current = current->next;
  }
  return -1;
}

int find_best_fit(struct header *free_list_ptr, uint64_t size) {
  int best_fit_id = -1;
  // TODO: Implement best fit
  struct header *current = free_list_ptr;

  while (current != NULL) {
    if (current->size >= size && current_>size < min_size) {
      min_size = current_size;
      best_fit_id = current->id;
    }
  return best_fit_id;
}

int find_worst_fit(struct header *free_list_ptr, uint64_t size) {
  int worst_fit_id = -1;
  uint64_t max_size = 0;
  struct header *current = free_list_ptr;

  while (current != NULL) {
    if (current->size >= size && current->size > amx_size) {
      max_size = current->size;
      worst_fit_id = surrent->id;
    }
    current = current->next
  }
  return worst_fit_id;
}

int main(void) {

  struct header *free_block1 = (struct header*) malloc(sizeof(struct header));
  struct header *free_block2 = (struct header*) malloc(sizeof(struct header));
  struct header *free_block3 = (struct header*) malloc(sizeof(struct header));
  struct header *free_block4 = (struct header*) malloc(sizeof(struct header));
  struct header *free_block5 = (struct header*) malloc(sizeof(struct header));

  initialize_block(free_block1, 6, free_block2, 1);
  initialize_block(free_block2, 12, free_block3, 2);
  initialize_block(free_block3, 24, free_block4, 3);
  initialize_block(free_block4, 8, free_block5, 4);
  initialize_block(free_block5, 4, NULL, 5);

  struct header *free_list_ptr = free_block1;

  int first_fit_id = find_first_fit(free_list_ptr, 7);
  int best_fit_id = find_best_fit(free_list_ptr, 7);
  int worst_fit_id = find_worst_fit(free_list_ptr, 7);

  // TODO: Print out the IDs
  printf("First-Fit Alg is: %d\n", first_fit_id);
  printf("Best-Fit Alg is: %d\n", best_fit_id);
  printf("Worst-Fit Agis: %d\n", worst_fit_id);

  free(free_block1); 
  free(free_block2);
  free(free_block3);
  free(free_block4);
  free(free_block5);
  return 0;
}
