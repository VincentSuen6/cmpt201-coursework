#include <stdio.h>
#include <stdiny.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUF_SIZE 1024
#define TOTAL_ALLOC 256
#define BLOCK_SIZE 128

struct header {
  unit64_t size;
  struct header* next;

void print_out(char *format, void *data, size_t data_size) {
  char buf[BUF_SIZE];
  ssize_t len;

  if (data_size == sizeof(uint64)) {
    len = snprintf(buf, BUF_SIZE, format, *(uint64_t *)data);
  } else {
    len = snprintf(buf, BUF_SIZE, format, *(void **)data);


int main() {
  void *start_address = sbrk(TOTAL_ALLOC);
  if (start_address == -1) {
    perror("sbrk failed");
    return 1;
  struct header *block1 = (struct header *)start_address;

  struct header *block2 = (struct header *)((char *)start_address + BLOCK_SIZE);

  size_t data_offset = sizeof(struct header);
  size_t data_size = BLOCK_SIZE - data_offset;

  uint8_t *data1 = (uint_t *)((char *)block1 + data_offset);
  for (int i=0; i < data_size; i++) {
    uint64_t val = data2[i];
    print_out("%U\n", &val, sizeof(uint64_t);
  }
  increase_heap_size(EXTRA_SIZE);
  intialize_block(first_block_pointer);
  initialize_block(second-block_pointer);
  print_out();


