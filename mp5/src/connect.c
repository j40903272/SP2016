#include "connect.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

//server starts to listen
int init_server(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    fprintf(stderr, "socket open error\n");
    return -1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(fd);
    fprintf(stderr, "port bind error\n");
    return -1;
  }
  if (listen(fd, 1024) < 0) {
    close(fd);
    fprintf(stderr, "port listen error\n");
    return -1;
  }
  return fd;
}

int connect_to(const char* servername, int port) {
  struct hostent* server = NULL;
  server = gethostbyname(servername);
  if (server == NULL) {
    fprintf(stderr, "bad server address error\n");
    return -1;
  }
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    fprintf(stderr, "socket open error\n");
    return -1;
  }
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  memcpy(&addr.sin_addr.s_addr,
         server->h_addr_list[0],
         server->h_length);
  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    fprintf(stderr, "connect server error\n");
    close(fd);
    return -1;
  }
  return fd;
}

//connect to "port_register" to get a port
int server_start() {
  int fd = connect_to("localhost", default_register_port);
  if (fd < 0) {
    fprintf(stderr, "socket open error\n");
    return -1;
  }
  int reg;
  char* name = getenv("USER");
  char buf[register_protocol_buf_size];
  sprintf(buf, "0%s", name);
  if (write(fd, buf, strlen(buf)) < 0) {
    fprintf(stderr, "register error\n");
    close(fd);
    return -1;
  }
  if (read(fd, &reg, sizeof(int)) < 0) {
    fprintf(stderr, "register error\n");
    close(fd);
    return -1;
  }
  close(fd);
  if (reg == -1) {
    fprintf(stderr, "register fail\n");
    return -1;
  }
  fprintf(stderr, "assigned port: %d\n", reg);
  return init_server(reg);
}

//connect to "port_register" to get server port, then connect to server
int client_start(const char* username, const char* server) {
  int fd = connect_to(server, default_register_port);
  if (fd < 0) {
    fprintf(stderr, "socket open error\n");
    return -1;
  }
  char buf[register_protocol_buf_size];
  int reg;
  sprintf(buf, "1%s", username);
  if (write(fd, buf, strlen(buf)) < 0) {
    fprintf(stderr, "register error\n");
    close(fd);
    return -1;
  }
  if (read(fd, &reg, sizeof(int)) < 0) {
    fprintf(stderr, "register error\n");
    close(fd);
    return -1;
  }
  close(fd);
  if (reg == -1) {
    fprintf(stderr, "server no start\n");
    return -1;
  }
  return connect_to(server, reg);
}
