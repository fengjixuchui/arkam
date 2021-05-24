#include "standard_main.h"


// ===== Error =====

void die(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void guard_err(VM* vm, Code code) {
  if (code != ARK_ERR) return;
  char* e = ark_err_str(vm->err);
  if (e == NULL) e = "unknown error";
  die("%s", e);
}



// ===== Peripheral =====

FILE* stdio_port;

Code handleSTDIO(VM* vm, Cell op) {
  switch (op) {

  case 0: // putc ( c -- )
    if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
    putc(Pop(), stdio_port);
    fflush(stdio_port);
    return ARK_OK;
    
  case 1: // getc ( -- c )
    if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
    Push(getc(stdin));
    return ARK_OK;

  case 2: // query port
    {
      if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
      Cell p = 0;
      
      if      (stdio_port == stdout) { p = 1; }
      else if (stdio_port == stderr) { p = 2; }
      else    { die("Stdio port is invalid"); }
      
      Push(p);
      return ARK_OK;
    }
    
  case 3: // set port
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell p = Pop();
      switch (p) {
      case 1: stdio_port = stdout; break;
      case 2: stdio_port = stderr; break;
      default: die("Unknown stdio port: %d", p);
      }      
      return ARK_OK;
    }

  default:
    Raise(IO_UNKNOWN_OP);    
  }
}


// ----- File -----

#define IO_FILENUM 64
FILE* io_files[IO_FILENUM];

static int find_free_file() {
  int len = IO_FILENUM;
  for (int i = 0; i < len; i++) {
    if (io_files[i] == NULL) return i;
  }
  return -1;
}

static FILE* fetch_file(int id) {
  if (id < 0 || id >= IO_FILENUM) return NULL;
  return io_files[id];
}

Code handleFILE(VM* vm, Cell op) {
  switch (op) {
    
  case 0: // open ( &fname &mode -- id ok | ng )
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      int file_i = find_free_file();
      if (file_i < 0) die("attempt to open too many files");
      
      Code code = ark_pop_valid_addr(vm); ExpectOK;
      char* mode = (char*)vm->mem + vm->result;
      
      code = ark_pop_valid_addr(vm); ExpectOK;
      char* fname = (char*)vm->mem + vm->result;

      FILE* file = fopen(fname, mode);
      if (file == NULL) {
        Push(0);
        return ARK_OK;
      }
      
      io_files[file_i] = file;
      Push(file_i);
      Push(-1);
      return ARK_OK;
    }
    
  case 1: // close ( id -- ? )
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell id = Pop();
      FILE* file = fetch_file(id);
      if (!file) die("invalid file id: %d", id); 
      
      int code = fclose(file);
      Push(code == 0 ? -1 : 0);
      io_files[id] = NULL;
      return ARK_OK;
    }

  case 2: // read ( &buf size id -- ? )
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);
      
      Cell id = Pop();
      FILE* file = fetch_file(id);
      if (!file) die("invalid file id: %d", id);

      Cell size = Pop();
      
      Code code = ark_pop_valid_addr(vm); ExpectOK;
      void* buf = (void*)vm->mem + vm->result;
      
      int got = fread(buf, size, 1, file);
      Push(got == 1 ? -1 : 0);
      return ARK_OK;
    }

  case 3: // write ( &buf size id -- ? )
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);

      Cell id = Pop();
      FILE* file = fetch_file(id);
      if (!file) die("invalid file id: %d", id);

      Cell size = Pop();

      Code code = ark_pop_valid_addr(vm); ExpectOK;
      void* buf = (void*)vm->mem + vm->result;

      int wrote = fwrite(buf, size, 1, file);
      Push(wrote == 1 ? -1 : 0);
      return ARK_OK;
    }

  case 4: // seek ( offset origin id -- ? )
    // origin: 0 SEEK_SET, 1 SEEK_CUR, 2 SEEK_END
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);

      Cell id = Pop();
      FILE* file = fetch_file(id);
      if (!file) die("invalid file id: %d", id);

      Cell origin = Pop();
      switch (origin) {
      case 0: origin = SEEK_SET; break;
      case 1: origin = SEEK_CUR; break;
      case 2: origin = SEEK_END; break;
      default: die("invalid file origin: %d", origin);
      }

      Cell offset = Pop();

      int result = fseek(file, offset, origin);
      Push(result == 0 ? -1 : 0);
      return ARK_OK;
    }

  case 5: // access ( path -- ? )
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Code code = ark_pop_valid_addr(vm); ExpectOK;
      char* path = (char*)vm->mem + vm->result;
      Push(access(path, F_OK) == 0 ? -1 : 0);
      return ARK_OK;
    }
  }

  Raise(IO_UNKNOWN_OP);
}


/* ----- Random ----- */

UCell xorshift(UCell s) {
  s = s ^ (s << 13);
  s = s ^ (s >> 17);
  return s ^ (s << 5);
}

Code handleRANDOM(VM* vm, Cell op) {
  static UCell s = 2463534242;
  
  switch (op) {
  case 0:
    /* gen ( n -- r )  0 <= r < n */    
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell n = Pop();
      if (n < 1) die("Random device(0) requires n>0");
      s = xorshift(s);
      Cell r =
        floor( (double)s / (double)ARK_MAX_UINT * (double)n );
      if (r == n) r = n - 1; // bound check
      Push(r);
      return ARK_OK;
    }

  case 1:
    /* set seed ( n -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell n = Pop();
      if (n == 0) die("Random seed should not be zero");
      s = n;
      return ARK_OK;
    }

  case 2:
    /* init seed randomly (by current time) */
    {
      time_t t = time(NULL);
      s = t;
      for (int i = 0; i < 10; i++) { s = xorshift(s); }
      return ARK_OK;
    }
    
  default: Raise(IO_UNKNOWN_OP);
  }
}



/* ===== Application Process ===== */
int    app_argc = 0;
char** app_argv;

Code handleAPP(VM* vm, Cell op) {
  switch (op) {
  case 0: /* argc ( -- n ) */
    {
      if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
      Push(app_argc);
      return ARK_OK;
    }
  case 1: /* read_argc ( addr i len -- ? ) */
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);
      Cell len = Pop();
      Cell max = len - 1; // null
      if (max < 1) die("Invalid arg length %d", len);
      Cell i   = Pop();
      if (i >= app_argc) die("Invalid argi %d", i);
      Cell addr; PopValid(&addr);
      char* s = app_argv[i];
      
      for (int i = 0; i < len; i++) {
        char c = s[i];
        if (!ark_valid_addr(vm, addr)) die("Invalid addr %d", addr);
        vm->mem[addr] = c;
        if (c == '\0') {
          Push(-1);
          break;
        }
        if (i >= max) { // overflow
          vm->mem[addr] = '\0';
          Push(0);
          break;
        }
        addr++;
      }
      return ARK_OK;
    }
  default: Raise(IO_UNKNOWN_OP);
  }
}

void setup_app(VM* vm, int argc, char* argv[]) {
  app_argc = argc;
  app_argv = argv;
  vm->io_handlers[ARK_DEVICE_APP] = handleAPP;
}



// ===== Setup =====

void read_image(VM* vm, char* fname) {
  FILE* file = fopen(fname, "rb");
  if (!file)
    die("ERROR %s : %s", strerror(errno), fname);

  // get file size
  fseek(file, 0L, SEEK_END);
  int size = ftell(file);
  rewind(file);

  Cell memsize = sizeof(Cell) * vm->cells;
  if (size >= memsize)
    die("Too big image");

  fread(vm->mem, sizeof(Byte), size, file);
}


VM* setup_arkam_vm(char* image_name) {
  stdio_port = stdout;
  
  ArkamVMOptions opts;
  ark_set_default_options(&opts);
  
  VM* vm = ark_new_vm(&opts);
  // devices
  vm->io_handlers[ARK_DEVICE_STDIO]  = handleSTDIO;
  vm->io_handlers[ARK_DEVICE_FILE]   = handleFILE;
  vm->io_handlers[ARK_DEVICE_RANDOM] = handleRANDOM;

  read_image(vm, image_name);

  return vm;
}

