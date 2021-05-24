#include "sdl_fmsynth.h"


#define SAMPLE_RATE 44100
#define AUDIO_SAMPLES 1024
#define FADE_MS 5
#define MAX_ATK_MS 5000
#define MAX_DCY_MS 3000
#define MAX_REL_MS 5000
#define MAX_FM_LEVEL 16 /* 0-16 */


/* ===== Shorthands ===== */

typedef ArkamVM   VM;
typedef ArkamCode Code;


#define WAVE_TABLE_SIZE FM_WAVE_TABLE_SIZE
#define ENV_TABLE_SIZE FM_ENV_TABLE_SIZE
#define env_table_ed fm_env_table_ed
#define env_table_eu fm_env_table_eu
#define env_table_ld fm_env_table_ld
#define env_table_lu fm_env_table_lu



/* ===== Structs ===== */


/* ----- FSM for Envelope -----
   Envelope including Note On/Off for each operators is
   managed by Finite State Machine.
   EnvState transits by EnvEvent.
   Function envelope_amp should be called for each samples,
   handles transitions, and returns sample amplitude.
*/

typedef enum {
  EnvS_Invalid = 0,
  EnvS_Silence = 1,
  EnvS_AttackInit,
  EnvS_Attack,
  EnvS_Decay,
  EnvS_SustainInit,
  EnvS_Sustain,
  EnvS_ReleaseInit,
  EnvS_Release,
  EnvS_CrossFreqInit,
  EnvS_CrossFreq,
  EnvStateCount
} EnvState;


typedef enum {
  EnvE_Continue = 0,
  EnvE_NoteOn,
  EnvE_NoteOff,
  EnvE_Next,
  EnvE_Stop,
  EnvEventCount
} EnvEvent;



/* ----- Modulation Algorithm ----- */

typedef enum {
  /* same as YM2151 */
  FMAlgo0,
  FMAlgo1,
  FMAlgo2,
  FMAlgo3,
  FMAlgo4,
  FMAlgo5,
  FMAlgo6,
  FMAlgo7,
  FMAlgoCount
} FMAlgo;



/* ----- Modulation Ratio ----- */

typedef enum {
  FMRatio1,  // 1
  FMRatioD2,  // 0.25
  FMRatioD1,  // 0.5
  FMRatio2,  // 2
  FMRatio3,  // 3
  FMRatio4,  // 4
  FMRatio5,  // 5
  FMRatio6,  // 6
  FMRatio7,  // 7
  FMRatio8,  // 8
  FMRatio9,  // 9
  FMRatio10, // 10
  FMRatio11, // 11
  FMRatio12, // 12
  FMRatio13, // 13
  FMRatio14, // 14
  FMRatio15, // 15
  FMRatio16, // 16
  FMRatioCount
} FMRatio;



/* ----- Operator ----- */

typedef struct FMOp {
  EnvState state;
  EnvEvent next_event;
  double   next_freq;

  /* ----- parameter ----- */
  double  vol;     // 0-1
  double  freq;    // Hz
  double* wave_table;
  double  amp_freq_mod; // 0-1
  double  feedback_ratio;
  FMRatio freq_ratio;
  double  fm_level;
  /* ADSR */
  double* atk_env;   // table
  int     atk_len;   // ms  
  double* dcy_env;   // table
  int     dcy_len;   // ms
  double  sus_vol;   // rate(0-1)
  double* rel_env;   // table
  int     rel_len;   // ms
  
  /* ----- status ----- */
  double amp;     // 0-1
  double phase_index; // for sine table
  double phase_delta; // for sine table
  double feedback;
  /* attack */
  double  atk_amp;   // base amp
  int     atk_rest;  // frame
  double  atk_diff;  // amp
  double  atk_step;
  double  atk_index;
  /* decay */
  int     dcy_rest;  // frame
  double  dcy_diff;  // amp, diff to sus_amp
  double  dcy_step;
  double  dcy_index;
  /* sustain */
  double  sus_amp;   // amp
  /* release */
  int     rel_rest;  // frame
  double  rel_diff;  // amp, diff from zero
  double  rel_step;
  double  rel_index;
  /* cross freq */
  int    crs_rest;  // frame
  double crs_delta; // phase
} FMOp;



/* ----- Voice(Channel) ----- 
   Currently each channel has only one voice
   so that voices are handled as channels.
*/

typedef struct FMVoice {
  double  vol;     // 0-1

  FMOp ops[FM_OPERATORS];
  FMAlgo algo;
} FMVoice;



/* FM Synthesizer Root */

typedef struct FMSynth {
  double vol; // 0-1
  double* ms_env;  // byte to ms env
  double* vol_env; // byte to vol env
  FMVoice voices[FM_VOICES];
} FMSynth;


  
/* ===== Global Variables ===== */

static SDL_AudioDeviceID audio_id;
static int current_operator = 0;
static int current_voice = 0;

double fm_sine_table[FM_WAVE_TABLE_SIZE];
double fm_saw_table[FM_WAVE_TABLE_SIZE];
double fm_sq_table[FM_WAVE_TABLE_SIZE];
double fm_noise_table[FM_WAVE_TABLE_SIZE];
double fm_env_table_ed[FM_ENV_TABLE_SIZE]; // early  down
double fm_env_table_eu[FM_ENV_TABLE_SIZE]; // early  up
double fm_env_table_ld[FM_ENV_TABLE_SIZE]; // lately down
double fm_env_table_lu[FM_ENV_TABLE_SIZE]; // lately up


/* ----- Envelope Transition Table ----- */
static int env_trans[EnvStateCount][EnvEventCount];

static FMSynth synth;



/* ===== Error ===== */

static void die(char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}



/* ===== Utils ===== */

static double byte_to_env_vol(Byte b, double* table) {
  // return 0-1
  const double step = (ENV_TABLE_SIZE-1) / (double)256;
  int i = b * (int)step;
  return table[i];  
}


static int byte_to_env_uint(Byte b, int max, double* table) {
  if (max < 0) die("Invalid max:%d", max);
  double v = byte_to_env_vol(b, table);
  return max * v;
}


double xorshift_vol() {
  /* return 0-1 */
  static UCell s = 2463534242;
  s = s ^ (s << 13);
  s = s ^ (s >> 17);
  s = s ^ (s << 5);
  return s / (double)ARK_MAX_UINT;
}



/* ===== Envelope ===== */

static double phase_delta(double freq) {
  /* ----- Calculate the value of delta -----
     sample_rate = frame/sec
     freq        = period/sec
     table_size  = sample/period
     delta       = sample/frame
                 = table_size * freq / sample_rate
   */
  return (double)WAVE_TABLE_SIZE * freq / (double)SAMPLE_RATE;
}


static void fm_next_state(FMOp* op) {
  EnvEvent e = op->next_event;
  if (e == EnvE_Continue) return; // no transition

  EnvState s = env_trans[op->state][e];
  if (s == 0)
    die("Invalid state %d with event %d", op->state, e);
  op->next_event = EnvE_Continue;
  op->state = s;
}


static void calc_ads(FMOp* op) {
  /* attack */
  op->atk_amp   = op->amp; // start from current amp
  op->atk_rest  = (op->atk_len + FADE_MS) / (double)1000 * SAMPLE_RATE; // frames
  op->atk_index = 0.0;
  op->atk_step  = (ENV_TABLE_SIZE-1) / (double)op->atk_rest;
  op->atk_diff  = op->vol - op->amp;

  /* decay & sustain */
  op->dcy_rest  = op->dcy_len / (double)1000 * SAMPLE_RATE; // frames
  op->sus_amp   = op->vol * op->sus_vol;
  op->dcy_diff  = op->vol - op->sus_amp;
  op->dcy_index = 0.0;
  op->dcy_step  = (ENV_TABLE_SIZE-1) / (double)op->dcy_rest;
}


static void calc_release(FMOp* op) {
  op->rel_rest  = (op->rel_len + FADE_MS) / (double)1000 * SAMPLE_RATE; // frames
  op->rel_diff  = op->amp; // diff from zero
  op->rel_index = 0.0;
  op->rel_step  = (ENV_TABLE_SIZE-1) / (double)op->rel_rest;
}


static void calc_cross(FMOp* op) {
  const int rest = FADE_MS * SAMPLE_RATE / (double)1000;
  op->crs_rest  = rest;
  double next_delta = phase_delta(op->next_freq);
  op->crs_delta = (next_delta - op->phase_delta) / (double)rest;
}

static double mod_ratio(FMOp* op) {
  switch (op->freq_ratio) {
  case FMRatio1:   return 1.0;
  case FMRatio2:   return 2.0;
  case FMRatio3:   return 3.0;
  case FMRatio4:   return 4.0;
  case FMRatio5:   return 5.0;
  case FMRatio6:   return 6.0;
  case FMRatio7:   return 7.0;
  case FMRatio8:   return 8.0;
  case FMRatio9:   return 9.0;
  case FMRatio10:  return 10.0;
  case FMRatio11:  return 11.0;
  case FMRatio12:  return 12.0;
  case FMRatio13:  return 13.0;
  case FMRatio14:  return 14.0;
  case FMRatio15:  return 15.0;
  case FMRatio16:  return 16.0;    
  case FMRatioD2:  return 0.25;
  case FMRatioD1:  return 0.5;
  default: die("Unknown ratio: %d", op->freq_ratio);
  }
  return 0.0; // do not reach here
}


static void note_on_voice(FMVoice* voice, double freq) {
  FMOp* op1 = &(voice->ops[0]);
  FMOp* op2 = &(voice->ops[1]);
  FMOp* op3 = &(voice->ops[2]);
  FMOp* op4 = &(voice->ops[3]);

  double f1;
  double f2;
  double f3;
  double f4;

  switch (voice->algo) {
  case FMAlgo0: /* 1m 2m 3m 4c */
  case FMAlgo1:
  case FMAlgo2:
  case FMAlgo3:
  case FMAlgo7: /* 1c 2c 3c 4c */    
    {
      f1 = freq * mod_ratio(op1);
      f2 = freq * mod_ratio(op2);
      f3 = freq * mod_ratio(op3);
      f4 = freq;
      break;
    }
  case FMAlgo4: /* 1m 2c 3m 4c */
    {
      f1 = freq * mod_ratio(op1);
      f3 = freq * mod_ratio(op3);
      f2 = f4 = freq;
      break;
    }
  case FMAlgo5: /* 1m 2c 3c 4c */
  case FMAlgo6:
    {
      f1 = freq * mod_ratio(op1);
      f2 = f3 = f4 = freq;
      break;
    }
  default:
    die("Unknown algo in note on: %d", voice->algo);
  }

  op1->next_freq = f1;
  op2->next_freq = f2;
  op3->next_freq = f3;
  op4->next_freq = f4;

  op1->feedback = 0.0;
  op2->feedback = 0.0;
  op3->feedback = 0.0;
  op4->feedback = 0.0;
  
  op1->next_event = EnvE_NoteOn;
  op2->next_event = EnvE_NoteOn;
  op3->next_event = EnvE_NoteOn;
  op4->next_event = EnvE_NoteOn;  
}


static void note_off_voice(FMVoice* voice) {
  voice->ops[0].next_event = EnvE_NoteOff;
  voice->ops[1].next_event = EnvE_NoteOff;
  voice->ops[2].next_event = EnvE_NoteOff;
  voice->ops[3].next_event = EnvE_NoteOff;  
}


static double envelope_amp(FMOp* op) {
  fm_next_state(op);

  switch (op->state) {
  case EnvS_Silence:
    {
      return op->amp = 0.0f;
    }
  case EnvS_AttackInit:
    {
      op->next_event = EnvE_Next;
      op->freq = op->next_freq;
      op->phase_delta = phase_delta(op->freq);
      
      /* reset index for noise as percussion */
      if (op->wave_table == fm_noise_table) op->phase_index = 0;
      
      calc_ads(op);
      return op->amp;
    }
  case EnvS_Attack:
    {
      double diff = op->atk_diff * op->atk_env[(int)op->atk_index];
      op->amp = op->atk_amp + diff;
      op->atk_rest--;
      op->atk_index += op->atk_step;
      op->next_event = op->atk_rest > 0 ? EnvE_Continue : EnvE_Next;      
      return op->amp;
    }
  case EnvS_Decay:
    {
      double diff = op->dcy_diff * op->dcy_env[(int)op->dcy_index];
      op->amp = op->sus_amp + diff;
      op->dcy_rest--;
      op->dcy_index += op->dcy_step;
      op->next_event = op->dcy_rest > 0 ? EnvE_Continue : EnvE_Next;
      return op->amp;      
    }
  case EnvS_SustainInit:
    {
      op->next_event = EnvE_Next;
      return op->amp = op->sus_amp;
    }
  case EnvS_Sustain:
    {
      op->next_event = EnvE_Continue;
      return op->amp;
    }
  case EnvS_ReleaseInit:
    {
      op->next_event = EnvE_Next;
      calc_release(op);
      return op->amp;
    }
  case EnvS_Release:
    {
      op->amp = op->rel_diff * op->rel_env[(int)op->rel_index];
      op->rel_rest--;
      op->rel_index += op->rel_step;
      op->next_event = op->rel_rest > 0 ? EnvE_Continue : EnvE_Next;      
      return op->amp;
    }
  case EnvS_CrossFreqInit:
    {
      op->next_event = EnvE_Next;
      calc_cross(op);
      return op->amp;
    }
  case EnvS_CrossFreq:
    {
      op->phase_delta += op->crs_delta;
      if (op->phase_delta < 0) op->phase_delta = 0.0f;
      op->crs_rest--;
      op->next_event = op->crs_rest > 0 ? EnvE_Continue : EnvE_Next;
      return op->amp;
    }
  default:
    die("Invalid state: %d", op->state);
  }
  
  return 0.0f; // do not reach here
}



/* ===== Audio Generation ===== */

#define AMP_FREQ_LEVEL 8
static double advance_sin(FMOp* op, double mod) {
  double amp = envelope_amp(op);
  double sin = amp * op->wave_table[(int)op->phase_index];
  
  /* advance */
  mod = (mod + 1.0) / 2.0 /* -1-1 to 0-1 */ * op->fm_level; // -1-1 to 0-16
  double ampmod = op->phase_delta * amp * op->amp_freq_mod * (double)AMP_FREQ_LEVEL;
  double delta = op->phase_delta * mod + ampmod;
  op->phase_index += delta;
  while (op->phase_index >= WAVE_TABLE_SIZE) op->phase_index -= WAVE_TABLE_SIZE;

  op->feedback = sin;
  return sin;
}


static double feedback(FMOp* op) {
  return op->feedback * op->feedback_ratio;
}


static double generate_sin(FMVoice* voice, FMAlgo algo) {
  FMOp* op1 = &voice->ops[0];
  FMOp* op2 = &voice->ops[1];
  FMOp* op3 = &voice->ops[2];
  FMOp* op4 = &voice->ops[3];

  switch (algo) {
  case FMAlgo0:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, sin1 * feedback(op2));
      double sin3 = advance_sin(op3, sin2 * feedback(op3));
      double sin4 = advance_sin(op4, sin3 * feedback(op4));
      double sin = sin4 * voice->vol;
      return sin;
    }
  case FMAlgo1:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, feedback(op2));
      double sin3 = advance_sin(op3, sin1 * sin2);
      double sin4 = advance_sin(op4, sin3 * feedback(op4));
      double sin = sin4 * voice->vol;
      return sin;
    }
  case FMAlgo2:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, feedback(op2));
      double sin3 = advance_sin(op3, sin2 * feedback(op3));
      double sin4 = advance_sin(op4, sin1 * sin3 * feedback(op4));
      double sin = sin4 * voice->vol;
      return sin;
    }
  case FMAlgo3:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, sin1 * feedback(op2));
      double sin3 = advance_sin(op3, feedback(op3));
      double sin4 = advance_sin(op4, sin2 * sin3 * feedback(op4));
      double sin = sin4 * voice->vol;
      return sin;
    }
  case FMAlgo4:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, sin1 * feedback(op2));
      double sin3 = advance_sin(op3, feedback(op3));
      double sin4 = advance_sin(op4, sin3 * feedback(op4));
      double sin = (sin2 + sin4) / 2.0 * voice->vol;
      return sin;
    }    
  case FMAlgo5:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, sin1 * feedback(op2));
      double sin3 = advance_sin(op3, sin1 * feedback(op3));
      double sin4 = advance_sin(op4, sin1 * feedback(op4));
      double sin = (sin2 + sin3 + sin4) / 3.0 * voice->vol;
      return sin;
    }
  case FMAlgo6:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, sin1 * feedback(op2));
      double sin3 = advance_sin(op3, feedback(op3));
      double sin4 = advance_sin(op4, feedback(op4));
      double sin = (sin2 + sin3 + sin4) / 3.0 * voice->vol;
      return sin;
    }
  case FMAlgo7:
    {
      double sin1 = advance_sin(op1, feedback(op1));
      double sin2 = advance_sin(op2, feedback(op2));
      double sin3 = advance_sin(op3, feedback(op3));
      double sin4 = advance_sin(op4, feedback(op4));
      double sin = (sin1 + sin2 + sin3 + sin4) / 4.0 * voice->vol;
      return sin;
    }    
  default: die("Unknown algorithm: %d", voice->algo);
  }
  return 0.0;
}



static void audio_callback(void* u, Byte* stream, int bytes) {
  SDL_memset(stream, 0, bytes);
  int16_t* samples = (int16_t *)stream;
  int len = bytes / 2; // 8bit len => 16bit len

  FMVoice* v1 = &synth.voices[0];
  FMVoice* v2 = &synth.voices[1];
  FMVoice* v3 = &synth.voices[2];
  FMVoice* v4 = &synth.voices[3];
  FMVoice* v5 = &synth.voices[4];
  FMVoice* v6 = &synth.voices[5];
  FMVoice* v7 = &synth.voices[6];
  FMVoice* v8 = &synth.voices[7];
  FMAlgo a1 = v1->algo;
  FMAlgo a2 = v2->algo;
  FMAlgo a3 = v3->algo;
  FMAlgo a4 = v4->algo;
  FMAlgo a5 = v5->algo;
  FMAlgo a6 = v6->algo;
  FMAlgo a7 = v7->algo;
  FMAlgo a8 = v8->algo;
  
  for(int i = 0; i < len; i+=2) {
    //TODO: panning
    double sin1 = generate_sin(v1, a1);
    double sin2 = generate_sin(v2, a2);
    double sin3 = generate_sin(v3, a3);
    double sin4 = generate_sin(v4, a4);
    double sin5 = generate_sin(v5, a5);
    double sin6 = generate_sin(v6, a6);
    double sin7 = generate_sin(v7, a7);
    double sin8 = generate_sin(v8, a8);
    int16_t sample = (
                      synth.vol
                      * (sin1 + sin2 + sin3 + sin4 + sin5 + sin6 + sin7 + sin8)
                      / 8.0 * 0x7FFF
                      ); // half of 16bit
    samples[i]   = sample;
    samples[i+1] = sample;
  }
}



/* ===== Arkam I/O Handler ===== */

static void io_set_param(FMVoice* voice, Cell op_i, Cell param, Cell v) {
  FMOp* op = &(voice->ops[op_i]);

  switch (param) {
  case 0: /* attack(ms) */
    {
      Cell ms = byte_to_env_uint(v, MAX_ATK_MS, synth.ms_env);
      op->atk_len = ms;
      return;
    }
  case 1: /* decay(ms) */
    {
      Cell ms = byte_to_env_uint(v, MAX_DCY_MS, synth.ms_env);
      op->dcy_len = ms;
      return;
    }
  case 2: /* sustain(vol) */
    {
      op->sus_vol = byte_to_env_vol(v, synth.vol_env);
      return;
    }
  case 3: /* release(ms) */
    {
      Cell ms = byte_to_env_uint(v, MAX_REL_MS, synth.ms_env);
      op->rel_len = ms;
      return;
    }
  case 4: /* modulation ratio */
    {
      if (v < 0 || v >= FMRatioCount) die("Invalid ratio: %d", v);
      op->freq_ratio = v;
      return;
    }
  case 5: /* feedback ratio 0-255 to 0-1*/
    {
      op->feedback_ratio = byte_to_env_vol(v, synth.vol_env);
      return;
    }
  case 6: /* wave table 0-2 */
    {
      if (v < 0 || v > 3) die("Invalid wave table: %d", v);
      switch (v) {
      case 0: op->wave_table = fm_sine_table; break;
      case 1: op->wave_table = fm_saw_table; break;
      case 2: op->wave_table = fm_sq_table; break;
      case 3: op->wave_table = fm_noise_table; break;
      }
      return;
    }
  case 7: /* amp-freq mod */
    {
      op->amp_freq_mod = byte_to_env_vol(v, synth.ms_env);
      return;
    }
  case 8: /* fm-level 0-255 */
    {
      op->fm_level = byte_to_env_vol(v, synth.ms_env) * (double)MAX_FM_LEVEL;
      return;
    }
  default:
    die("Unknown param:%d v:%d", param, v);
  }
}


Code handleFMSYNTH(VM* vm, Cell op) {
  switch (op) {
  case 0: /* set currnt voice: i -- */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= FM_VOICES) die("Invalid voice: %d", i);
      current_voice = i;
      return ARK_OK;
    }
  case 1: /* set current operator: i -- */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell i = Pop();
      if (i < 0 || i >= FM_OPERATORS) die("Invalid operator: %d", i);
      current_operator = i;
      return ARK_OK;
    }
  case 2: /* play: freq -- */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell freq = Pop();
      FMVoice* voice = &(synth.voices[current_voice]);
      SDL_LockAudioDevice(audio_id);
      note_on_voice(voice, freq);
      SDL_UnlockAudioDevice(audio_id);
      return ARK_OK;
    }
  case 3: /* stop: -- */
    {
      FMVoice* voice = &(synth.voices[current_voice]);      
      SDL_LockAudioDevice(audio_id);
      note_off_voice(voice);
      SDL_UnlockAudioDevice(audio_id);
      return ARK_OK;
    }
  case 4: /* set_param: v param -- */
    // 0:atk 1:dcy 2:sus 3:rel 4:ratio 5:feedback_ratio 6:wave_table
    {
      if (!ark_has_ds_items(vm, 2)) Raise(DS_UNDERFLOW);
      Cell param = Pop();
      Cell v = Pop();
      FMVoice* voice = &(synth.voices[current_voice]);      
      SDL_LockAudioDevice(audio_id);
      io_set_param(voice, current_operator, param, v);
      SDL_UnlockAudioDevice(audio_id);
      return ARK_OK;
    }
  case 5: /* set_algo: algo -- */
    {
      if (!ark_has_ds_items(vm, 1)) Raise(DS_UNDERFLOW);
      Cell algo = Pop();
      FMVoice* voice = &(synth.voices[current_voice]);      
      if (algo < 0 || algo >= FMAlgoCount) die("Invalid algo: %d", algo);
      SDL_LockAudioDevice(audio_id);
      voice->algo = algo;
      SDL_UnlockAudioDevice(audio_id);
      return ARK_OK;
    }
  default: Raise(IO_UNKNOWN_OP);          
  }
}



/* ===== Entrypoint ===== */


#define Trans(from, e, next) env_trans[EnvS_##from][EnvE_##e] = EnvS_##next

static void fm_init_transitions() {
  memset(env_trans, 0, sizeof(env_trans));
  
  Trans(Silence, NoteOn,  AttackInit);
  Trans(Silence, NoteOff, Silence);

  Trans(AttackInit, Next,    Attack);
  Trans(AttackInit, NoteOn,  CrossFreqInit);
  Trans(AttackInit, NoteOff, ReleaseInit);

  Trans(Attack, Next,    Decay);
  Trans(Attack, NoteOn,  CrossFreqInit);
  Trans(Attack, NoteOff, ReleaseInit);

  Trans(Decay, Next,    SustainInit);
  Trans(Decay, NoteOn,  CrossFreqInit);
  Trans(Decay, NoteOff, ReleaseInit);

  Trans(SustainInit, Next,    Sustain);
  Trans(SustainInit, NoteOn,  CrossFreqInit);
  Trans(SustainInit, NoteOff, ReleaseInit);  

  Trans(Sustain, Next,    ReleaseInit);
  Trans(Sustain, NoteOn,  CrossFreqInit);
  Trans(Sustain, NoteOff, ReleaseInit);

  Trans(ReleaseInit, Next,    Release);
  Trans(ReleaseInit, NoteOn,  CrossFreq);
  Trans(ReleaseInit, NoteOff, Release);

  Trans(Release, Next,    Silence);
  Trans(Release, NoteOn,  CrossFreqInit);
  Trans(Release, NoteOff, Release);

  Trans(CrossFreqInit, Next,    CrossFreq);
  Trans(CrossFreqInit, NoteOn,  CrossFreqInit);
  Trans(CrossFreqInit, NoteOff, CrossFreq);

  Trans(CrossFreq, Next,    AttackInit);
  Trans(CrossFreq, NoteOn,  CrossFreqInit);
  Trans(CrossFreq, NoteOff, ReleaseInit);
}


static void init_operator(FMOp* op) {
  memset(op, 0, sizeof(FMOp));

  op->state = EnvS_Silence;
  op->next_event = EnvE_Continue;

  op->wave_table = fm_sine_table;
  
  double freq = 440;
  op->vol = 1.0f;
  op->amp = 0.0f;
  op->freq = freq;
  op->next_freq = freq;
  op->phase_index = 0.0f;
  op->phase_delta = phase_delta(freq);
  op->feedback = 0.0;
  op->feedback_ratio = 0.0;
  op->freq_ratio = FMRatio1;
  op->amp_freq_mod = 0.0;
  op->fm_level = 1.0;

  /* default adsr */
  op->atk_env = env_table_eu;
  op->atk_len = 10; // ms
  
  op->dcy_env = env_table_ed;
  op->dcy_len = 50;  // ms
  
  op->sus_vol = 0.3;
  
  op->rel_env = env_table_ld;
  op->rel_len = 500; // ms
}


static void init_voice(FMVoice* voice) {
  memset(voice, 0, sizeof(FMVoice));
  voice->algo = FMAlgo0;
  voice->vol = 1.0f;

  init_operator(&(voice->ops[0]));
  init_operator(&(voice->ops[1]));
  init_operator(&(voice->ops[2]));
  init_operator(&(voice->ops[3]));
}


static void init_synth(FMSynth* synth) {
  memset(synth, 0, sizeof(FMSynth));
  synth->vol = 0.3f;
  synth->ms_env  = env_table_lu;
  synth->vol_env = env_table_eu;
  
  for (int i = 0; i < FM_VOICES; i++) {
    init_voice(&(synth->voices[i]));
  }
}


static void init_wave_table() {
  /* ----- sine ----- */
  for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
    fm_sine_table[i] = sin(2.0 * M_PI * i / (double) WAVE_TABLE_SIZE);
  }

  
  /* ----- saw ----- */
  double n = -1.0;
  double step = 2 / (double)WAVE_TABLE_SIZE;
  for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
    fm_saw_table[i] = n;
    n += step;
  }
  fm_saw_table[WAVE_TABLE_SIZE-1] = 1.0;

  
  /* ----- sq ----- */
  int half = WAVE_TABLE_SIZE / 2;
  for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
    fm_sq_table[i] = i < half ? -1 : 1;
  }


  /* ----- noise ----- */
  for (int i = 0; i < WAVE_TABLE_SIZE; i++) {
    fm_noise_table[i] = xorshift_vol() * 2.0 - 1.0; // 0..1 to -1..1
  }
}


static void init_env_tables() {
  const double minimum = 0.001;
  /* ----- early down ----- */
  // double m = 1.0 + (log(1) - log(minimum)) / (double)ENV_TABLE_SIZE;
  double m = pow(minimum, 1 / (double)ENV_TABLE_SIZE);
  double v = 1.0;
  for (int i = 0; i < ENV_TABLE_SIZE; i++) {
    env_table_ed[i] = v;
    v *= m;
  }
  env_table_ed[ENV_TABLE_SIZE - 1] = 0.0f;

  /* ----- early up / vertical mirror */
  for (int i = 0; i < ENV_TABLE_SIZE; i++) {
    env_table_eu[i] = 1.0 - env_table_ed[i];
  }

  /* ----- lately up / horizontal mirror */
  for (int i = 0; i < ENV_TABLE_SIZE; i++) {
    env_table_lu[i] = env_table_ed[ENV_TABLE_SIZE - 1 - i];
  }

  /* ----- lately down / h&v mirror */
  for (int i = 0; i < ENV_TABLE_SIZE; i++) {
    env_table_ld[i] = 1.0 - env_table_lu[i];
  }
}


void setup_fmsynth(VM* vm) {
  fm_init_transitions();
  
  SDL_AudioSpec spec;
  
  SDL_zero(spec);
  spec.freq = SAMPLE_RATE;
  spec.format = AUDIO_S16;
  spec.channels = 2;
  spec.samples = AUDIO_SAMPLES;
  spec.callback = audio_callback;
  audio_id = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);

  if (!audio_id)
    die("Can't open audio: %s", SDL_GetError());

  current_operator = 0;
  current_voice = 0;
  init_wave_table();
  init_env_tables();
  init_synth(&synth);

  SDL_PauseAudioDevice(audio_id, 0); // unpause = start playing
}
