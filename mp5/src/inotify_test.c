#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/inotify.h> //header for inotify
#include <string.h>
#include <unistd.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

int main() {
  int length, i = 0;
  int fd;
  int wd;
  char buffer[EVENT_BUF_LEN];
  memset(buffer, 0, EVENT_BUF_LEN);

  //create a instance and returns a file descriptor
  fd = inotify_init();

  if (fd < 0) {
    perror("inotify_init");
  }

  //add directory "." to watch list with specified events
  wd = inotify_add_watch(fd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);

  while ((length = read(fd, buffer, EVENT_BUF_LEN)) > 0) {
    i = 0;
    while (i < length) {
      struct inotify_event* event = (struct inotify_event*)&buffer[i];
      printf("event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
      if (event->mask & IN_CREATE) {
        printf("create ");
      }
      if (event->mask & IN_DELETE) {
        printf("delete ");
      }
      if (event->mask & IN_ATTRIB) {
        printf("attrib ");
      }
      if (event->mask & IN_MODIFY) {
        printf("modify ");
      }
      if (event->mask & IN_ISDIR) {
        printf("dir\n");
      } else {
        printf("file\n");
      }
      i += EVENT_SIZE + event->len;
    }
    memset(buffer, 0, EVENT_BUF_LEN);
  }

  //inotify_rm_watch(fd, wd);
  close(fd);
  return 0;
}
