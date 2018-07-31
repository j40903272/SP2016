#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "connect.h"

#define port_reserve_size 512

typedef struct {
  char name[register_protocol_buf_size];
  int port;
} record;

static int book_port(char* name, int size);
static int get_port(char* name, int size);
static int get_hash_code(char* name, int size);
static int get_next_port();
static void handle_request(int conn);
static void check_port_reserve();
static void clean_port_reserve();
static void print_timestamp();

static hash record_hash;
static int port_reserve[port_reserve_size];

int main(int argc, char** argv) {
  memset(&record_hash, 0, sizeof(hash));
  memset(port_reserve, 0, sizeof(port_reserve));
  if (!init_hash(&record_hash, 100)) {
    fprintf(stderr, "hash init err\n");
    exit(1);
  }
  int fd = init_server(default_register_port);
  if (fd < 0) {
    fprintf(stderr, "server init failed\n");
    exit(-1);
  }
  check_port_reserve();
  int conn, conn_len, max_fd;
  struct sockaddr_in addr;
  fd_set readset;
  struct timeval timeout;

  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  max_fd = getdtablesize();

  while(1) {
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    FD_ZERO(&readset);
    FD_SET(fd, &readset);
    if (select(max_fd, &readset, NULL, NULL, &timeout) < 0) {
      print_timestamp();
      fprintf(stderr, "select err\n");
      break;
    }
    if (FD_ISSET(fd, &readset)) {
      memset(&addr, 0, sizeof(addr));
      conn_len = 0;
      conn = accept(fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
      if (conn < 0) {
        if (errno == ENFILE) {
          print_timestamp();
          fprintf(stderr, "out of file descriptor table\n");
          continue;
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        } else {
          print_timestamp();
          fprintf(stderr, "accept err\n");
          print_timestamp();
          fprintf(stderr, "code: %s\n", strerror(errno));
          break;
        }
      }
      handle_request(conn);
      close(conn);
    } else {
      clean_port_reserve();
    }
    sleep(1);
  }
  destroy_hash(&record_hash);
}

static int book_port(char* name, int size) {
  int code = get_hash_code(name, size);
  record* r = NULL;
  if (get_from_hash(&record_hash, (void**)&r, code)) {
    print_timestamp();
    fprintf(stderr, "%s has booked\n", name);
    print_timestamp();
    fprintf(stderr, "give new port\n");
  }
  int port = get_next_port();
  if (port == -1) {
    print_timestamp();
    fprintf(stderr, "conn max\n");
    return -1;
  }
  if (r == NULL) {
    r = (record*)malloc(sizeof(record));
    if (r == NULL) {
      return -1;
    }
    strncpy(r->name, name, size);
    r->port = port;
    put_into_hash(&record_hash, r, code);
  } else {
    r->port = port;
  }
  return r->port;
}

static int get_port(char* name, int size) {
  int code = get_hash_code(name, size);
  record* r = NULL;
  if (!get_from_hash(&record_hash, (void**)&r, code)) {
    print_timestamp();
    fprintf(stderr, "%s haven't booked\n", name);
    return -1;
  }
  return r->port;
}

static int get_hash_code(char* name, int size) {
  int code = 0, i = 0;
  for (i = 0; i < size; ++i) {
    code = (code*256) + (int)name[i];
  }
  return code;
}

static int get_next_port() {
  int i = 0;
  for (i = 0; i < port_reserve_size; ++i) {
    if (port_reserve[i] == 0) {
      port_reserve[i] = 1;
      return i + default_register_port + 1;
    }
  }
  return -1;
}

static void handle_request(int conn) {
  char buf[register_protocol_buf_size];
  char act;
  int ret, info_ret;
  memset(buf, 0, sizeof(buf));
  ret = read(conn, buf, 1);
  if (ret < 1) {
    print_timestamp();
    fprintf(stderr, "bad protocol\n");
    return;
  }
  act = buf[0];
  ret = read(conn, buf, sizeof(buf));
  if (ret <= 0) {
    print_timestamp();
    fprintf(stderr, "bad protocol\n");
    return;
  }
  switch (act) {
    case '0':
      print_timestamp();
      fprintf(stderr, "book %s\n", buf);
      info_ret = book_port(buf, ret);
      break;
    case '1':
      print_timestamp();
      fprintf(stderr, "request %s\n", buf);
      info_ret = get_port(buf, ret);
      break;
    default:
      print_timestamp();
      fprintf(stderr, "bad protocol\n");
      return;
  }
  print_timestamp();
  fprintf(stderr, "return %d\n", info_ret);
  write(conn, &info_ret, sizeof(int));
}

static void check_port_reserve() {
  int i = 0;
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  for (i = 0; i < port_reserve_size; ++i) {
    int port = i + default_register_port + 1;
    addr.sin_port = htons(port);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int state = 0;
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
      state = 1;
    }
    if (state != port_reserve[i]) {
      print_timestamp();
      fprintf(stderr,
              "unuse port %d change: %d->%d\n", port, port_reserve[i], state);
      port_reserve[i] = state;
    }
    close(fd);
  }
}

static void clean_port_reserve() {
  int i = 0;
  struct sockaddr_in addr;
  record* r;
  hash_node* tmp_node;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  for (i = 0; i < record_hash.n; ++i) {
    hash_node* n = record_hash.node[i];
    while (n != NULL) {
      int port = ((record*)(n->contain))->port;
      addr.sin_port = htons(port);
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) >= 0) {
        print_timestamp();
        fprintf(stderr, "clean port: %d\n", port);
        port_reserve[port - 1 - default_register_port] = 0;
        tmp_node = n->next;
        del_from_hash(&record_hash, (void**)&r, n->hash_code);
        free(r);
        n = tmp_node;
      } else {
        n = n->next;
      }
      close(fd);
    }
  }
  check_port_reserve();
}

static void print_timestamp() {
  char buf[512];
  memset(buf, 0, 512);
  time_t t;
  struct tm* tmp;
  t = time(NULL);
  tmp = localtime(&t);
  if (tmp == NULL) {
    fprintf(stderr, "[time err] ");
    return;
  }
  if (strftime(buf, sizeof(buf), "%F %H:%M:%S", tmp) == 0) {
    fprintf(stderr, "[time err] ");
    return;
  }
  fprintf(stderr, "[%s] ", buf);
}
