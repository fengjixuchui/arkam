#if !defined(__ARKAM_STANDARD_MAIN__)
#define __ARKAM_STANDARD_MAIN__

#include "arkam.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "shorthands.h"



// ===== Error =====

void die(char* fmt, ...);
void guard_err(VM* vm, Code code);


// ===== Peripheral =====

extern FILE* stdio_port;
Code handleSTDIO(VM* vm, Cell op);

// ----- File -----
#define IO_FILENUM 64
extern FILE* io_files[IO_FILENUM];
Code handleFILE(VM* vm, Cell op);


/* ----- Random ----- */
UCell xorshift(UCell s);
Code handleRANDOM(VM* vm, Cell op);


/* ===== Application Process ===== */
extern int    app_argc;
extern char** app_argv;

Code handleAPP(VM* vm, Cell op);
void setup_app(VM* vm, int argc, char* argv[]);



// ===== Setup =====

void read_image(VM* vm, char* fname);
VM* setup_arkam_vm(char* image_name);


#endif
