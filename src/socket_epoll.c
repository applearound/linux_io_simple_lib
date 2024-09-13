#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 128

int set_nonblocking(const int sock_fd) {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl get flags");
    return -1;
  }

  flags |= O_NONBLOCK;

  const int result = fcntl(sock_fd, F_SETFL, flags);
  if (result == -1) {
    perror("fcntl set flags");
    return -1;
  }

  return 0;
}

int init_socket() {
  const int server_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (server_socket == -1) {
    perror("socket");
    return -1;
  }

  const int REUSE_ADDR = 1;
  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &REUSE_ADDR,
                 sizeof(REUSE_ADDR)) == -1) {
    perror("setsockopt: SO_REUSEADDR");
    return -1;
  }

  const struct sockaddr_in server_bind_addr = {
      .sin_family = AF_INET,
      .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
      .sin_port = htons(1080)};

  if (bind(server_socket, (struct sockaddr *)&server_bind_addr,
           sizeof(server_bind_addr)) == -1) {
    perror("bind");
    return -1;
  }

  if (listen(server_socket, 1024) == -1) {
    perror("listen");
    return -1;
  }

  return server_socket;
}

int main(void) {
  const int epfd = epoll_create(1);
  if (epfd == -1) {
    perror("epoll_create");
    return 1;
  }

  int socketfd = init_socket();
  if (socketfd == -1) {
    return 1;
  }

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = socketfd;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
    perror("epoll_ctl");
    return 1;
  }

  struct epoll_event events[MAX_EVENTS];
  while (true) {
    int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      return 1;
    }

    struct sockaddr client_sockaddr;
    socklen_t client_socklen;

    for (int n = 0; n < nfds; n++) {
      if (events[n].data.fd == socketfd) {

        const int client_sock =
            accept(socketfd, &client_sockaddr, &client_socklen);
        if (client_sock == -1) {
          perror("accept");
          return 1;
        }

        // 使用 EPOLLET 要求 fd 必须为 O_NONBLOCK
        set_nonblocking(client_sock);

        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_sock;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_sock, &ev) == -1) {
          perror("epoll_ctl");
          return 1;
        }
      } else {
        const char greetings[] = "Hello world!\n";
        const int client_sock_fd = events[n].data.fd;
        write(client_sock_fd, greetings, sizeof(greetings));
        close(client_sock_fd);
      }
    }
  }

  close(epfd);
  return 0;
}
