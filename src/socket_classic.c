#include <arpa/inet.h>
#include <liburing.h>
#include <stdio.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

struct client_handler_params {
  int client_fd;
  struct sockaddr client_sockaddr;
  socklen_t client_socklen;
};

int client_handler(void *args) {
  const struct client_handler_params *const params = args;

  const char greetings[] = "foobar\n";

  const int client_fd = params->client_fd;
  write(client_fd, greetings, sizeof(greetings));
  close(client_fd);

  return 0;
}

int main(void) {
  const int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_socket == -1) {
    perror("socket");
    return 1;
  }

  const int REUSE_ADDR = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &REUSE_ADDR,
                 sizeof(REUSE_ADDR)) == -1) {
    perror("setsockopt: SO_REUSEADDR");
    return 1;
  }

  const struct sockaddr_in server_bind_addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
      .sin_port = htons(1080)};

  if (bind(server_socket, (struct sockaddr *)&server_bind_addr,
           sizeof(server_bind_addr)) == -1) {
    perror("bind");
    return 1;
  }

  if (listen(server_socket, 1024) == -1) {
    perror("listen");
    return 1;
  }

  struct sockaddr client_sockaddr;
  socklen_t client_socklen;
  while (true) {
    const int client_fd =
        accept(server_socket, &client_sockaddr, &client_socklen);
    if (client_fd == -1) {
      perror("accept");
      return 1;
    }

    thrd_t client_thread;
    struct client_handler_params params = {.client_fd = client_fd,
                                           .client_sockaddr = client_sockaddr,
                                           .client_socklen = client_socklen};

    const int ret = thrd_create(&client_thread, client_handler, &params);

    if (ret != thrd_success) {
      if (ret == thrd_nomem) {
        fprintf(stderr, "out of memory\n");
      }
      if (ret == thrd_error) {
        fprintf(stderr, "error: thrd_create\n");
      }

      return 1;
    }
  }

  return 0;
}
