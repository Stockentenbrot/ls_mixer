/*
** Copyright (c) 2017 rxi
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
**/

// some small changes and IIR filter functionality by Laurin Schnorr marked with 'LS'

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h> // LS

#define CM_USE_STB_VORBIS
#include "cmixer.h"
#include "ls_mixer.h"
#define CM_MAX_CB_QUEUE LS_MIXER_NCHANNEL // (LS)

#define UNUSED(x)         ((void) (x))
#define CLAMP(x, a, b)    ((x) < (a) ? (a) : (x) > (b) ? (b) : (x))
#define MIN(a, b)         ((a) < (b) ? (a) : (b))
#define MAX(a, b)         ((a) > (b) ? (a) : (b))

#define FX_BITS           (12)
#define FX_UNIT           (1 << FX_BITS)
#define FX_MASK           (FX_UNIT - 1)
#define FX_FROM_FLOAT(f)  ((f) * FX_UNIT)
#define FX_LERP(a, b, p)  ((a) + ((((b) - (a)) * (p)) >> FX_BITS))




cm_Source *cb_queue[CM_MAX_CB_QUEUE];


static struct {
  const char *lasterror;        /* Last error message */
  cm_EventHandler lock;         /* Event handler for lock/unlock events */
  double (*time_function)(void);/* Function that provides a continiously advancing time in seconds (LS) */ 
  cm_Source *sources;           /* Linked list of active (playing) sources */
  cm_Int32 buffer[BUFFER_SIZE]; /* Internal master buffer */
  int samplerate;               /* Master samplerate */
  int gain;                     /* Master gain (fixed point) */
  // Master-IIR stuff:
  cm_Int16 xl[2],xr[2],x0l,x0r;
  cm_Int16 yl[2],yr[2],y0l,y0r;
  double a1,a2,b0,b1,b2;
} cmixer;


static void dummy_handler(cm_Event *e) {
  UNUSED(e);
}


static void lock(void) {
  cm_Event e;
  e.type = CM_EVENT_LOCK;
  cmixer.lock(&e);
}


static void unlock(void) {
  cm_Event e;
  e.type = CM_EVENT_UNLOCK;
  cmixer.lock(&e);
}


const char* cm_get_error(void) {
  const char *res = cmixer.lasterror;
  cmixer.lasterror = NULL;
  return res;
}


static const char* error(const char *msg) {
  cmixer.lasterror = msg;
  return msg;
}


void cm_init(int samplerate) {
  cmixer.samplerate = samplerate;
  cmixer.lock = dummy_handler;
  cmixer.sources = NULL;
  cmixer.gain = FX_UNIT;
  
  // (LS) initialize IIR buffers
  cmixer.xl[0] = 0;
  cmixer.xl[1] = 0;
  cmixer.yl[0] = 0;
  cmixer.yl[1] = 0;
  cmixer.xr[0] = 0;
  cmixer.xr[1] = 0;
  cmixer.yr[0] = 0;
  cmixer.yr[1] = 0;
  cmixer.x0l = 0;
  cmixer.x0r = 0;
  cmixer.y0l = 0;
  cmixer.y0r = 0;
  
  // (LS) initialize IIR coefficients
  cmixer.a1 = 0.0;
  cmixer.a2 = 0.0;
  cmixer.b0 = 1.0;
  cmixer.b1 = 0.0;
  cmixer.b2 = 0.0; 
}


void cm_set_lock(cm_EventHandler lock) {
  cmixer.lock = lock;
}

void cm_set_time_function(double (time_function)(void)) /* (LS) */
{
	cmixer.time_function = time_function;
	return;
}

void cm_set_master_gain(double gain) {
  cmixer.gain = FX_FROM_FLOAT(gain);
}


static void rewind_source(cm_Source *src) {
  cm_Event e;
  e.type = CM_EVENT_REWIND;
  e.udata = src->udata;
  src->handler(&e);
  src->position = 0;
  src->rewind = 0;
  src->end = src->length;
  src->nextfill = 0;
}


static void fill_source_buffer(cm_Source *src, int offset, int length) {
  cm_Event e;
  e.type = CM_EVENT_SAMPLES;
  e.udata = src->udata;
  e.buffer = src->buffer + offset;
  e.length = length;
  src->handler(&e);
}

static void add_to_cb_queue(cm_Source *src) // LS
{
int i;
//printf("add\n");
    for (i=0;i<CM_MAX_CB_QUEUE;i++)
    {//printf("add %d\n",i);
	if (cb_queue[i] == NULL)
	{
	    cb_queue[i] = src;
		//printf("Added to cb_queue\n");
	    return;
	}
    }
    fprintf(stderr,"cmixer: too many callbacks at once!\n");
    return;
}

static void process_cb_queue() // LS
{
int i;
cm_Source *src;
    for (i=0;i<CM_MAX_CB_QUEUE;i++)
    {
	src = cb_queue[i];
	if (src)
	{
	    (*src->finished_cb)(src->channel); /* if set, call callback funciton(LS) */
	    cb_queue[i] = NULL;
	}
    }
    
    return;
}

static void cm_clear_cb_queue()
{
	int i;
    for (i=0;i<CM_MAX_CB_QUEUE;i++)
    {
		cb_queue[i] = NULL;
    }
    return;
}

static void process_source(cm_Source *src, int len) {
  int i, n, a, b, p;
  int frame, count;
  cm_Int32 *dst = cmixer.buffer;

  /* Do rewind if flag is set */
  if (src->rewind) {
    rewind_source(src);
  }

  /* Don't process if not playing */
  if (src->state != CM_STATE_PLAYING) {
    return;
  }

  /* Process audio */
  while (len > 0) {
    /* Get current position frame */
    frame = src->position >> FX_BITS;

    /* Fill buffer if required */
    if (frame + 3 >= src->nextfill) {
      fill_source_buffer(src, (src->nextfill*2) & BUFFER_MASK, BUFFER_SIZE/2);
      src->nextfill += BUFFER_SIZE / 4;
    }
    
    /* Handle fading (LS) */ 
	if (src->fade)
	{
		double teff = 2.0*(cmixer.time_function() - src->fade_t0)/src->fade_T -1.0;
		if (teff >= 1.0) 
		{
			src->fade = 0;
			cm_set_gain(src, src->gainf);
		}
		else
		{
			//printf("teff: %g\n",teff);
			cm_set_gain(src, (src->gain0-src->gainf)*0.5*(1.0-teff)+src->gainf); // crossfade constant voltage
			//cm_set_gain(src, (src->gain0-src->gainf)*sqrt(0.5*(1.0-teff))+src->gainf); // crossfade constant power
		}
		//printf("gain: %g\n",src->gain);
		if (src->gain < 0.0 || src->gain > 1.0) exit(EXIT_FAILURE);
		
	}
	

    /* Handle reaching the end of the playthrough */
    if (frame >= src->end) {
      /* As streams continiously fill the raw buffer in a loop we simply
      ** increment the end idx by one length and continue reading from it for
      ** another play-through */
      src->end = frame + src->length;
	  if (src->finished_cb) add_to_cb_queue(src);
      /* Set state and stop processing if we're not set to loop */
      if (!src->loop) {
        src->state = CM_STATE_STOPPED;
	
		//if (src->finished_cb) src->should_cb = 1;//(*src->finished_cb)(src->channel); /* if set, call callback funciton(LS) */
        break;
      }
    }

    /* Work out how many frames we should process in the loop */
    n = MIN(src->nextfill - 2, src->end) - frame;
    count = (n << FX_BITS) / src->rate;
    count = MAX(count, 1);
    count = MIN(count, len / 2);
    len -= count * 2;

    /* Add audio to master buffer */
    if (src->rate == FX_UNIT) {
      /* Add audio to buffer -- basic */
      n = frame * 2;
      for (i = 0; i < count; i++) {
		
		// (LS) get current sample:
		src->x0l = src->buffer[(n    ) & BUFFER_MASK];
		src->x0r = src->buffer[(n + 1) & BUFFER_MASK];
		
		// (LS) calculate current output:
		src->y0l = src->b0*(double)src->x0l + src->b1 * (double)src->xl[0] + src->b2 * (double)src->xl[1] - (src->a1*(double)src->yl[0] + src->a2*(double)src->yl[1]);
		src->y0r = src->b0*(double)src->x0r + src->b1 * (double)src->xr[0] + src->b2 * (double)src->xr[1] - (src->a1*(double)src->yr[0] + src->a2*(double)src->yr[1]);
		
		// (LS) update sample memory:
		src->yl[1] = src->yl[0];
		src->yr[1] = src->yr[0];
		src->yl[0] = src->y0l;
		src->yr[0] = src->y0r;
		src->xl[1] = src->xl[0];
		src->xr[1] = src->xr[0];
		src->xl[0] = src->x0l;
		src->xr[0] = src->x0r;
		
		// (LS) add to master buffer with gain:
        dst[0] += (src->y0l * src->lgain) >> FX_BITS;
        dst[1] += (src->y0r * src->rgain) >> FX_BITS;
		
        n += 2;
        dst += 2;
      }
      src->position += count * FX_UNIT;

    } else {
      /* Add audio to buffer -- interpolated */
      for (i = 0; i < count; i++) {
        n = (src->position >> FX_BITS) * 2;
        p = src->position & FX_MASK;
        
		// (LS) get current left sample:
		a = src->buffer[(n    ) & BUFFER_MASK];
        b = src->buffer[(n + 2) & BUFFER_MASK];
		src->x0l = FX_LERP(a, b, p);
        
        n++;
        
		// (LS) get current right sample:
		a = src->buffer[(n    ) & BUFFER_MASK];
        b = src->buffer[(n + 2) & BUFFER_MASK];
		src->x0r = FX_LERP(a, b, p);
        
		
		// (LS) calculate current output:
		src->y0l = src->b0*(double)src->x0l + src->b1 * (double)src->xl[0] + src->b2 * (double)src->xl[1] - (src->a1*(double)src->yl[0] + src->a2*(double)src->yl[1]);
		src->y0r = src->b0*(double)src->x0r + src->b1 * (double)src->xr[0] + src->b2 * (double)src->xr[1] - (src->a1*(double)src->yr[0] + src->a2*(double)src->yr[1]);
		
		// (LS) update sample memory:
		src->yl[1] = src->yl[0];
		src->yr[1] = src->yr[0];
		src->yl[0] = src->y0l;
		src->yr[0] = src->y0r;
		src->xl[1] = src->xl[0];
		src->xr[1] = src->xr[0];
		src->xl[0] = src->x0l;
		src->xr[0] = src->x0r;
		
		dst[0] += (src->y0l * src->lgain) >> FX_BITS;
		dst[1] += (src->y0r * src->rgain) >> FX_BITS;
		
        src->position += src->rate;
        dst += 2;
      }
    }

  }
}

void cm_set_iir(cm_Source *src, double b0, double b1, double b2, double a1, double a2) // (LS)
{
	src->b0 = b0;
	src->b1 = b1;
	src->b2 = b2;
	src->a1 = a1;
	src->a2 = a2;
	return;
}

void cm_process(cm_Int16 *dst, int len) {
  int i;
  cm_Source **s;

  /* Process in chunks of BUFFER_SIZE if `len` is larger than BUFFER_SIZE */
  while (len > BUFFER_SIZE) {
    cm_process(dst, BUFFER_SIZE);
    dst += BUFFER_SIZE;
    len -= BUFFER_SIZE;
  }

  /* Zeroset internal buffer */
  memset(cmixer.buffer, 0, len * sizeof(cmixer.buffer[0]));
  /* Zeroset callback queue (LS) */
  cm_clear_cb_queue();

  /* Process active sources */
  lock();
  s = &cmixer.sources;
  while (*s) {
    process_source(*s, len);
    /* Remove source from list if it is no longer playing */
    if ((*s)->state != CM_STATE_PLAYING) {
      (*s)->active = 0;
      *s = (*s)->next;
    } else {
      s = &(*s)->next;
    }
  }
  unlock();
  process_cb_queue();
  /* Copy internal buffer to destination and clip */
  for (i = 0; i < len; i+=2) {
	  
		// (LS) get current sample:
		cmixer.x0l = cmixer.buffer[i  ];
		cmixer.x0r = cmixer.buffer[i+1];
		
		// (LS) calculate current output:
		cmixer.y0l = cmixer.b0*(double)cmixer.x0l + cmixer.b1 * (double)cmixer.xl[0] + cmixer.b2 * (double)cmixer.xl[1] - (cmixer.a1*(double)cmixer.yl[0] + cmixer.a2*(double)cmixer.yl[1]);
		cmixer.y0r = cmixer.b0*(double)cmixer.x0r + cmixer.b1 * (double)cmixer.xr[0] + cmixer.b2 * (double)cmixer.xr[1] - (cmixer.a1*(double)cmixer.yr[0] + cmixer.a2*(double)cmixer.yr[1]);
		
		// (LS) update sample memory:
		cmixer.yl[1] = cmixer.yl[0];
		cmixer.yr[1] = cmixer.yr[0];
		cmixer.yl[0] = cmixer.y0l;
		cmixer.yr[0] = cmixer.y0r;
		cmixer.xl[1] = cmixer.xl[0];
		cmixer.xr[1] = cmixer.xr[0];
		cmixer.xl[0] = cmixer.x0l;
		cmixer.xr[0] = cmixer.x0r;
		
		// (LS) add to master buffer with gain:
        int yl = (cmixer.y0l * cmixer.gain) >> FX_BITS;
        int yr = (cmixer.y0r * cmixer.gain) >> FX_BITS;
		
		dst[i  ] = CLAMP(yl, -32768, 32767);
		dst[i+1] = CLAMP(yr, -32768, 32767);

  }
}

void cm_set_master_iir(double b0, double b1, double b2, double a1, double a2) // (LS)
{
	cmixer.b0 = b0;
	cmixer.b1 = b1;
	cmixer.b2 = b2;
	cmixer.a1 = a1;
	cmixer.a2 = a2;
	return;
}


cm_Source* cm_new_source(const cm_SourceInfo *info) {
  cm_Source *src = calloc(1, sizeof(*src));
  if (!src) {
    error("allocation failed");
    return NULL;
  }
  src->handler = info->handler;
  src->length = info->length;
  src->samplerate = info->samplerate;
  src->udata = info->udata;
  
  // (LS) initialize IIR buffers
  src->xl[0] = 0;
  src->xl[1] = 0;
  src->yl[0] = 0;
  src->yl[1] = 0;
  src->xr[0] = 0;
  src->xr[1] = 0;
  src->yr[0] = 0;
  src->yr[1] = 0;
  src->x0l = 0;
  src->x0r = 0;
  src->y0l = 0;
  src->y0r = 0;
  
  // (LS) initialize IIR coefficients
  src->a1 = 0.0;
  src->a2 = 0.0;
  src->b0 = 1.0;
  src->b1 = 0.0;
  src->b2 = 0.0; 
  
  cm_set_pan(src, 0);
  cm_set_pitch(src, 1);
  cm_set_loop(src, 0);
  cm_stop(src);
  return src;
}


static const char* wav_init(cm_SourceInfo *info, void *data, int len, int ownsdata);

#ifdef CM_USE_STB_VORBIS
static const char* ogg_init(cm_SourceInfo *info, void *data, int len, int ownsdata);
#endif


static int check_header(void *data, int size, char *str, int offset) {
  int len = strlen(str);
  return (size >= offset + len) && !memcmp((char*) data + offset, str, len);
}


static cm_Source* new_source_from_mem(void *data, int size, int ownsdata) {
  const char *err;
  cm_SourceInfo info;

  if (check_header(data, size, "WAVE", 8)) {
    err = wav_init(&info, data, size, ownsdata);
    if (err) {
      return NULL;
    }
    return cm_new_source(&info);
  }

#ifdef CM_USE_STB_VORBIS
  if (check_header(data, size, "OggS", 0)) {
    err = ogg_init(&info, data, size, ownsdata);
    if (err) {
      return NULL;
    }
    return cm_new_source(&info);
  }
#endif

  error("unknown format or invalid data");
  return NULL;
}


static void* load_file(const char *filename, int *size) {
  FILE *fp;
  void *data;
  int n;

  fp = fopen(filename, "rb");
  if (!fp) {
    return NULL;
  }

  /* Get size */
  fseek(fp, 0, SEEK_END);
  *size = ftell(fp);
  rewind(fp);

  /* Malloc, read and return data */
  data = malloc(*size);
  if (!data) {
    fclose(fp);
    return NULL;
  }
  n = fread(data, 1, *size, fp);
  fclose(fp);
  if (n != *size) {
    free(data);
    return NULL;
  }

  return data;
}


cm_Source* cm_new_source_from_file(const char *filename) {
  int size;
  cm_Source *src;
  void *data;

  /* Load file into memory */
  data = load_file(filename, &size);
  if (!data) {
    error("could not load file");
    return NULL;
  }

  /* Try to load and return */
  src = new_source_from_mem(data, size, 1);
  if (!src) {
    free(data);
    return NULL;
  }

  return src;
}


cm_Source* cm_new_source_from_mem(void *data, int size) {
  return new_source_from_mem(data, size, 0);
}


void cm_destroy_source(cm_Source *src) {
  cm_Event e;
  lock();
  if (src->active) {
    cm_Source **s = &cmixer.sources;
    while (*s) {
		//printf("Destroyed source\n");
      if (*s == src) {
        *s = src->next;
        break;
      }
      s = &((*s)->next); // (LS) fixed infinity loop
    }
  }
  unlock();
  e.type = CM_EVENT_DESTROY;
  e.udata = src->udata;
  src->handler(&e);
  free(src);
}


double cm_get_length(cm_Source *src) {
  return src->length / (double) src->samplerate;
}


double cm_get_position(cm_Source *src) {
  return ((src->position >> FX_BITS) % src->length) / (double) src->samplerate;
}


int cm_get_state(cm_Source *src) {
  return src->state;
}


static void recalc_source_gains(cm_Source *src) {
  double l, r;
  double pan = src->pan;
  l = src->gain * (pan <= 0. ? 1. : 1. - pan);
  r = src->gain * (pan >= 0. ? 1. : 1. + pan);
  src->lgain = FX_FROM_FLOAT(l);
  src->rgain = FX_FROM_FLOAT(r);
}


void cm_set_gain(cm_Source *src, double gain) {
  src->gain = gain;
  recalc_source_gains(src);
}


void cm_set_pan(cm_Source *src, double pan) {
  src->pan = CLAMP(pan, -1.0, 1.0);
  recalc_source_gains(src);
}


void cm_set_pitch(cm_Source *src, double pitch) {
  double rate;
  if (pitch > 0.) {
    rate = src->samplerate / (double) cmixer.samplerate * pitch;
  } else {
    rate = 0.001;
  }
  src->rate = FX_FROM_FLOAT(rate);
}


void cm_set_loop(cm_Source *src, int loop) {
  src->loop = loop;
}


void cm_play(cm_Source *src) {
  lock();
  src->state = CM_STATE_PLAYING;
  if (!src->active) {
    src->active = 1;
    src->next = cmixer.sources;
    cmixer.sources = src;
  }
  unlock();
}


void cm_pause(cm_Source *src) {
  src->state = CM_STATE_PAUSED;
}


void cm_stop(cm_Source *src) {
  src->state = CM_STATE_STOPPED;
  src->rewind = 1;
}


/*============================================================================
** Wav stream
**============================================================================*/

typedef struct {
  void *data;
  int bitdepth;
  int samplerate;
  int channels;
  int length;
} Wav;

typedef struct {
  Wav wav;
  void *data;
  int idx;
} WavStream;


static char* find_subchunk(char *data, int len, char *id, int *size) {
  /* TODO : Error handling on malformed wav file */
  int idlen = strlen(id);
  char *p = data + 12;
next:
  *size = *((cm_UInt32*) (p + 4));
  if (memcmp(p, id, idlen)) {
    p += 8 + *size;
    if (p > data + len) return NULL;
    goto next;
  }
  return p + 8;
}


static const char* read_wav(Wav *w, void *data, int len) {
  int bitdepth, channels, samplerate, format;
  int sz;
  char *p = data;
  memset(w, 0, sizeof(*w));

  /* Check header */
  if (memcmp(p, "RIFF", 4) || memcmp(p + 8, "WAVE", 4)) {
    return error("bad wav header");
  }
  /* Find fmt subchunk */
  p = find_subchunk(data, len, "fmt", &sz);
  if (!p) {
    return error("no fmt subchunk");
  }

  /* Load fmt info */
  format      = *((cm_UInt16*) (p));
  channels    = *((cm_UInt16*) (p + 2));
  samplerate  = *((cm_UInt32*) (p + 4));
  bitdepth    = *((cm_UInt16*) (p + 14));
  if (format != 1) {
    return error("unsupported format");
  }
  if (channels == 0 || samplerate == 0 || bitdepth == 0) {
    return error("bad format");
  }

  /* Find data subchunk */
  p = find_subchunk(data, len, "data", &sz);
  if (!p) {
    return error("no data subchunk");
  }

  /* Init struct */
  w->data = (void*) p;
  w->samplerate = samplerate;
  w->channels = channels;
  w->length = (sz / (bitdepth / 8)) / channels;
  w->bitdepth = bitdepth;
  /* Done */
  return NULL;
}


#define WAV_PROCESS_LOOP(X) \
  while (n--) {             \
    X                       \
    dst += 2;               \
    s->idx++;               \
  }

static void wav_handler(cm_Event *e) {
  int x, n;
  cm_Int16 *dst;
  WavStream *s = e->udata;
  int len;

  switch (e->type) {

    case CM_EVENT_DESTROY:
      free(s->data);
      free(s);
      break;

    case CM_EVENT_SAMPLES:
      dst = e->buffer;
      len = e->length / 2;
fill:
      n = MIN(len, s->wav.length - s->idx);
      len -= n;
      if (s->wav.bitdepth == 16 && s->wav.channels == 1) {
        WAV_PROCESS_LOOP({
          dst[0] = dst[1] = ((cm_Int16*) s->wav.data)[s->idx];
        });
      } else if (s->wav.bitdepth == 16 && s->wav.channels == 2) {
        WAV_PROCESS_LOOP({
          x = s->idx * 2;
          dst[0] = ((cm_Int16*) s->wav.data)[x    ];
          dst[1] = ((cm_Int16*) s->wav.data)[x + 1];
        });
      } else if (s->wav.bitdepth == 8 && s->wav.channels == 1) {
        WAV_PROCESS_LOOP({
          dst[0] = dst[1] = (((cm_UInt8*) s->wav.data)[s->idx] - 128) << 8;
        });
      } else if (s->wav.bitdepth == 8 && s->wav.channels == 2) {
        WAV_PROCESS_LOOP({
          x = s->idx * 2;
          dst[0] = (((cm_UInt8*) s->wav.data)[x    ] - 128) << 8;
          dst[1] = (((cm_UInt8*) s->wav.data)[x + 1] - 128) << 8;
        });
      }
      /* Loop back and continue filling buffer if we didn't fill the buffer */
      if (len > 0) {
        s->idx = 0;
        goto fill;
      }
      break;

    case CM_EVENT_REWIND:
      s->idx = 0;
      break;
  }
}


static const char* wav_init(cm_SourceInfo *info, void *data, int len, int ownsdata) {
  WavStream *stream;
  Wav wav;

  const char *err = read_wav(&wav, data, len);
  if (err != NULL) {
    return err;
  }

  if (wav.channels > 2 || (wav.bitdepth != 16 && wav.bitdepth != 8)) {
    return error("unsupported wav format");
  }

  stream = calloc(1, sizeof(*stream));
  if (!stream) {
    return error("allocation failed");
  }
  stream->wav = wav;

  if (ownsdata) {
    stream->data = data;
  }
  stream->idx = 0;

  info->udata = stream;
  info->handler = wav_handler;
  info->samplerate = wav.samplerate;
  info->length = wav.length;

  /* Return NULL (no error) for success */
  return NULL;
}


/*============================================================================
** Ogg stream
**============================================================================*/

#ifdef CM_USE_STB_VORBIS

#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

typedef struct {
  stb_vorbis *ogg;
  void *data;
} OggStream;


static void ogg_handler(cm_Event *e) {
  int n, len;
  OggStream *s = e->udata;
  cm_Int16 *buf;

  switch (e->type) {

    case CM_EVENT_DESTROY:
      stb_vorbis_close(s->ogg);
      free(s->data);
      free(s);
      break;

    case CM_EVENT_SAMPLES:
      len = e->length;
      buf = e->buffer;
fill:
      n = stb_vorbis_get_samples_short_interleaved(s->ogg, 2, buf, len);
      n *= 2;
      /* rewind and fill remaining buffer if we reached the end of the ogg
      ** before filling it */
      if (len != n) {
        stb_vorbis_seek_start(s->ogg);
        buf += n;
        len -= n;
        goto fill;
      }
      break;

    case CM_EVENT_REWIND:
      stb_vorbis_seek_start(s->ogg);
      break;
  }
}


static const char* ogg_init(cm_SourceInfo *info, void *data, int len, int ownsdata) {
  OggStream *stream;
  stb_vorbis *ogg;
  stb_vorbis_info ogginfo;
  int err;

  ogg = stb_vorbis_open_memory(data, len, &err, NULL);
  if (!ogg) {
	  fprintf(stderr,"stb_vorbis errno: %d\n",err);
    return error("invalid ogg data");
  }

  stream = calloc(1, sizeof(*stream));
  if (!stream) {
    stb_vorbis_close(ogg);
    return error("allocation failed");
  }

  stream->ogg = ogg;
  if (ownsdata) {
    stream->data = data;
  }

  ogginfo = stb_vorbis_get_info(ogg);

  info->udata = stream;
  info->handler = ogg_handler;
  info->samplerate = ogginfo.sample_rate;
  info->length = stb_vorbis_stream_length_in_samples(ogg);

  /* Return NULL (no error) for success */
  return NULL;
}


#endif
