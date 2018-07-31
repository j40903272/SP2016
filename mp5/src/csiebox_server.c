#include "csiebox_server.h"

#include "csiebox_common.h"
#include "connect.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <stdlib.h>
#include <utime.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

static int parse_arg(csiebox_server* server, int argc, char** argv);
static void handle_request(void* info);
static int get_account_info(
	csiebox_server* server,  const char* user, csiebox_account_info* info);
static void login(
	csiebox_server* server, int conn_fd, csiebox_protocol_login* login);
static void logout(csiebox_server* server, int conn_fd);
static char* get_user_homedir(
	csiebox_server* server, csiebox_client_info* info);

#define DIR_S_FLAG (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)//permission you can use to create new file
#define REG_S_FLAG (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)//permission you can use to create new directory

fd_set masterset;
csiebox_server* siguse;
void sig_usr(int signo){
	fprintf(stderr, "into sig_usr\n");
	if(signo == SIGUSR1){
		fprintf(stderr, "SIGUSR1 received\n");
		int fd = open(siguse->arg.fifo_path, O_WRONLY), n = 0;
		//FILE *fp = fopen(siguse->arg.fifo_path, "w");
		uint32_t threads = siguse->client_num;
		//for(int i = 0 ; i < getdtablesize() ; i++) if(FD_ISSET(i, &masterset)) ++n;
		threads = htonl(threads);
		write(fd, &threads, sizeof(threads));
		//fwrite(&threads, sizeof(threads), 1, fp);
		//fclose(fp);
		close(fd);
	} else {
		fprintf(stderr, "sig err\n");
	}
	fprintf(stderr, "end of sig_usr\n");
}
void sig_del_fifo(int signo){
	fprintf(stderr, "into sig_del_fifo\n");
	if(signo == SIGTERM){
		fprintf(stderr, "received SIGTERM\n");
	}
	else if(signo == SIGINT){
		fprintf(stderr, "received SIGINT\n");
	} else {
		fprintf(stderr, "sig err\n");
	}
	csiebox_server_destroy(&siguse);
	exit(0);
}

//read config file, and start to listen
void csiebox_server_init(csiebox_server** server, int argc, char** argv) {
	csiebox_server* tmp = (csiebox_server*)malloc(sizeof(csiebox_server));
	if (!tmp) {
		fprintf(stderr, "server malloc fail\n");
		return;
	}
	memset(tmp, 0, sizeof(csiebox_server));
	if (!parse_arg(tmp, argc, argv)) {
		fprintf(stderr, "Usage: %s [config file]\n", argv[0]);
		free(tmp);
		return;
	}
	int fd = server_start();
	if (fd < 0) {
		fprintf(stderr, "server fail\n");
		free(tmp);
		return;
	}
	tmp->client_num = 0;
	tmp->client = (csiebox_client_info**)malloc(sizeof(csiebox_client_info*) * getdtablesize());
	if (!tmp->client) {
		fprintf(stderr, "client list malloc fail\n");
		close(fd);
		free(tmp);
		return;
	}
	memset(tmp->client, 0, sizeof(csiebox_client_info*) * getdtablesize());
	tmp->listen_fd = fd;

	memset(tmp->pid, 0, sizeof(tmp->pid));
	memset(tmp->arg.fifo_path, 0, sizeof(tmp->arg.fifo_path));

	tmp->threads = (pthread_t*)malloc(sizeof(pthread_t) * tmp->arg.thread_num);
	memset(tmp->threads, 0, sizeof(pthread_t) * tmp->arg.thread_num);
	tmp->pool = threadpool_create(tmp->arg.thread_num, tmp->arg.thread_num, 0);
	if(tmp->pool == NULL) fprintf(stderr, "pool create err\n");


	*server = tmp;
	siguse = tmp;
	if(signal(SIGUSR1, sig_usr) < 0) fprintf(stderr, "cant catch SIGUSR1\n");
	if(signal(SIGTERM, sig_del_fifo) < 0) fprintf(stderr, "cant catch SIGTERM\n");
	if(signal(SIGINT, sig_del_fifo) < 0) fprintf(stderr, "cant catch SIGINT\n");
	//atexit(csiebox_server_destroy);
	
}

//wait client to connect and handle requests from connected socket fd
//===============================
//		TODO: you need to modify code in here and handle_request() to support I/O multiplexing
//===============================

H hl[200][110];
int cnt[200];
int csiebox_server_run(csiebox_server* server) {
	fprintf(stderr, "server start run\n");
	memset(hl, 0, sizeof(hl));
	int conn_fd, conn_len, maxfd = 3;
	struct sockaddr_in addr;

	//struct flock fl;
	fd_set workset;
	FD_ZERO(&masterset);
	FD_SET(server->listen_fd, &masterset);
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	memset(&addr, 0, sizeof(addr));
	conn_len = 0;
	fprintf(stderr, "server listen_fd : %d \n", server->listen_fd);

	int flag;
	req_arg thread_info[server->arg.thread_num];
	memset(thread_info, 0, sizeof(req_arg) * server->arg.thread_num);
	fprintf(stderr, "server->arg.thread_num : %d\n", server->arg.thread_num);

	strcpy(server->arg.fifo_path, server->arg.run_path);
	strcat(server->arg.fifo_path, "csiebox_server.pid");
	fprintf(stderr, "pid file path : %s\n", server->arg.fifo_path);

	FILE *fp = fopen(server->arg.fifo_path, "r");
	if(fp == NULL){
		fprintf(stderr, "%s open err\n", server->arg.fifo_path);
		exit(0);
	}
	fscanf(fp, "%s", server->pid);
	fclose(fp);

	fprintf(stderr, "pid : %s\n", server->pid);
	//fprintf(stderr, "pid : %d\n", getpid());
	//sprintf(server->pid, "%d", getpid());

	strcpy(server->arg.fifo_path + strlen(server->arg.fifo_path) - 3, server->pid);
	if(mkfifo(server->arg.fifo_path, 0644) < 0) fprintf(stderr, "mkfifo failed\n");

	fprintf(stderr, "listen_fd : %d\n", server->listen_fd);
	while (1) {

		// waiting client connect
		FD_ZERO(&workset);
		workset = masterset;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;
		if(select(maxfd+1, &workset, NULL, NULL, &timeout) < 0){
			fprintf(stderr,"select error\n");
			fprintf(stderr, "%d %s\n", errno, strerror(errno));
			if(errno == EINTR) continue;
		}
		//fprintf(stderr, "selected\n");
		if(FD_ISSET(server->listen_fd, &workset)){
				fprintf(stderr, "yes\n");
				conn_len = sizeof(addr);
				conn_fd = accept(server->listen_fd, (struct sockaddr*)&addr, (socklen_t*)&conn_len);
				if (conn_fd < 0) {
					if (errno == ENFILE) {
							fprintf(stderr, "out of file descriptor table\n");
							continue;
						} else if (errno == EAGAIN || errno == EINTR) {
							continue;
						} else {
							fprintf(stderr, "accept err\n");
							fprintf(stderr, "code: %s\n", strerror(errno));
							break;
						}
				}
				FD_SET(conn_fd, &masterset);
				FD_CLR(server->listen_fd, &workset);
				if(conn_fd > maxfd) maxfd = conn_fd;
				fprintf(stderr, "///maxfd : %d\n",maxfd);
		}
		int n = 0;
		for(int i = 4 ; i <= maxfd ; i++){
				if(FD_ISSET(i,&workset)){
						// handle request from connected socket fd
						
						/*flag = 0;
						for(int j = 0 ; j < server->arg.thread_num ; j++){
								if(thread_info[j].complete && thread_info[j].conn_fd == i){
										flag = 1;
										break;
								}
						}
						if(flag) continue;
						fprintf(stderr, "head %d count %d\n", server->pool->head, server->pool->count);
						csiebox_protocol_header header;
						memset(&header, 0, sizeof(header));
						if(server->pool->count == server->arg.thread_num){
								fprintf(stderr, "busy\n");
								header.res.status = CSIEBOX_PROTOCOL_STATUS_BUSY;
								send_message(i, &header, sizeof(header));
								continue;
						}
						thread_info[server->pool->count] = (req_arg){server, i, 1};
						int err = threadpool_add(server->pool, &handle_request, (void*)thread_info + server->pool->count, 0);
						fprintf(stderr, "end\n");*/
						thread_info[0] = (req_arg){server, i, 1};
						handle_request((void*)thread_info);
				}
		}
	}
	return 1;
}

void csiebox_server_destroy(csiebox_server** server) {
	csiebox_server* tmp = *server;
	*server = 0;
	if (!tmp) {
		return;
	}
	unlink(tmp->arg.fifo_path);
	fprintf(stderr, "unlink %s \n", tmp->arg.fifo_path);
	strcat(tmp->arg.run_path, "csiebox_server.pid");
	remove(tmp->arg.run_path);
	fprintf(stderr, "remove %s \n", tmp->arg.run_path);

	close(tmp->listen_fd);
	int i = getdtablesize() - 1;
	for (; i >= 0; --i) {
		if (tmp->client[i]) {
			free(tmp->client[i]);
		}
	}
	threadpool_destroy(tmp->pool, 0);
	free(tmp->threads);
	free(tmp->client);
	free(tmp);
}

//read config file
static int parse_arg(csiebox_server* server, int argc, char** argv) {
	if (argc > 3 || argc < 1) {
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
	int accept_config_total = 4;
	int accept_config[4] = {0, 0, 0, 0};
	while ((keylen = getdelim(&key, &keysize, '=', file) - 1) > 0) {
		key[keylen] = '\0';
		vallen = getline(&val, &valsize, file) - 1;
		val[vallen] = '\0';
		fprintf(stderr, "config (%d, %s)=(%d, %s)\n", keylen, key, vallen, val);
		if (strcmp("path", key) == 0) {
			if (vallen <= sizeof(server->arg.path)) {
				strncpy(server->arg.path, val, vallen);
				accept_config[0] = 1;
			}
		} else if (strcmp("account_path", key) == 0) {
			if (vallen <= sizeof(server->arg.account_path)) {
				strncpy(server->arg.account_path, val, vallen);
				accept_config[1] = 1;
			}
		} else if (strcmp("thread_num", key) == 0) {
				server->arg.thread_num = atoi(val);
				accept_config[2] = 1;
		} else if (strcmp("run_path", key) == 0) {
				strncpy(server->arg.run_path, val, vallen);
				strcat(server->arg.run_path, "/");
				accept_config[3] = 1;
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
void traverse_file(char dir_name[], int len, int conn_fd);
//this is where the server handle requests, you should write your code here
static void handle_request(void* info) {
	fprintf(stderr, "into handle request\n");
	//pthread_mutex_t lock;
	//pthread_mutex_init(&lock, NULL);
	//pthread_mutex_lock(&lock);
	csiebox_server* server = ((req_arg*)info)->server;
	int conn_fd = ((req_arg*)info)->conn_fd;
	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	if (recv_message(conn_fd, &header, sizeof(header))) {
		if (header.req.magic != CSIEBOX_PROTOCOL_MAGIC_REQ) {
				fprintf(stderr, "header magic err\n");
				pthread_exit(NULL);
				return;
		}
		switch (header.req.op) {
				char filepath[PATH_MAX];
				memset(filepath, 0, sizeof(filepath));

			case CSIEBOX_PROTOCOL_OP_LOGIN:
				fprintf(stderr, "login\n");
				csiebox_protocol_login req;
				if (complete_message_with_header(conn_fd, &header, &req)) {
					login(server, conn_fd, &req);
				}
				strcpy(filepath, get_user_homedir(server, server->client[conn_fd]));
				
				fprintf(stderr, "login path : %s\n", filepath);
				DIR *dir = opendir(filepath);
				struct dirent *entry;
				int num = 0, len = strlen(filepath);
				while((entry = readdir(dir)) != NULL){
					++num;
					if(num > 2) break;
				}
				closedir(dir);
				if(num > 2){
						fprintf(stderr, "server not empty\n");
						fprintf(stderr, "start send from server\n");
						traverse_file(filepath, len+1, conn_fd);
						csiebox_protocol_header header;
						header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
						send_message(conn_fd, &header, sizeof(header));
						usleep(10000);
				}
				++server->client_num;
				break;


			case CSIEBOX_PROTOCOL_OP_SYNC_META:
				send_message(conn_fd, &header, sizeof(header));
				strcpy(filepath, get_user_homedir(server, server->client[conn_fd]));
				strcat(filepath, "/");
				fprintf(stderr, "sync meta: [");
				csiebox_protocol_meta meta;
				if (complete_message_with_header(conn_fd, &header, &meta)) {
					//====================
					//        TODO: here is where you handle sync_meta and even sync_file request from client
					//====================
						struct stat st = meta.message.body.stat;
						int pathlen = meta.message.body.pathlen, fd;
						char rec_path[pathlen+1];
						memset(rec_path,0,sizeof(rec_path));
						if(recv_message(conn_fd, rec_path, pathlen) < 0)
								fprintf(stderr,"meta recieve path err\n");
						strcat(filepath, rec_path);
						fprintf(stderr, "%s]\n" ,filepath);

						//csiebox_protocol_header header;
						memset(&header, 0, sizeof(header));
						header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
						header.res.op = CSIEBOX_PROTOCOL_OP_SYNC_META;
						header.res.datalen = 0;

						if(S_ISDIR(st.st_mode)){
								mkdir(filepath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
								header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
						}
						else if(S_ISLNK(st.st_mode)){
								fprintf(stderr, "sync file is symlink\n");
								char path[PATH_MAX];
								uint8_t buf[MD5_DIGEST_LENGTH];
								memset(path,0,sizeof(path));
								memset(buf,0,sizeof(buf));
								int len;
								if((len = readlink(filepath, path, sizeof(path))) < 0){
										header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
								}
								else md5(path,len,buf);
								if(!memcmp(meta.message.body.hash, buf, MD5_DIGEST_LENGTH))
										header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
								else
										header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;

						}
						else if(S_ISREG(st.st_mode)){
								fprintf(stderr, "sync file is reg\n");
								uint8_t buf[MD5_DIGEST_LENGTH];
								memset(buf,0,sizeof(buf));
								if(md5_file(filepath, buf) < 0 || memcmp(meta.message.body.hash, buf, MD5_DIGEST_LENGTH) != 0)
										header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
								else
										header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
						}
						
						fprintf(stderr, "%04x\n", header.res.status);
						struct flock fl;
						memset(&fl, 0, sizeof(fl));
								
						

						if(header.res.status == CSIEBOX_PROTOCOL_STATUS_MORE){
								csiebox_protocol_file req;
								memset(&req, 0, sizeof(req));
								char buf[N];
								if(S_ISLNK(st.st_mode)){
										send_message(conn_fd, &header, sizeof(header));
										if(recv_message(conn_fd, &req, sizeof(req)) < 0)
												fprintf(stderr,"file recieve header err\n");
										else fprintf(stderr, "file header recieve\n");
										if(recv_message(conn_fd, buf, req.message.body.datalen) < 0)
												fprintf(stderr,"file recieve err\n");
										else fprintf(stderr, "file recieve\n");
										symlink(buf, filepath);
								} else if(S_ISREG(st.st_mode)) {
										FILE *fp = fopen(filepath, "w");
										fd = fileno(fp);
										while(1){
												if(fcntl(fd, F_GETLK, &fl) < 0) fprintf(stderr, "get lock err\n");
												if(fl.l_type != F_UNLCK) header.res.status == CSIEBOX_PROTOCOL_STATUS_BLOCK;
												send_message(conn_fd, &header, sizeof(header));
												if(fl.l_type == F_UNLCK) break;
												sleep(3);
										}


										fl.l_type   = F_WRLCK;
										fl.l_whence = SEEK_SET;
										fl.l_start  = 0;
										fl.l_len    = 0;
										fl.l_pid    = getpid();
										if(fcntl(fd, F_SETLK, &fl) < 0) fprintf(stderr, "wlock err\n");
										if(fp == NULL) fprintf(stderr, "open file %s write fail\n", filepath);
										do{                        
												if(recv_message(conn_fd, &req, sizeof(req)) < 0)
														fprintf(stderr,"file recieve header err\n");
												else fprintf(stderr, "file header recieve\n");
												//if(!req.message.body.datalen) break;
												if(recv_message(conn_fd, buf, req.message.body.datalen) < 0)
														fprintf(stderr,"file recieve err\n");
												else fprintf(stderr, "file recieve\n");
												fwrite(buf,1,req.message.body.datalen,fp);
										}while(req.message.body.datalen == N);
										fl.l_type = F_UNLCK;
										if(fcntl(fd, F_SETLK, &fl) < 0) fprintf(stderr, "unlock err\n");
										fclose(fp);
										fprintf(stderr, "end of file\n");
								}
						} else {
								send_message(conn_fd, &header, sizeof(header));
						}
						struct utimbuf ubuf = {st.st_atime, st.st_mtime};
						utime(filepath, &ubuf);


				}
				break;
			case CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK:
				fprintf(stderr, "sync hardlink\n");
				send_message(conn_fd, &header, sizeof(header));
				strcpy(filepath, get_user_homedir(server, server->client[conn_fd]));
				strcat(filepath, "/");
				char tmp[PATH_MAX];
				memset(tmp, 0, sizeof(tmp));
				strcpy(tmp,filepath);
				csiebox_protocol_hardlink hardlink;
				if (complete_message_with_header(conn_fd, &header, &hardlink)) {
					//====================
					//        TODO: here is where you handle sync_hardlink request from client
					//====================
						char src_path[hardlink.message.body.srclen+1], target_path[hardlink.message.body.targetlen+1];
						memset(src_path, 0, sizeof(src_path));
						memset(target_path, 0, sizeof(target_path));
						if(recv_message(conn_fd, src_path, hardlink.message.body.srclen) < 0)
								fprintf(stderr, "hardlink path recieve fail\n");
						if(recv_message(conn_fd, target_path, hardlink.message.body.targetlen) < 0)
								fprintf(stderr, "hardlink path recieve fail\n");
						struct stat st;
						memset(&st, 0, sizeof(st));
						if(recv_message(conn_fd, &st, sizeof(st)) < 0)
								fprintf(stderr, "hardlink st recieve err\n");
						strcat(filepath, src_path);
						strcat(tmp, target_path);
						if(link(filepath, tmp) < 0)
								fprintf(stderr, "link err\n");
						struct utimbuf ubuf = {st.st_atime, st.st_mtime};
						utime(tmp, &ubuf);
				}
				break;
			case CSIEBOX_PROTOCOL_OP_SYNC_END:
				fprintf(stderr, "sync end\n");
				csiebox_protocol_header end;
					//====================
					//        TODO: here is where you handle end of synchronization request from client
					//====================
				break;
			case CSIEBOX_PROTOCOL_OP_RM:
				fprintf(stderr, "rm\n");
				send_message(conn_fd, &header, sizeof(header));
				strcpy(filepath, get_user_homedir(server, server->client[conn_fd]));
				strcat(filepath, "/");
				csiebox_protocol_rm rm;
				if (complete_message_with_header(conn_fd, &header, &rm)) {
					//====================
					//        TODO: here is where you handle rm file or directory request from client
					//====================
						char buf[rm.message.body.pathlen+1];
						memset(buf, 0, sizeof(buf));
						if(recv_message(conn_fd, buf, rm.message.body.pathlen) < 0)
								fprintf(stderr,"rm file recieve path err\n");
						strcat(filepath, buf);
						fprintf(stderr, "rm path : %s\n", filepath);
						if(remove(filepath) < 0)
								fprintf(stderr, "remove err\n");
				}
				break;
			default:
				fprintf(stderr, "unknown op %x\n", header.req.op);
				break;
		}
	}
	else{
				fprintf(stderr, "end of connection\n");
				fprintf(stderr, "====================================logout\n");
				--server->client_num;
				((req_arg*)info)->complete = 0;
				logout(server, conn_fd);
	}
	fprintf(stderr, "end of handle request\n");
	((req_arg*)info)->complete = 0;
	//pthread_mutex_unlock(&lock);
}

void traverse_file(char dir_name[], int len, int conn_fd){
		DIR *dir = opendir(dir_name);
		if(dir == NULL) return;
		struct dirent *entry;
		struct stat st;
		while((entry = readdir(dir)) != NULL){
				if(entry->d_name[0] == '.') continue;
				char cpath[PATH_MAX];
				snprintf(cpath, sizeof(cpath), "%s/%s", dir_name, entry->d_name);
				fprintf(stderr, "traverse [%s]\n",cpath);
				memset(&st, 0, sizeof(st));
				if(lstat(cpath, &st) == -1){
						fprintf(stderr,"get stat on %sError\n",cpath);
						return;
				}
				csiebox_protocol_header header;
				memset(&header, 0 , sizeof(header));
				header.res.status = CSIEBOX_PROTOCOL_STATUS_MORE;
				if(!S_ISDIR(st.st_mode) && st.st_nlink != 1){
						int i = 0;
						for(; i < cnt[conn_fd] ; i++)
								if(hl[conn_fd][i].inode == st.st_ino){
										send_message(conn_fd, &header, sizeof(header));
										send_hardlink(conn_fd, hl[conn_fd][i].path, cpath+len);
										send_message(conn_fd, &st, sizeof(st));
								}
						if(i == cnt[conn_fd]){
								send_message(conn_fd, &header, sizeof(header));
								send_meta(conn_fd, st, cpath+len);
								send_file(conn_fd, cpath, st);
								hl[conn_fd][cnt[conn_fd]].inode = st.st_ino;
								strcpy(hl[conn_fd][cnt[conn_fd]].path, cpath+len);
								++cnt[conn_fd];
						}
				} else {
						send_message(conn_fd, &header, sizeof(header));
						send_meta(conn_fd, st, cpath+len);
						send_file(conn_fd, cpath, st);
				}

				
				if(S_ISDIR(st.st_mode)) traverse_file(cpath, len, conn_fd);
		}
		fprintf(stderr, "end of traverse_file\n");
		closedir(dir);
}


//open account file to get account information
static int get_account_info(csiebox_server* server,  const char* user, csiebox_account_info* info) {
	FILE* file = fopen(server->arg.account_path, "r");
	if (!file) {
		return 0;
	}
	size_t buflen = 100;
	char* buf = (char*)malloc(sizeof(char) * buflen);
	memset(buf, 0, buflen);
	ssize_t len;
	int ret = 0;
	int line = 0;
	while ((len = getline(&buf, &buflen, file) - 1) > 0) {
		++line;
		buf[len] = '\0';
		char* u = strtok(buf, ",");
		if (!u) {
			fprintf(stderr, "illegal form in account file, line %d\n", line);
			continue;
		}
		if (strcmp(user, u) == 0) {
			memcpy(info->user, user, strlen(user));
			char* passwd = strtok(NULL, ",");
			if (!passwd) {
				fprintf(stderr, "illegal form in account file, line %d\n", line);
				continue;
			}
			md5(passwd, strlen(passwd), info->passwd_hash);
			ret = 1;
			break;
		}
	}
	free(buf);
	fclose(file);
	return ret;
}

//handle the login request from client
static void login(csiebox_server* server, int conn_fd, csiebox_protocol_login* login) {
	fprintf(stderr, "into login\n");
	int succ = 1;
	csiebox_client_info* info = (csiebox_client_info*)malloc(sizeof(csiebox_client_info));
	memset(info, 0, sizeof(csiebox_client_info));
	if (!get_account_info(server, login->message.body.user, &(info->account))) {
		fprintf(stderr, "cannot find account\n");
		succ = 0;
	}
	if (succ &&
			memcmp(login->message.body.passwd_hash,
						 info->account.passwd_hash,
						 MD5_DIGEST_LENGTH) != 0) {
		fprintf(stderr, "passwd miss match\n");
		succ = 0;
	}
	fprintf(stderr, "1\n");
	csiebox_protocol_header header;
	memset(&header, 0, sizeof(header));
	header.res.magic = CSIEBOX_PROTOCOL_MAGIC_RES;
	header.res.op = CSIEBOX_PROTOCOL_OP_LOGIN;
	header.res.datalen = 0;
	if (succ) {
		fprintf(stderr, "2\n");
		if (server->client[conn_fd]) {
			free(server->client[conn_fd]);
		}
		info->conn_fd = conn_fd;
		server->client[conn_fd] = info;
		header.res.status = CSIEBOX_PROTOCOL_STATUS_OK;
		header.res.client_id = info->conn_fd;
		char* homedir = get_user_homedir(server, info);
		mkdir(homedir, DIR_S_FLAG);
		free(homedir);
	} else {
		header.res.status = CSIEBOX_PROTOCOL_STATUS_FAIL;
		free(info);
	}
	fprintf(stderr, "login send message\n");
	send_message(conn_fd, &header, sizeof(header));
}

static void logout(csiebox_server* server, int conn_fd) {
	free(server->client[conn_fd]);
	server->client[conn_fd] = 0;
	close(conn_fd);
	FD_CLR(conn_fd, &masterset);
}

static char* get_user_homedir(csiebox_server* server, csiebox_client_info* info) {
	char* ret = (char*)malloc(sizeof(char) * PATH_MAX);
	memset(ret, 0, PATH_MAX);
	sprintf(ret, "%s/%s", server->arg.path, info->account.user);
	return ret;
}

void daemonize(const char* cmd, csiebox_server* server){
	fprintf(stderr, "start daemonize\n");
	pid_t pid = fork();
	if(pid < 0){
		fprintf(stderr, "fork failed\n");
		exit(1);
	}
	else if(pid > 0) exit(0);
	umask(0);
	if(setsid() < 0){
		fprintf(stderr, "setsid fialed\n");
		exit(1);
	}
	signal(SIGHUP, SIG_IGN);
	pid = fork();
	if(pid < 0){
		fprintf(stderr, "fork failed\n");
		exit(1);
	}
	else if(pid > 0) exit(0);
	//mkdir(server->arg.run_path, 0644);
	char buf[64];
	memset(buf, 0, sizeof(buf));
	strcpy(buf, server->arg.run_path);
	strcat(buf, "csiebox_server.pid");
	FILE *fp = fopen(buf, "w");
	fprintf(fp, "%d", getpid());
	fclose(fp);
	close(0); close(1); close(2);
	fopen("/tmp2/b04902103/log", "w");
	dup(0);
	dup(0);
}