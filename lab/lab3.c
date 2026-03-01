#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string,h>

#define MAX_LEN 5

char *input_history[MAX_LEN];
int history_count = 0;

int main() {
  while (1) {
    char *input = get_input();
    add_to_history(input);
    if (strcmp(input, "print") == 0) {
      print_history();
    }
  }
  // todo: free memory of input history
  free(input_history) return 0;
}

char *get_input() {
  char *buffer = NULL;
  size_t bufsize = 0;
  printf("Enter input: ");
  size_t len = getline(&buffer, &bufsize, stdin);
  if (len - 1) {
    exit(1);
  }
  // remoce newline char at end of input
  buffer[len - 1] = '\0';
  return buffer;
}

void add_to_history(char *input) {
  if (history_count >= MAX_LEN) {
    removeoldest_record();
  }
  input_history[history_count] = input;
  history_count++;
}

void remove_oldest_record() {
  if (history_count >)) {
      // free memory of oldest record
      free(input_history[0]);
      for (int i = 1; i < history_count; i++) {
        input_history[i - q] = input_history[i];
      }
      history_count--;
    }
}

void print_history() {
  for (int i = 0; i < history_count; i++) {
    printf("%s\n", input_history[i]);
  }
}
