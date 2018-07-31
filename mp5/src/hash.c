#include "hash.h"
#include <stdlib.h>
#include <string.h>
//#include <stdio.h>

int init_hash(hash* h, int n) {
  if (n <= 0 || h->node != NULL) {
    return 0;
  }
  h->node = (hash_node**)malloc(sizeof(hash_node*) * n);
  if (h->node == NULL) {
    return 0;
  }
  memset(h->node, 0, sizeof(hash_node*) * n);
  h->n = n;
  return 1;
}

int put_into_hash(hash* h, void* contain, int hash_code) {
  int pos = hash_code % h->n;
  if (pos < 0) {
    pos = h->n + pos;
  }
  hash_node* n = h->node[pos];
  if (n == NULL) {
    hash_node* new_node = (hash_node*)malloc(sizeof(hash_node));
    if (new_node == NULL) {
      return 0;
    }
    new_node->contain = contain;
    new_node->hash_code = hash_code;
    new_node->next = NULL;
    h->node[pos] = new_node;
    return 1;
  }
  for (; n->next != NULL; n = n->next) {
    if (n->hash_code == hash_code) {
      return 0;
    }
  }
  if (n->hash_code == hash_code) {
    return 0;
  }
  hash_node* new_node = (hash_node*)malloc(sizeof(hash_node));
  if (new_node == NULL) {
    return 0;
  }
  new_node->contain = contain;
  new_node->hash_code = hash_code;
  new_node->next = NULL;
  n->next = new_node;
  return 1;
}

int get_from_hash(hash* h, void** contain, int hash_code) {
  *contain = NULL;
  int pos = hash_code % h->n;
  if (pos < 0) {
    pos = h->n + pos;
  }
  hash_node* n = h->node[pos];
  if (n == NULL) {
    return 0;
  }
  for (; n != NULL; n = n->next) {
    if (n->hash_code == hash_code) {
      *contain = n->contain;
      return 1;
    }
  }
  return 0;
}

int del_from_hash(hash* h, void** contain, int hash_code) {
  *contain = NULL;
  int pos = hash_code % h->n;
  if (pos < 0) {
    pos = h->n + pos;
  }
  hash_node* n = h->node[pos];
  if (n == NULL) {
    return 0;
  }
  hash_node* pre = NULL;
  for (; n != NULL; n = n->next) {
    if (n->hash_code == hash_code) {
      if (pre != NULL) {
        pre->next = n->next;
      } else {
        h->node[pos] = NULL;
      }
      *contain = n->contain;
      free(n);
      return 1;
    }
    pre = n;
  }
  return 0;
}

void clean_hash(hash* h) {
  int i = 0;
  for (i = 0; i < h->n; ++i) {
    hash_node* n = h->node[i];
    while (n != NULL) {
      hash_node* next = n->next;
      free(n);
      n = next;
    }
  }
  free(h->node);
  h->node = NULL;
  h->n = 0;
}

void destroy_hash(hash* h) {
  int i = 0;
  for (i = 0; i < h->n; ++i) {
    hash_node* n = h->node[i];
    while (n != NULL) {
      hash_node* next = n->next;
      free(n->contain);
      free(n);
      n = next;
    }
  }
  free(h->node);
  h->node = NULL;
  h->n = 0;
}
