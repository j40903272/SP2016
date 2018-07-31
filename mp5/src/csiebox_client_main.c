#include "csiebox_client.h"

//where the server starts
int main(int argc, char** argv) {
  csiebox_client* box = 0;
  csiebox_client_init(&box, argc, argv);
  if (box) {
    csiebox_client_run(box);
  }
  csiebox_client_destroy(&box);
  return 0;
}
