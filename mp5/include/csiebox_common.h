#ifndef _CSIEBOX_COMMON_
#define _CSIEBOX_COMMON_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <bsd/md5.h>
#include <sys/stat.h>
#include <linux/limits.h>

/*
 * protocol
 */

#define USER_LEN_MAX 30
#define PASSWD_LEN_MAX 30
#define N 1048576
typedef enum {
  CSIEBOX_PROTOCOL_MAGIC_REQ = 0x90,
  CSIEBOX_PROTOCOL_MAGIC_RES = 0x91,
} csiebox_protocol_magic;

typedef enum {
  CSIEBOX_PROTOCOL_OP_LOGIN = 0x00,
  CSIEBOX_PROTOCOL_OP_SYNC_META = 0x01,
  CSIEBOX_PROTOCOL_OP_SYNC_FILE = 0x02,
  CSIEBOX_PROTOCOL_OP_SYNC_HARDLINK = 0x03,
  CSIEBOX_PROTOCOL_OP_SYNC_END = 0x04,
  CSIEBOX_PROTOCOL_OP_RM = 0x05,
} csiebox_protocol_op;

typedef enum {
  CSIEBOX_PROTOCOL_STATUS_OK = 0x00,
  CSIEBOX_PROTOCOL_STATUS_FAIL = 0x01,
  CSIEBOX_PROTOCOL_STATUS_MORE = 0x02,
  CSIEBOX_PROTOCOL_STATUS_BUSY = 0x03,
  CSIEBOX_PROTOCOL_STATUS_BLOCK = 0x04,
} csiebox_protocol_status;

//common header
typedef union {
  struct {
    uint8_t magic;
    uint8_t op;
    uint8_t status;
    uint16_t client_id;
    //datalen = the length of complete header - the length of common header
    uint32_t datalen;
  } req;
  struct {
    uint8_t magic;
    uint8_t op;
    uint8_t status;
    uint16_t client_id;
    //datalen = the length of complete header - the length of common header
    uint32_t datalen;
  } res;
  uint8_t bytes[9];
} csiebox_protocol_header;

//below are five types of complete header
//header used to login
typedef union {
  struct {
    csiebox_protocol_header header;
    struct {
      uint8_t user[USER_LEN_MAX];
      uint8_t passwd_hash[MD5_DIGEST_LENGTH];
    } body;
  } message;
  uint8_t bytes[sizeof(csiebox_protocol_header) + MD5_DIGEST_LENGTH * 2];
} csiebox_protocol_login;

//header used to send meta data
typedef union {
  struct {
    csiebox_protocol_header header;
    struct {
      //lenght of path of that file or directory under local repository
      uint32_t pathlen;
      //use lstat() to get file meta data
      struct stat stat;
      uint8_t hash[MD5_DIGEST_LENGTH];
    } body;
  } message;
  uint8_t bytes[sizeof(csiebox_protocol_header) +
                4 +
                sizeof(struct stat) +
                MD5_DIGEST_LENGTH];
} csiebox_protocol_meta;

//header used to send data of file
typedef union {
  struct {
    csiebox_protocol_header header;
    struct {
      uint64_t datalen;
    } body;
  } message;
  uint8_t bytes[sizeof(csiebox_protocol_header) + 8];
} csiebox_protocol_file;

//header used to sync hard link
typedef union {
  struct {
    csiebox_protocol_header header;
    struct {
      uint32_t srclen;
      uint32_t targetlen;
    } body;
  } message;
  uint8_t bytes[sizeof(csiebox_protocol_header) + 8];
} csiebox_protocol_hardlink;

//header used to delete file on server side
typedef union {
  struct {
    csiebox_protocol_header header;
    struct {
      uint32_t pathlen;
    } body;
  } message;
  uint8_t bytes[sizeof(csiebox_protocol_header) + 4];
} csiebox_protocol_rm;


typedef struct hlnk{
    ino_t inode;
    char path[PATH_MAX];
}H;


/*
 * utility
 */

// do md5 hash
void md5(const char* str, size_t len, uint8_t digest[MD5_DIGEST_LENGTH]);

// do file md5
int md5_file(const char* path, uint8_t digest[MD5_DIGEST_LENGTH]);

// recv message
int recv_message(int conn_fd, void* message, size_t len);

// copy header and recv remain part of message
int complete_message_with_header(
  int conn_fd, csiebox_protocol_header* header, void* result);

// send message
int send_message(int conn_fd, void* message, size_t len);
void send_meta(int conn_fd, struct stat st, char path[]);
void send_file(int conn_fd, char path[], struct stat st);
void sync_file(int conn_fd, struct stat st, char path[]);
void send_hardlink(int conn_fd, char old_path[], char new_path[]);
#ifdef __cplusplus
}
#endif

#endif
