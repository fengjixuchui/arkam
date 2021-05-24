#include "shorthands.h"

Cell force_get(VM* vm, Cell i) {
  Code code = ark_get(vm, i);
  ExpectOK;
  return vm->result;
}

void force_set(VM* vm, Cell i, Cell v) {
  Code code = ark_set(vm, i, v);
  ExpectOK;
}

Cell force_pop(VM* vm) {
  Code code = ark_pop(vm);
  ExpectOK;
  return vm->result;
}

void force_push(VM* vm, Cell v) {
  Code code = ark_push(vm, v);
  ExpectOK;
}
