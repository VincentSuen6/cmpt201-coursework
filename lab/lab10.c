/*
 * lab10.c — Lab 10: Socket Programming with Multiple Clients
 * Contains: client.c followed by server.c
 *
 * ══════════════════════════════════════════════════
 * Questions answered at the top of server.c section:
 * ══════════════════════════════════════════════════
 *
 * ── Understanding the Client ──
 *
 * 1. How is the client sending data to the server? What protocol?
 *    The client uses TCP (SOCK_STREAM) over IPv4. It calls connect() to
 *    establish a connection to 127.0.0.1:8001, then sends data with write().
 *
 * 2. What data is the client sending to the server?
 *    It sends 5 fixed strings: "Hello", "Apple", "Car", "Green", "Dog".
 *    Each is copied into a BUF_SIZE (1024-byte) buffer with strncpy (zero-
 *    padded), then written as a full 1024-byte chunk. One message per second.
 *
 * ── Understanding the Server ──
 *
 * 1. Explain the argument that the run_acceptor thread is passed as an argument.
 *    It receives a pointer to a struct acceptor_args, which holds:
 *      - run (atomic_bool): loop-control flag; set to false to stop accepting.
 *      - list_handle: pointer to the shared linked-list that stores messages.
 *      - list_lock: pointer to the mutex that guards the list.
 *
 * 2. How are received messages stored?
 *    In a singly-linked list. Each message is heap-allocated into a
 *    list_node whose `data` field holds a malloc'd BUF_SIZE copy of the
 *    message. Nodes are appended at the tail tracked by list_handle->last.
 *
 * 3. What does main() do with the received messages?
 *    After all expected messages arrive, main() calls collect_all(), which
 *    walks the list, prints each message, frees each node and its data, and
 *    returns the total count. It then verifies the count matches the expected
 *    total (MAX_CLIENTS * NUM_MSG_PER_CLIENT).
 *
 * 4. How are threads used in this sample code?
 *    One acceptor thread (run_acceptor) listens for new TCP connections.
 *    For every client that connects, the acceptor spawns a dedicated client
 *    thread (run_client) that reads messages from that socket in a loop.
 *    The main thread busy-waits (with mutex) until enough messages have been
 *    collected, then signals the acceptor to stop.
 *
 * ── Non-blocking sockets ──
 *
 * How are sockets made non-blocking?
 *    set_non_blocking(fd) retrieves current file-descriptor flags with
 *    fcntl(fd, F_GETFL, 0), then sets the O_NONBLOCK bit with
 *    fcntl(fd, F_SETFL, flags | O_NONBLOCK).
 *
 * What sockets are made non-blocking?
 *    (a) The listening server socket (sfd) inside run_acceptor().
 *    (b) Each accepted client socket (cfd) inside run_client().
 *
 * Why? What purpose does it serve?
 *    Without O_NONBLOCK, accept() and read() would block the calling thread
 *    indefinitely waiting for a connection/data. Both run_acceptor and
 *    run_client must periodically re-check their `run` flag to know when to
 *    stop. Non-blocking mode causes these calls to return immediately with
 *    errno EAGAIN/EWOULDBLOCK when there is nothing to do, so the while-loop
 *    can re-check `run` on every iteration without being stuck inside a
 *    blocking system call — enabling a clean, cooperative shutdown.
 */

/* ================================================================
 *  client.c
 * ================================================================ */

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

/*
 * Append new_node to the tail of the list.
 * CALLER MUST HOLD list_lock.
 */
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

      /*
       * COMPLETED TODO:
       * Lock the mutex then safely append the new node to the shared list.
       */
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
    /*
     * COMPLETED TODO:
     * Clear the run flag so run_client's while-loop exits.
     */
    client_args[i].run = false;

    /*
     * COMPLETED TODO:
     * Wait for the thread to finish.
     * (The socket fd is closed inside run_client after the loop.)
     */
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

  /*
   * COMPLETED TODO:
   * Busy-wait until we have received every expected message.
   * The mutex is used to safely read list_handle.count, which client
   * threads modify inside add_to_list() (also under the same mutex).
   */
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
