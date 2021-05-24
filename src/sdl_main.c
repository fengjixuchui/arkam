#include "arkam.h"
#include "standard_main.h"
#include "sdl_fmsynth.h"
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <getopt.h>
#include <SDL2/SDL.h>

typedef ArkamVM   VM;
typedef ArkamCode Code;


Cell zoom = 2;
#define WIDTH  256
#define HEIGHT 192

Cell poll_step = 5000;
Cell req_poll = 1;


/* ===== Graceful Shutdown ===== */

static void quit(int code) {
  SDL_Quit();
  exit(code);
}



/* ===== Pixel Processing Unit ===== */

#define SPRITE_WIDTH 8
#define SPRITE_SIZE  64 /* 8x8 */
#define SPRITE_NUM   256

// 64 * 4 = 256 ( 1 byte )
#define PALETTES 64
#define COLORS   4
#define Color (ppu->palette_i * COLORS + ppu->color)

typedef struct PPU {
  Cell  width;
  Cell  height;
  Cell  pixels;
  Byte* fg;
  Byte* bg;
  Cell* out;
  Cell  palette_i;
  UCell palettes[PALETTES][COLORS];
  Cell  color; // color number
  int   req_redraw;
  /* 8x8 Sprites */
  Cell sprites[SPRITE_NUM];
  Cell sprite_i;
  /* SDL Specific */
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *texture;
} PPU;


PPU* ppu;


void init_sdl(PPU* ppu) {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    die("Can't initialize SDL: %s", SDL_GetError());
    
  ppu->window = SDL_CreateWindow("Arkam",
                                 SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED,
                                 ppu->width  * zoom,
                                 ppu->height * zoom,
                                 0
                                 );
  
  ppu->renderer = SDL_CreateRenderer(ppu->window, -1, 0);
  if (!ppu->renderer) die("Can't create renderer: %s", SDL_GetError());

  ppu->texture = SDL_CreateTexture(ppu->renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STATIC,
                               ppu->width,
                               ppu->height
                               );
  if (!ppu->texture) die("Can't create texture: %s", SDL_GetError());

  SDL_ShowCursor(SDL_ENABLE);
}

PPU* new_ppu(Cell width, Cell height) {
  PPU* ppu = calloc(sizeof(PPU), 1);
  if (!ppu) die("Can't create ppu");

  ppu->width  = width;
  ppu->height = height;
  Cell pixels = ppu->pixels = width * height;
  

  if (!(ppu->fg  = calloc(sizeof(Byte), pixels))) die("Can't create ppu-fg");
  if (!(ppu->bg  = calloc(sizeof(Byte), pixels))) die("Can't create ppu-bg");
  if (!(ppu->out = calloc(sizeof(Cell), pixels))) die("Can't create ppu-out");

  ppu->palette_i = 0;
  ppu->palettes[0][0] = 0xFF86A35A;
  ppu->palettes[0][1] = 0xFF6F894F;
  ppu->palettes[0][2] = 0xFF58754F;
  ppu->palettes[0][3] = 0xFF32544F;

  init_sdl(ppu);

  return ppu;
}


void draw_ppu(PPU* ppu) {
  // render on-screen buffer to out buffer
  Cell pixels = ppu->pixels;
  for (int i = 0; i < pixels; i++) {
      Cell pixel = ppu->fg[i];
      Cell palette_i = pixel / PALETTES;
      Cell color_i = pixel % PALETTES;
      Cell color = ppu->palettes[palette_i][color_i];
      ppu->out[i] = color;
  }
}

void render_ppu(PPU* ppu) {
  // render out buffer to window
  draw_ppu(ppu);
  Cell bytes_of_line = ppu->width * sizeof(Cell);
  SDL_UpdateTexture(ppu->texture, NULL, ppu->out, bytes_of_line);
  SDL_RenderClear(ppu->renderer);
  SDL_RenderCopy(ppu->renderer, ppu->texture, NULL, NULL);
  SDL_RenderPresent(ppu->renderer);
}


Code handlePPU(VM* vm, Cell op) {
  switch (op) {
  case 0: /* set palette color ( color i -- ) */
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      Cell c = Pop();
      if (i < 0 || i >= COLORS) die("Invalid color number %d", i);
      UCell color = 0xFF000000 | c;
      ppu->palettes[ppu->palette_i][i] = color;
      return ARK_OK;
    }

  case 1: /* set color number ( i -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= COLORS) die("Invalid color number: %d", i);
      ppu->color = i;
      return ARK_OK;
    }

  case 2: /* set palette number ( i -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= PALETTES) die("Invalid palette number: %d", i);
      ppu->palette_i = i;
      return ARK_OK;
    }

  case 3: /* get palette number ( -- i ) */
    {
      if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
      Push(ppu->palette_i);
      return ARK_OK;
    }

  case 10: /* clear */
    {
      Cell pixels = ppu->pixels;
      for (int i = 0; i < pixels; i++) {
        ppu->bg[i] = Color;
      }
      return ARK_OK;
    }

  case 11: /* plot ( x y -- ) */
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      Cell y = Pop();
      Cell x = Pop();
      if (x < 0 || x >= ppu->width)  die("Invalid position x: %d", x);
      if (y < 0 || y >= ppu->height) die("Invalid position y: %d", y);
      ppu->bg[y*ppu->width + x] = Color;
      return ARK_OK;
    }

  case 12: /* ploti ( i -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= ppu->pixels)  die("Invalid index i: %d", i);
      ppu->bg[i] = Color;
      return ARK_OK;      
    }

  case 13: /* switch */
    {
      Byte* tmp = ppu->fg;
      ppu->fg = ppu->bg;
      ppu->bg = tmp;
      ppu->req_redraw = 1;
      return ARK_OK;
    }

  case 14: /* transfer ( addr -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell pixels = ppu->pixels;
      Cell start = Pop();
      Cell end   = start+pixels - 1;
      if (!(ark_valid_addr(vm, start) && ark_valid_addr(vm, end)))
        Raise(INVALID_ADDR);
      memcpy(ppu->bg, vm->mem + start, pixels);
      return ARK_OK;
    }

  case 15: /* copy ( -- ) copy fg to bg */
    {
      memcpy(ppu->bg, ppu->fg, ppu->pixels);
      return ARK_OK;
    }

  case 16: /* width ( -- w ) */
    {
      if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
      Push(ppu->width);
      return ARK_OK;
    }

  case 17: /* height ( -- h ) */
    {
      if (!ark_has_ds_spaces(vm, 1)) Raise(DS_OVERFLOW);
      Push(ppu->height);
      return ARK_OK;
    }

  case 20: /* sprite number ( i -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= SPRITE_NUM) die("Invalid sprite number: %d", i);
      ppu->sprite_i = i;
      return ARK_OK;
    }

  case 21: /* load sprite ( addr -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell a; PopValid(&a);
      if (!ark_valid_addr(vm, a + SPRITE_SIZE - 1))
        die("Invalid address of sprite data %d", a);
      ppu->sprites[ppu->sprite_i] = a;
      return ARK_OK;
    }

  case 22: /* plot sprite to bg ( x y -- ) */
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      Cell oy = Pop();
      Cell ox = Pop();
      Cell addr = ppu->sprites[ppu->sprite_i];
      if (addr == 0) return ARK_OK; // ignore null sprite
      Cell w = ppu->width;
      Cell h = ppu->height;
      Byte* sprite = vm->mem + addr;
      int i = 0;
      for (int dy = 0; dy < SPRITE_WIDTH; dy++) {
        for (int dx = 0; dx < SPRITE_WIDTH; dx++) {
          Cell x = ox + dx;
          Cell y = oy + dy;
          if (x >= 0 && x < w && y >= 0 && y < h) {
            Cell bi = y * w + x;
            if (sprite[i] != 0) ppu->bg[bi] = sprite[i] + (COLORS * ppu->palette_i);
          }
          i++;
        }
      }
      return ARK_OK;
    }
    
  default: Raise(IO_UNKNOWN_OP);
  }
}

void setup_ppu(VM* vm, Cell width, Cell height) {
  ppu = new_ppu(width, height);
  vm->io_handlers[ARK_DEVICE_VIDEO] = handlePPU;  
}


/* ===== Mouse ===== */

typedef struct Mouse {
  Cell x, y;
  Cell lx, ly, lpress;
  Cell rx, ry, rpress;
} Mouse;


Mouse* mouse;


Mouse* new_mouse() {
  Mouse* m = calloc(sizeof(Mouse), 1);
  return m;
}

Code handleMOUSE(VM* vm, Cell op) {
  switch (op) {
  case 0: /* addr pos ( &x &y -- ) */
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      PopValid(&(mouse->y));
      PopValid(&(mouse->x));      
      return ARK_OK;
    }
  case 1: /* addr left ( &x &y &press -- ) */
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);
      PopValid(&(mouse->lpress));
      PopValid(&(mouse->ly));
      PopValid(&(mouse->lx));
      return ARK_OK;
    }
  case 2: /* addr right */
    {
      if (!ark_has_ds_items(vm, 3)) Raise(DS_UNDERFLOW);
      PopValid(&(mouse->rpress));
      PopValid(&(mouse->ry));
      PopValid(&(mouse->rx));
      return ARK_OK;      
    }
  default: Raise(IO_UNKNOWN_OP);
  }
}

Cell clamp(Cell n, Cell min, Cell max) {
  // min <= n < max
  return n < min ? min : (n > max ? max : n);
}

void handle_mouse_event(VM* vm, SDL_Event* ev) {
  Cell x = clamp(ev->motion.x / zoom, 0, ppu->width - 1);
  Cell y = clamp(ev->motion.y / zoom, 0, ppu->height - 1);

  Cell cx, cy, cp;
  switch (ev->button.button) {
  case SDL_BUTTON_LEFT:  cx = mouse->lx; cy = mouse->ly; cp = mouse->lpress; break;
  case SDL_BUTTON_RIGHT: cx = mouse->rx; cy = mouse->ry; cp = mouse->rpress; break;
  }

  switch(ev->type) {
  case SDL_MOUSEBUTTONUP:
    if (cx) Set(cx, x);
    if (cy) Set(cy, y);
    if (cp) Set(cp, 0);
    return;
  case SDL_MOUSEBUTTONDOWN:
    if (cx) Set(cx, x);
    if (cy) Set(cy, y);
    if (cp) Set(cp, -1);    
    return;
  case SDL_MOUSEMOTION:
    if (mouse->x) Set(mouse->x, x);
    if (mouse->y) Set(mouse->y, y);
    return;
  }
}

void setup_mouse(VM* vm) {
  mouse = new_mouse();
  vm->io_handlers[ARK_DEVICE_MOUSE] = handleMOUSE;
}


/* ===== Audio ===== */

void dbg_draw_env(PPU* ppu, double* table, int ox, int oy, int width, int height) {
  const int pixel = 3;

  double step = FM_ENV_TABLE_SIZE / (double)width;
  for (int x = 0; x < width; x++) {
    int i = (int)(x * step);
    if (i < 0 || i >= FM_ENV_TABLE_SIZE) die("Invalid table index:%d step:%lf", i, step);
    double e = table[i];
    int y = height - (height * e);
    int px = ox + x;
    int py = oy + y;
    if (x < 0 || x >= ppu->width)  die("Invalid position x:%d", x);
    if (y < 0 || y >= ppu->height) die("Invalid position y:%d e:%lf", y, e);
    ppu->fg[py*ppu->width + px] = pixel;
  }
}


void dbg_draw_envs(PPU* ppu) {
  dbg_draw_env(ppu, fm_env_table_ed, 8,   8,  100, 48);
  dbg_draw_env(ppu, fm_env_table_eu, 120, 8,  100, 48);
  dbg_draw_env(ppu, fm_env_table_ld, 8,   96, 100, 48);
  dbg_draw_env(ppu, fm_env_table_lu, 120, 96, 100, 48);  
}


void setup_audio(VM* vm) {
  setup_fmsynth(vm);
  vm->io_handlers[ARK_DEVICE_AUDIO] = handleFMSYNTH;
}


/* ===== EMU ===== */

Code handleEMU(VM* vm, Cell op) {
  switch (op) {
  case 0: /* set title ( s -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell s; PopValid(&s);
      SDL_SetWindowTitle(ppu->window, (char*)(vm->mem + s));
      return ARK_OK;
    }
  case 1: /* show/hide cursor ( n -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell n = Pop();
      switch (n) {
      case 0: /* hide */
        SDL_ShowCursor(SDL_DISABLE); break;
      case -1: /* show */
        SDL_ShowCursor(SDL_ENABLE); break;
      default: die("Unknown cursor state: %d", n);
      }
      return ARK_OK;
    }
  case 2: /* poll_step ( n -- ) */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell n = Pop();
      if (n < 1) die("Invalid poll_step: %d", n);
      poll_step = n;
      return ARK_OK;
    }
  case 3: /* poll ( -- ) */
    {
      req_poll = 1;
      return ARK_OK;
    }
  default: Raise(IO_UNKNOWN_OP);
  }
}

void setup_emu(VM* vm) {
  vm->io_handlers[ARK_DEVICE_EMU] = handleEMU;
}


/* ===== Main Loop & Entrypoint ===== */

void poll_sdl_event(VM* vm, PPU* ppu) {
  SDL_Event event;
  
  while (SDL_PollEvent(&event) != 0) {
    switch(event.type) {
    case SDL_QUIT:
      quit(0);
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_EXPOSED) render_ppu(ppu);
      break;
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEMOTION:
      handle_mouse_event(vm, &event);
      break;
    }
  }
}


Code run(VM* vm) {
  Code code = ARK_OK;
 
  while (1) {
    double start = SDL_GetPerformanceCounter();
    
    while (!ppu->req_redraw) {
      poll_sdl_event(vm, ppu);
      req_poll = 0;
      int i = 0;
      while (!ppu->req_redraw && !req_poll && i < poll_step) {
        code = ark_step(vm);
        if (code != ARK_OK) return code;
        i++;
      }
    }

    // dbg_draw_envs(ppu);
    render_ppu(ppu);
    ppu->req_redraw = 0;

    // adjust FPS
    double elapsed = (SDL_GetPerformanceCounter() - start); // count
    double elapsed_msec = elapsed / SDL_GetPerformanceFrequency() * 1000.0f; // count / (count/sec) * msec/sec
    double rest_msec = 16.666f - elapsed_msec;
    SDL_Delay(rest_msec > 0 ? rest_msec : 0);
  }
 
  return code;
}


void usage() {
  fprintf(stderr, "Usage: arkam IMAGE\n");
  exit(1);
}


int handle_opts(int argc, char* argv[]) {
  const char* optstr = "hz";

  struct option long_opts[] =
    { { "help",  no_argument,       NULL, 'h' },
      { "zoom",  required_argument, NULL, 'z' },
    };

  opterr = 0; // disable logging error
  int c;
  int long_index;
  while ((c = getopt_long(argc, argv, optstr, long_opts, &long_index)) != -1) {
    switch (c) {
    case 'h':
      usage();
    case 'z':
      {
        char* invalid = NULL;
        zoom = strtol(optarg, &invalid, 10);
        if (zoom == 0) die("Invalid zoom: %s", optarg);
        break;
      }
    case '?':
      fprintf(stderr, "Unknown option: %c\n", optopt);
      usage();
    }
  }

  return optind;
}


int main(int argc, char* argv[]) {
  int argi     = handle_opts(argc, argv);
  int restc    = argc - argi;
  int image_i  = argi;
  if (restc < 1) usage();  
  int app_argi = argi + 1;
  int app_argc = restc - 1;
  char* image_name = argv[image_i];

  VM* vm = setup_arkam_vm(image_name);

  setup_ppu(vm, WIDTH, HEIGHT);
  setup_mouse(vm);
  setup_audio(vm);
  setup_emu(vm);
  setup_app(vm, app_argc, argv + app_argi);

  Code code = ark_get(vm, ARK_ADDR_START);
  guard_err(vm, code);
  vm->ip = vm->result;

  code = run(vm);
  guard_err(vm, code);

  ark_free_vm(vm);
  return 0;
}
