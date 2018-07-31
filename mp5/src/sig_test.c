#include <stdio.h>
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


int main(int argc, char* argv[]){
	char path[PATH_MAX], buf[512];
	memset(path, 0 , sizeof(path));
	strcpy(path, "/tmp2/b04902103/csiebox_server.");
	strcat(path, argv[1]);
	fprintf(stderr, "path : %s\n", path);
	int n;
	while(1){
		FILE *fp = fopen(path, "r");
		while( (n = fread(buf, 1, sizeof(buf), fp) ))
			fprintf(stderr, "%s\n", buf);
		fprintf(stderr, "==================================\n");
		fclose(fp);
	}
}
