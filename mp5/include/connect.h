#ifndef _CONNECT_H_
#define _CONNECT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define default_register_port 2500
#define register_protocol_buf_size 512
/*
@param port: which port you want to listen.
@return: socket fd, -1 is fail.
@description: prepare a listening socket for server.
*/
int init_server(int port);

/*
@param servername: which server you want to connect.
@param port: which port of server you want to connect.
@return: socket fd, -1 is fail.
@description: prepare a connected socket for client.
*/
int connect_to(const char* servername, int port);

/*
@return: socket fd, -1 is fail.
@description:
  Prepare listening socket for service.
  It would handle the register part for you.
  If it reports "connect server error" on terminal, please contact to TA.
  It's because of no register server which run by TA is running on the machine.
*/
int server_start();

/*
@param username: which user you want to connect.
@param server: which server you want to connect.
@return: socket fd, -1 is fail.
@description:
  Prepare connected socket for service.
  It would handle request register server, get new port and reconnect for you.
  If it reports "connect server error" on terminal, please contact to TA.
  It's because of no register server which run by TA is running on the machine.
  If it reports "bad server address error" on terminal, it means you give a
  non-exist servername.
  If it reports "server no start" on terminal, it means you should haven't run
  your server.
*/
int client_start(const char* username, const char* server);

#ifdef __cplusplus
}
#endif

#endif
