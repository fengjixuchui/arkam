#include "standard_main.h"


void usage() {
  fprintf(stderr, "Usage: arkam IMAGE\n");
  exit(1);
}


int main(int argc, char* argv[]) {
  if (argc != 2) usage();

  VM* vm = setup_arkam_vm(argv[1]);

  Code code = ark_get(vm, ARK_ADDR_START);
  guard_err(vm, code);
  vm->ip = vm->result;

  code = ark_run(vm);
  guard_err(vm, code);

  code = ark_pop(vm);
  guard_err(vm, code);
  Cell r = vm->result;

  ark_free_vm(vm);
  return r;
}
