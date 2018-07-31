#include <stdio.h>
#include "csiebox_common.h"
#include "csiebox_client.h"
#include <stdlib.h>
#include "connect.h"
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

void send_meta(int conn_fd, struct stat st, char path[]){
    fprintf(stderr, "into send meta\n");
	csiebox_protocol_meta req;
	memset(&req, 0, sizeof(req));
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
    req.message.header.req.status = CSIEBOX_PROTOCOL_STATUS_MORE;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    req.message.body.pathlen = strlen(path);
    req.message.body.stat = st;
    if(S_ISLNK(st.st_mode)){
        fprintf(stderr, "is symlink\n");
        char buf[PATH_MAX];
        memset(buf,0,PATH_MAX);
        int len = readlink(path, buf, PATH_MAX);
        md5(buf, len, req.message.body.hash);
    }
    else if(S_ISREG(st.st_mode)){
        fprintf(stderr, "is reg\n");
        if(md5_file(path, req.message.body.hash) < 0)
            fprintf(stderr, "md5_file fail\n");
    }
    while(1){
        csiebox_protocol_header header;
        memset(&header, 0, sizeof(header));
        if (!send_message(conn_fd, &req, sizeof(req))) {
          fprintf(stderr, "send meta fail\n");
        }
        if (recv_message(conn_fd, &header, sizeof(header))) {
            if(header.res.status == CSIEBOX_PROTOCOL_STATUS_BUSY){
                fprintf(stdout, "Server busy\n");
                sleep(3);
                continue;
            }
            break;
        } else fprintf(stderr, "recive 0\n");
    }
    if (!send_message(conn_fd, path, strlen(path))) {
      fprintf(stderr, "send meta path fail\n");
    }
}
void send_file(int conn_fd, char path[], struct stat st){
    fprintf(stderr, "into send file\n");
	csiebox_protocol_file req;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    memset(&req, 0, sizeof(req));
    fl.l_type   = F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    fl.l_pid    = getpid();
    req.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    req.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_FILE;
    req.message.header.req.datalen = sizeof(req) - sizeof(req.message.header);
    char buf[N];
    memset(buf, 0, sizeof(buf));
    int num, fd;
    if(S_ISREG(st.st_mode)){
        FILE *fp = fopen(path, "r");
        fd = fileno(fp);
        if(fcntl(fd, F_SETLKW, &fl) < 0) fprintf(stderr, "rlock err\n");
        if(fp == NULL) fprintf(stderr, "open file %s read fail\n", path);
        do{
            num = fread(buf,1,N,fp);
            req.message.body.datalen = num;
            if (!send_message(conn_fd, &req, sizeof(req))) {
              fprintf(stderr, "send file header fail\n");
            }
            if(!send_message(conn_fd, buf, num))
                fprintf(stderr, "send file fail\n");
            fprintf(stderr, "file data sent\n");
        }while(num == N);
        fl.l_type = F_UNLCK;
        if(fcntl(fd, F_SETLKW, &fl) < 0) fprintf(stderr, "unlock err\n");
        fclose(fp);
        fprintf(stderr, "end of file\n");
    }
    else if(S_ISLNK(st.st_mode)){
        num = readlink(path, buf, sizeof(buf));
        fprintf(stderr,"link: %s\n",buf);
        req.message.body.datalen = num;
        if (!send_message(conn_fd, &req, sizeof(req))) {
            fprintf(stderr, "send file header fail\n");
        }
        if(!send_message(conn_fd, buf, num) && num != 0)
            fprintf(stderr, "send file fail\n");
        fprintf(stderr, "end of file\n");
    }
}
void sync_file(int conn_fd, struct stat st, char path[]){
    fprintf(stderr, "start sync file\n");
    send_meta(conn_fd, st, path+2);
    csiebox_protocol_header header;
    memset(&header, 0 , sizeof(header));
    while(1){
        if(recv_message(conn_fd, &header, sizeof(header))){
            if(header.res.status == CSIEBOX_PROTOCOL_STATUS_BLOCK){
                fprintf(stdout, "Server block\n");
                sleep(3);
                continue;
            }
            if(//header.res.magic == CSIEBOX_PROTOCOL_MAGIC_RES &&
               header.res.op == CSIEBOX_PROTOCOL_OP_SYNC_META ) {
                fprintf(stderr, "recieve from server: %04x\n", header.res.status);
            } else {
                fprintf(stderr, "recive from server fail\n");
            }
            break;
        }
    }
    if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
        fprintf(stderr, "MORE\n");
        send_file(conn_fd, path, st);
    } else {
        fprintf(stderr, "NO MORE\n");
    }
    fprintf(stderr, "end of sync file\n");
}
void send_hardlink(int conn_fd, char old_path[] , char new_path[]){
    csiebox_protocol_hardlink hl;
    memset(&hl, 0, sizeof(hl));
    hl.message.header.req.magic = CSIEBOX_PROTOCOL_MAGIC_REQ;
    hl.message.header.req.op = CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK;
    hl.message.header.req.datalen = sizeof(hl) - sizeof(hl.message.header);
    hl.message.body.srclen = strlen(old_path);
    hl.message.body.targetlen = strlen(new_path);
    csiebox_protocol_header header;
    memset(&header, 0, sizeof(header));
    while(1){
        send_message(conn_fd, &hl, sizeof(hl));
        recv_message(conn_fd, &header, sizeof(header));
        if(header.res.status == CSIEBOX_PROTOCOL_STATUS_BUSY){
            fprintf(stdout, "Server busy\n");
            sleep(3);
            continue;
        }
        break;
    }
    send_message(conn_fd, old_path, strlen(old_path));
    send_message(conn_fd, new_path, strlen(new_path));
}