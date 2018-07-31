#include "csiebox_client.h"

#include "csiebox_common.h"
#include "connect.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sys/inotify.h>
#include <utime.h>
#include <fcntl.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

static int parse_arg(csiebox_client* client, int argc, char** argv);
static int login(csiebox_client* client);

//read config file, and connect to server
void csiebox_client_init(csiebox_client** client, int argc, char** argv) {
  csiebox_client* tmp = (csiebox_client*)malloc(sizeof(csiebox_client));
  if (!tmp) {
    fprintf(stderr, "client malloc fail\n");
    return;
  }
  memset(tmp, 0, sizeof(csiebox_client));
  if (!parse_arg(tmp, argc, argv)) {
    fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
    free(tmp);
    return;
  }
  int fd = client_start(tmp->arg.name, tmp->arg.server);
  if (fd < 0) {
    fprintf(stderr, "connect fail\n");
    free(tmp);
    return;
  }
  tmp->conn_fd = fd;
  *client = tmp;
}
char event_path[1500][PATH_MAX];
H hl[1000];
int cnt;
void traverse_file(int fd, char dir_name[], csiebox_client* client);
int find(int conn_fd, char dir_name[], ino_t inode, char new_path[]){
    DIR *dir = opendir(dir_name);
    if(dir == NULL) return 0;
    struct dirent *entry;
    struct stat st;
    while((entry = readdir(dir)) != NULL){
        if(entry->d_name[0] == '.') continue;
        char cpath[PATH_MAX];
        if(strlen(dir_name) == 2)
            snprintf(cpath, sizeof(cpath), "%s%s", dir_name, entry->d_name);
        else
            snprintf(cpath, sizeof(cpath), "%s/%s", dir_name, entry->d_name);
        if(lstat(cpath, &st) == -1)
            fprintf(stderr,"get stat on %s Error\n", cpath);
        if(st.st_ino == inode && strcmp(cpath+2, new_path)){
            fprintf(stderr, "found %s\n", cpath+2);
            send_hardlink(conn_fd, cpath+2, new_path);
            return 1;
        }
        if(S_ISDIR(st.st_mode)) find(conn_fd, cpath, inode, new_path);
    }
    closedir(dir);
    return 0;
}
int is_hardlink(int conn_fd, ino_t inode, char path[]){
    fprintf(stderr, "into is_hardlink\n");
    int i = 0;
    for(; i < cnt ; i++)
        if(hl[i].inode == inode){
            send_hardlink(conn_fd, hl[i].path, path);
            return 1;
        }
    fprintf(stderr, "start find\n");
    if(find(conn_fd, "./", inode, path)){
        hl[cnt].inode = inode;
        strcpy(hl[cnt].path, path);
        ++cnt;
        return 1;
    }
    return 0;
}


//this is where client sends request, you sould write your code here
int csiebox_client_run(csiebox_client* client) {
    fprintf(stderr, "client start run\n");
  if (!login(client)) {
    fprintf(stderr, "login fail\n");
    return 0;
  }
  fprintf(stderr, "login success\n");
  
  //====================
  //        TODO: add your client-side code here
  //====================
  fprintf(stderr, "start todo\n");
  chdir(client->arg.path);
  memset(event_path, 0, sizeof(event_path));
  memset(hl, 0, sizeof(hl));
  DIR *dir = opendir("./");
  if(dir == NULL) fprintf(stderr, "open dir ./ fail\n");
  struct dirent *entry;
  int num = 0;
  while((entry = readdir(dir)) != NULL){
    ++num;
    if(num > 2) break;
  }
  closedir(dir);
  fprintf(stderr, "num : %d\n", num);
  int length, i = 0;
  int fd;
  int wd;
  char buffer[EVENT_BUF_LEN];
  memset(buffer, 0, EVENT_BUF_LEN);
  fd = inotify_init();
  if (fd < 0) {
    perror("inotify_init err");
  }
  if(num <= 2){
      fprintf(stderr, "client empty\n");

      //meta
      csiebox_protocol_header header;
      memset(&header, 0, sizeof(header));
      while(recv_message(client->conn_fd, &header, sizeof(header))){
          if(header.res.status == CSIEBOX_PROTOCOL_STATUS_OK) break;
          if(header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK){
            csiebox_protocol_hardlink hardlink;
            memset(&hardlink, 0, sizeof(hardlink));
            if(recv_message(client->conn_fd, &hardlink, sizeof(hardlink)) < 0)
                fprintf(stderr, "recieve hardlink header err\n");
            struct stat st;
            memset(&st, 0, sizeof(st));
            if(recv_message(client->conn_fd, &st, sizeof(st)) < 0)
                fprintf(stderr, "recieve st header err\n");
            char buf1[hardlink.message.body.srclen+5], buf2[hardlink.message.body.targetlen+5];
            memset(buf1, 0, sizeof(buf1));
            memset(buf2, 0, sizeof(buf2));
            strcpy(buf1,"./");
            strcpy(buf2,"./");
            if(recv_message(client->conn_fd, buf1+2, hardlink.message.body.srclen) < 0)
                fprintf(stderr, "recieve hardlink header err\n");
            if(recv_message(client->conn_fd, buf2+2, hardlink.message.body.targetlen) < 0)
                fprintf(stderr, "recieve hardlink header err\n");
            if(link(buf1, buf2) < 0) fprintf(stderr, "link err\n");
            struct utimbuf ubuf = {st.st_atime, st.st_mtime};
            utime(buf2, &ubuf);
            continue;
          }
          csiebox_protocol_meta meta;
          memset(&meta, 0, sizeof(meta));
          if(recv_message(client->conn_fd, &meta, sizeof(meta)) < 0)
            fprintf(stderr, "recieve meta err\n");
          header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
          send_message(client->conn_fd, &header, sizeof(header));
          struct stat st = meta.message.body.stat;
          int pathlen = meta.message.body.pathlen;
          char rec_path[pathlen+3];
          memset(rec_path, 0 , sizeof(rec_path));
          strcpy(rec_path,"./");
          if(recv_message(client->conn_fd, rec_path+2, pathlen) < 0)
              fprintf(stderr,"meta recieve path err\n");
          fprintf(stderr, "path recieve:        %s\n", rec_path);
          

          if(S_ISDIR(st.st_mode)){
              mkdir(rec_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
              wd = inotify_add_watch(fd, rec_path, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
              strcpy(event_path[wd], rec_path);
          }
          else if(S_ISLNK(st.st_mode)){
                fprintf(stderr, "sync file is symlink\n");
                //sync file
                csiebox_protocol_file file;
                if(recv_message(client->conn_fd, &file, sizeof(file)) < 0)
                    fprintf(stderr,"sync link header recieve err\n");
                int len = file.message.body.datalen;
                char buf[len+1];
                memset(buf, 0 , sizeof(buf));
                if(recv_message(client->conn_fd, &buf, len) < 0)
                    fprintf(stderr,"sync link recieve err\n");
                fprintf(stderr, "link : %s\n",buf);
                symlink(buf, rec_path);
          }
          else if(S_ISREG(st.st_mode)){
                fprintf(stderr, "sync file is reg\n");
                char buf[N];
                int fd;
                csiebox_protocol_file req;
                memset(buf, 0 , sizeof(buf));
                struct flock fl;
                memset(&fl, 0, sizeof(fl));
                fl.l_type   = F_WRLCK;
                fl.l_whence = SEEK_SET;
                fl.l_start  = 0;
                fl.l_len    = 0;
                fl.l_pid    = getpid();
                FILE *fp = fopen(rec_path, "w");
                fd = fileno(fp);
                if(fcntl(fd, F_SETLKW, &fl) < 0) fprintf(stderr, "lock err\n");
                if(fp == NULL) fprintf(stderr, "open file %s write fail\n", rec_path);
                do{
                    if(recv_message(client->conn_fd, &req, sizeof(req)) < 0)
                        fprintf(stderr,"file recieve header err\n");
                    else fprintf(stderr, "file header recieve\n");
                    if(recv_message(client->conn_fd, buf, req.message.body.datalen) < 0)
                        fprintf(stderr,"file recieve err\n");
                    else fprintf(stderr, "file recieve\n");
                    fwrite(buf,1,req.message.body.datalen,fp);
                }while(req.message.body.datalen == N);
                fl.l_type = F_UNLCK;
                if(fcntl(fd, F_SETLKW, &fl) < 0) fprintf(stderr, "unlock err\n");
                fclose(fp);
                fprintf(stderr, "end of file\n");
          }
          struct utimbuf ubuf = {st.st_atime, st.st_mtime};
          utime(rec_path, &ubuf);
      }
      fprintf(stderr, "end of server pull\n");
  }
  else
      traverse_file(fd, "./", client);
    

    fprintf(stderr, "start watch\n");
    wd = inotify_add_watch(fd, ".", IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
    strcpy(event_path[wd], ".");
    while ((length = read(fd, buffer, EVENT_BUF_LEN)) > 0) {
        i = 0;
        while (i < length) {
            struct inotify_event* event = (struct inotify_event*)&buffer[i];
            char epath[PATH_MAX];
            memset(epath, 0, sizeof(epath));
            strcpy(epath,event_path[event->wd]);
            strcat(epath, "/");
            strcat(epath, event->name);
            fprintf(stderr, "\nevent path: %s\n", epath);
            fprintf(stderr, "event: (%d, %d, %s)\ntype: ", event->wd, strlen(event->name), event->name);
            struct stat st;
            memset(&st, 0 , sizeof(st));
            if(lstat(epath, &st) == -1){
                fprintf(stderr,"get stat on %s Error\n", epath);
                //continue;
            }

            if (event->mask & IN_CREATE) {
                fprintf(stderr, "create \n");
                //if(S_ISDIR(st.st_mode) || !is_hardlink(client->conn_fd, st.st_ino, epath+2))
                sync_file(client->conn_fd, st, epath);
                if(S_ISDIR(st.st_mode)){
                    int wd = inotify_add_watch(fd, epath, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
                    memset(event_path[wd], 0, sizeof(event_path[wd]));
                    strcpy(event_path[wd], epath);
                }
            }
            if (event->mask & IN_DELETE) {
                fprintf(stderr, "delete \n");
                csiebox_protocol_rm rm;
                memset(&rm, 0, sizeof(rm));
                rm.message.body.pathlen = strlen(epath+2);
                rm.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
                rm.message.header.req.op = CSIEBOX_PROTOCOL_OP_RM;
                rm.message.header.req.datalen = sizeof(rm) - sizeof(rm.message.header);
                while(1){
                    send_message(client->conn_fd, &rm, sizeof(rm));
                    csiebox_protocol_header header;
                    memset(&header, 0, sizeof(header));
                    recv_message(client->conn_fd, &header, sizeof(header));
                    if(header.res.status == CSIEBOX_PROTOCOL_STATUS_BUSY){
                        fprintf(stdout, "Server busy\n");
                        sleep(3);
                        continue;
                    }
                    break;
                }
                send_message(client->conn_fd, epath+2, strlen(epath+2));
                if(event->mask & IN_ISDIR){
                    if(inotify_rm_watch(fd, event->wd) < 0)
                        fprintf(stderr, "rm watch err\n");
                }
            }
            if (event->mask & IN_ATTRIB) {
                fprintf(stderr, "attrib \n");
                //send_meta(client->conn_fd, st, epath+2);
                sync_file(client->conn_fd, st, epath);
            }
            if (event->mask & IN_MODIFY) {
                fprintf(stderr, "modify \n");
                sync_file(client->conn_fd, st, epath);
            }
            if (event->mask & IN_ISDIR) {
                fprintf(stderr, "dir\n");
            } else {
                fprintf(stderr, "file\n");
            }
            i += EVENT_SIZE + event->len;
        }
        memset(buffer, 0, EVENT_BUF_LEN);
    }
    //inotify_rm_watch(fd, wd);
    //close(fd);

  return 1;
}

void traverse_file(int fd, char dir_name[], csiebox_client* client){
    DIR *dir = opendir(dir_name);
    if(dir == NULL) return;
    struct dirent *entry;
    struct stat st;
    struct flock fl;
    while((entry = readdir(dir)) != NULL){
        if(entry->d_name[0] == '.') continue;
        char cpath[PATH_MAX];
        if(strlen(dir_name) == 2)
            snprintf(cpath, sizeof(cpath), "%s%s", dir_name, entry->d_name);
        else
            snprintf(cpath, sizeof(cpath), "%s/%s", dir_name, entry->d_name);
        fprintf(stderr, "\ntraverse : [%s]\n",cpath);
        if(lstat(cpath, &st) == -1){
            fprintf(stderr,"get stat on %s Error\n", cpath);
            return;
        }
        fprintf(stderr, "st_nlink : %d\n", st.st_nlink);
        if(!S_ISDIR(st.st_mode) && st.st_nlink != 1){
            int i = 0;
            for(; i < cnt ; i++)
                if(hl[i].inode == st.st_ino){
                    send_hardlink(client->conn_fd, hl[i].path, cpath+2);
                    send_message(client->conn_fd, &st, sizeof(st));
                }
            if(i == cnt){
                sync_file(client->conn_fd, st, cpath);
                hl[cnt].inode = st.st_ino;
                strcpy(hl[cnt].path, cpath+2);
                ++cnt;
            }
        }
        else sync_file(client->conn_fd, st, cpath);
        
        if(DT_DIR == entry->d_type){
            int wd = inotify_add_watch(fd, cpath, IN_CREATE | IN_DELETE | IN_ATTRIB | IN_MODIFY);
            fprintf(stderr, "add watch %d %s\n", wd, cpath);
            strcpy(event_path[wd], cpath);
            traverse_file(fd, cpath, client);
        }
    }
    fprintf(stderr, "end of traverse_file\n");
    closedir(dir);
}

void csiebox_client_destroy(csiebox_client** client) {
  csiebox_client* tmp = *client;
  *client = 0;
  if (!tmp) {
    return;
  }
  close(tmp->conn_fd);
  free(tmp);
}

//read config file
static int parse_arg(csiebox_client* client, int argc, char** argv) {
  if (argc != 2) {
    return 0;
  }
  FILE* file = fopen(argv[1], "r");
  if (!file) {
    return 0;
  }
  fprintf(stderr, "reading config...\n");
  size_t keysize = 20, valsize = 20;
  char* key = (char*)malloc(sizeof(char) * keysize);
  char* val = (char*)malloc(sizeof(char) * valsize);
  ssize_t keylen, vallen;
  int accept_config_total = 5;
  int accept_config[5] = {0, 0, 0, 0, 0};
  while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
    key[keylen] = '\0';
    vallen = getline(&val, &valsize, file) - 1;
    val[vallen] = '\0';
    fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
    if (strcmp("name", key) == 0) {
      if (vallen <= sizeof(client->arg.name)) {
        strncpy(client->arg.name, val, vallen);
        accept_config[0] = 1;
      }
    } else if (strcmp("server", key) == 0) {
      if (vallen <= sizeof(client->arg.server)) {
        strncpy(client->arg.server, val, vallen);
        accept_config[1] = 1;
      }
    } else if (strcmp("user", key) == 0) {
      if (vallen <= sizeof(client->arg.user)) {
        strncpy(client->arg.user, val, vallen);
        accept_config[2] = 1;
      }
    } else if (strcmp("passwd", key) == 0) {
      if (vallen <= sizeof(client->arg.passwd)) {
        strncpy(client->arg.passwd, val, vallen);
        accept_config[3] = 1;
      }
    } else if (strcmp("path", key) == 0) {
      if (vallen <= sizeof(client->arg.path)) {
        strncpy(client->arg.path, val, vallen);
        accept_config[4] = 1;
      }
    }
  }
  free(key);
  free(val);
  fclose(file);
  int i, test = 1;
  for (i = 0; i < accept_config_total; ++i) {
    test &= accept_config[i];
  }
  if (!test) {
    fprintf(stderr, "config error\n");
    return 0;
  }
  return 1;
}

static int login(csiebox_client* client) {
    fprintf(stderr, "start login\n");
  csiebox_protocol_login req;
  memset(&req, 0, sizeof(req));
  req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
  req.message.header.req.op = CSIEBOX_PROTOCOL_OP_LOGIN;
  req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
  memcpy(req.message.body.user, client->arg.user, strlen(client->arg.user));
  md5(client->arg.passwd,
      strlen(client->arg.passwd),
      req.message.body.passwd_hash);
  while(1){
      if (!send_message(client->conn_fd, &req, sizeof(req))) {
        fprintf(stderr, "send fail\n");
        return 0;
      }
      fprintf(stderr, "login sent\n");
      csiebox_protocol_header header;
      memset(&header, 0, sizeof(header));
      if (recv_message(client->conn_fd, &header, sizeof(header))) {
        if (header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
            header.res.op == CSIEBOX_PROTOCOL_OP_LOGIN &&
            header.res.status == CSIEBOX_PROTOCOL_STATUS_OK
            ) {
            client->client_id = header.res.client_id;
            return 1;
        }
        else if(header.res.status == CSIEBOX_PROTOCOL_STATUS_BUSY){
            fprintf(stdout, "Server busy\n");
            sleep(3);
            continue;
        }
      }
      else fprintf(stderr, "recieve message err\n");
      sleep(3);
  }
  return 0;
}
