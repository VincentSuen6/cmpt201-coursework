

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int client_main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];
  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);
    /* pads destination with NUL bytes */
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  exit(EXIT_SUCCESS);
}

/* ================================================================
 *  server.c
 * ================================================================ */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/socket.h>

#define SERVER_BUF_SIZE        1024
#define SERVER_PORT            8001
#define LISTEN_BACKLOG         32
#define MAX_CLIENTS            4
#define NUM_MSG_PER_CLIENT     5

#define srv_handle_error(msg)                                                  \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

/* ── data structures ── */

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last;
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;
  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t    *list_lock;
};

struct acceptor_args {
  atomic_bool run;
  struct list_handle *list_handle;
  pthread_mutex_t    *list_lock;
};

/* ── helpers ── */

static int init_server_socket(void) {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    srv_handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family      = AF_INET;
  addr.sin_port        = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    srv_handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    srv_handle_error("listen");
  }

  return sfd;
}

static void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
}

static void add_to_list(struct list_handle *list_handle,
                        struct list_node   *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next   = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

static int collect_all(struct list_node head) {
  struct list_node *node  = head.next;
  uint32_t          total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return total;
}

/* ── per-client thread ── */

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[SERVER_BUF_SIZE];

  while (cargs->run) {
    ssize_t bytes_read = read(cfd, &msg_buf, SERVER_BUF_SIZE);

    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket!\n");
        break;
      }
      /* No data yet — loop back and re-check run flag */
    } else if (bytes_read > 0) {
      /* Allocate a new list node with a heap copy of the received message */
      struct list_node *new_node = malloc(sizeof(struct list_node));
      new_node->next = NULL;
      new_node->data = malloc(SERVER_BUF_SIZE);
      memcpy(new_node->data, msg_buf, SERVER_BUF_SIZE);

      pthread_mutex_lock(cargs->list_lock);
      add_to_list(cargs->list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    }
  }

  /* Close this client's socket before the thread exits */
  if (close(cfd) == -1) {
    perror("client thread close");
  }
  return NULL;
}

/* ── acceptor thread ── */

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t          threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (aargs->run) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);

      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          srv_handle_error("accept");
        }
        /* No pending connection — keep looping */
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd         = cfd;
        client_args[num_clients].run         = true;
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock   = aargs->list_lock;

        /*
         * COMPLETED TODO:
         * Spawn a dedicated thread for this client.
         */
        pthread_create(&threads[num_clients], NULL, run_client,
                       &client_args[num_clients]);

        num_clients++;
      }
    }
  }

  printf("Not accepting any more clients!\n");

  /* Signal every client thread to stop, then join. */
  for (int i = 0; i < num_clients; i++) {

    client_args[i].run = false;

    pthread_join(threads[i], NULL);
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }
  return NULL;
}

/* ── main ── */

int main(void) {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  /* Sentinel head node — stack-allocated, never freed */
  struct list_node   head        = {NULL, NULL};
  struct list_handle list_handle = {
      .last  = &head,
      .count = 0,
  };

  pthread_t            acceptor_thread;
  struct acceptor_args aargs = {
      .run         = true,
      .list_handle = &list_handle,
      .list_lock   = &list_mutex,
  };
  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);


  const uint32_t expected = MAX_CLIENTS * NUM_MSG_PER_CLIENT;
  while (1) {
    pthread_mutex_lock(&list_mutex);
    uint32_t current = list_handle.count;
    pthread_mutex_unlock(&list_mutex);

    if (current >= expected) {
      break;
    }
    sleep(1000); /* sleep 1 ms between polls to yield CPU */
  }

  /* Signal acceptor to stop, then wait for it (and all client threads). */
  aargs.run = false;
  pthread_join(acceptor_thread, NULL);

  if (list_handle.count != expected) {
    printf("Not enough messages were received!\n");
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);
  if (collected != (int)list_handle.count) {
    printf("Not all messages were collected!\n");
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);
  return 0;
}
