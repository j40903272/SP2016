#ifndef _HASH_H_
#define _HASH_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hash_node hash_node;
typedef struct hash hash;

struct hash_node {
  void* contain;
  int hash_code;
  hash_node* next;
};

struct hash {
  hash_node** node;
  int n;
};

int init_hash(hash* h, int n);
int put_into_hash(hash* h, void* contain, int hash_code);
int get_from_hash(hash* h, void** contain, int hash_code);
int del_from_hash(hash* h, void** conatin, int hash_code);
void clean_hash(hash* h);
void destroy_hash(hash* h);

#ifdef __cplusplus
}
#endif

#endif
