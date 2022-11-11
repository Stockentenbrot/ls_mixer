/*    
 *    Part of ls_mixer
 *    Copyright (c) 2021-2022 Laurin Schnorr (laurin point schnorr at online point de)
 *    
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 2 of the License, or
 *    (at your option) any later version.
 * 
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ls_mixer.h"
#include "iir.h"

struct ls_mixer_channel channel[LS_MIXER_NCHANNEL];

static SDL_AudioDeviceID dev;

static SDL_mutex* audio_mutex;

static uint16_t fs; // sample frequency [Hz]

static void lock_handler(cm_Event *e) {
  if (e->type == CM_EVENT_LOCK) {
    SDL_LockMutex(audio_mutex);
  }
  if (e->type == CM_EVENT_UNLOCK) {
    SDL_UnlockMutex(audio_mutex);
  }
}


static void audio_callback(void *udata, Uint8 *stream, int size) {
  cm_process((void*) stream, size / 2);
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
	fprintf(stderr,"Could not load file %s\n",filename);
	exit(EXIT_FAILURE);
  }

  return data;
}

double ls_mixer_time(void)
{
		return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

void ls_mixer_init(uint16_t freq,uint16_t samples)
{
  
  SDL_AudioSpec fmt, got;
  
  fs = freq;

  /* Init SDL */
  SDL_Init(SDL_INIT_AUDIO);
  audio_mutex = SDL_CreateMutex();

  /* Init SDL audio */
  memset(&fmt, 0, sizeof(fmt));
  fmt.freq      = freq;
  fmt.format    = AUDIO_S16;
  fmt.channels  = 2;
  fmt.samples   = samples;
  fmt.callback  = audio_callback;

  dev = SDL_OpenAudioDevice(NULL, 0, &fmt, &got, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (dev == 0) {
    fprintf(stderr, "Error: failed to open audio device '%s'\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  /* Init library */
  cm_init(got.freq);
  cm_set_lock(lock_handler);
  cm_set_time_function(ls_mixer_time);
  cm_set_master_gain(0.5);

  /* Start audio */
  SDL_PauseAudioDevice(dev, 0);
  
  int i;
  for (i=0; i < LS_MIXER_NCHANNEL; i++)
  {
	  channel[i].src = NULL;
  }
  return;
}

void ls_mixer_close()
{
	int i;
	for (i=0; i < LS_MIXER_NCHANNEL; i++)
  {
	  if (channel[i].src != NULL)
	  {
		  cm_destroy_source(channel[i].src);
		  channel[i].src = NULL;
	  }
  }
	SDL_CloseAudioDevice(dev);
	return;
}


int ls_mixer_find_free_channel()
{
	int i;
	for (i=0; i < LS_MIXER_NCHANNEL; i++)
	{
		if (channel[i].src == NULL) return i;
		if (cm_get_state(channel[i].src) == CM_STATE_STOPPED)
		{
			cm_destroy_source(channel[i].src);
			channel[i].src = NULL;
			return i;
		}
	}
	fprintf(stderr,"ls_mixer: No free channels available!");
	return -1;
}


ls_mixer_sounddata *ls_mixer_load(const char *filename)
{
	struct ls_mixer_sounddata *load;
	load = malloc(sizeof(struct ls_mixer_sounddata));
	load->filename = strdup(filename);
	load->data = load_file(filename, &load->size);
	return load;
}

void ls_mixer_delete(ls_mixer_sounddata *sound)
{
	int i;
	for (i=0; i < LS_MIXER_NCHANNEL; i++) // destroy all sources that use the deleted data
	{
		if (channel[i].data == sound->data) 
		{
			cm_destroy_source(channel[i].src);
			channel[i].src = NULL;
			channel[i].data = NULL; // freed later via other reference
		}
	}
	sound->size = 0;
	free(sound->data);
	free(sound->filename);
	free(sound);
	return;
}

double ls_mixer_get_position(int chan)
{
    return cm_get_position(channel[chan].src);
}

void ls_mixer_set_gain(int chan,double gain)
{
	if (chan == -1) cm_set_master_gain(gain);
	else cm_set_gain(channel[chan].src, gain);
	return;
}

void ls_mixer_set_iir(int chan, double b0, double b1, double b2, double a1, double a2)
{
	if (chan == -1) cm_set_master_iir(b0, b1, b2, a1, a2);
	else cm_set_iir(channel[chan].src, b0, b1, b2, a1, a2);
	return;
}


/* old versions that don't work as well as the Butterworth filters
void ls_mixer_set_lowpassRC(int chan, double cutoff) // calculate IIR coefficients for a simple RC-lowpass
{
	double Ts = 1.0/fs;
	double A = Ts/(Ts + 1.0/(2.0*3.14159265358979323846*cutoff));
	ls_mixer_set_iir(chan,A,0.0,0.0,A-1.0,0.0);
	return;
}

void ls_mixer_set_highpassRC(int chan, double cutoff) // calculate IIR coefficients for a simple RC-highpass
{
	double Ts = 1.0/fs;
	double B = 1.0/(1.0 + 2.0*3.14159265358979323846*cutoff*Ts);
	ls_mixer_set_iir(chan,B,-B,0.0,-B,0.0);
	return;
}
*/

/*
void ls_mixer_set_bandpass(int chan, double f1, double f2) // calculate IIR coefficients for a simple RC-bandpass
{
	double Ts = 1.0/fs;
	double A = Ts/(Ts + 1.0/(2.0*3.14159265358979323846*f2));
	double B = 1.0/(1.0 + 2.0*3.14159265358979323846*f1*Ts);
	ls_mixer_set_iir(chan,A*B,-A*B,0.0,A-1.0-B,B-A*B);
	return;
}
*/

void ls_mixer_set_lowpass(int chan, int n, double fc) // calculate IIR coefficients for a Butterworth lowpass of order n <= 2
{
	double *dcof;     // d coefficients =^ a
	int *ccof;        // c coefficients =^ b
	double sf;        // scaling factor
	double fcf = 2.0*fc/(double)fs;        // cutoff frequency (fraction of pi)
	
 	if (fcf < 0.001) fcf = 0.001;
	if (fcf >= 1.0) fcf = 0.999;
	if (n > 2) n = 2;
	if (n < 1) n = 1;
    
    /* calculate the d coefficients */
	dcof = dcof_bwlp( n, fcf );
	/* calculate the c coefficients */
    ccof = ccof_bwlp( n );
	sf = sf_bwlp( n, fcf ); /* scaling factor for the c coefficients */
	ls_mixer_set_iir(chan,(double)ccof[0]*sf,(double)ccof[1]*sf,(double)ccof[2]*sf,dcof[1],dcof[2]);
	free( dcof );
    free( ccof );
	return;
}

void ls_mixer_set_highpass(int chan, int n, double fc) // calculate IIR coefficients for a Butterworth highpass of order n <= 2
{
	double *dcof;     // d coefficients =^ a
	int *ccof;        // c coefficients =^ b
	double sf;        // scaling factor
	double fcf = 2.0*fc/(double)fs;        // cutoff frequency (fraction of pi)
	
	if (fcf < 0.002) fcf = 0.002;
	if (fcf >= 1.0) fcf = 0.999;
	if (n > 2) n = 2;
	if (n < 1) n = 1;
    
    /* calculate the d coefficients */
	dcof = dcof_bwhp( n, fcf );
	/* calculate the c coefficients */
    ccof = ccof_bwhp( n );
	sf = sf_bwhp( n, fcf ); /* scaling factor for the c coefficients */
	ls_mixer_set_iir(chan,(double)ccof[0]*sf,(double)ccof[1]*sf,(double)ccof[2]*sf,dcof[1],dcof[2]);
	free( dcof );
    free( ccof );
	return;
}

void ls_mixer_set_bandpass(int chan, double f1, double f2) // calculate IIR coefficients for a first order Butterworth bandpass
{
	double *dcof;     // d coefficients =^ a
	int *ccof;        // c coefficients =^ b
	double sf;        // scaling factor
	double f1f = 2.0*f1/(double)fs;       // lower cutoff frequency (fraction of pi)
    double f2f = 2.0*f2/(double)fs;       // upper cutoff frequency (fraction of pi)
    
	if (f1f < 0.0) f1f = 0.0;
	if (f1f >= 1.0) f1f = 0.999;
	if (f2f < 0.0) f2f = 0.0;
	if (f2f >= 1.0) f2f = 0.999;
	
    /* calculate the d coefficients */
    dcof = dcof_bwbp( 1, f1f, f2f );
	/* calculate the c coefficients */
    ccof = ccof_bwbp(1);
	sf = sf_bwbp(1, f1f, f2f ); /* scaling factor for the c coefficients */
	ls_mixer_set_iir(chan,(double)ccof[0]*sf,(double)ccof[1]*sf,(double)ccof[2]*sf,dcof[1],dcof[2]);
	free( dcof );
    free( ccof );
	return;
}

void ls_mixer_set_bandstop(int chan, double f1, double f2) // calculate IIR coefficients for a first order Butterworth bandstop
{
	double *dcof;     // d coefficients =^ a
	double *ccof;        // c coefficients =^ b
	double sf;        // scaling factor
	double f1f = 2.0*f1/(double)fs;       // lower cutoff frequency (fraction of pi)
    double f2f = 2.0*f2/(double)fs;       // upper cutoff frequency (fraction of pi)
		
	if (f1f < 0.0) f1f = 0.0;
	if (f1f >= 1.0) f1f = 0.999;
	if (f2f < 0.002) f2f = 0.002;
	if (f2f >= 1.0) f2f = 0.999;
    
    /* calculate the d coefficients */
    dcof = dcof_bwbs( 1, f1f, f2f );
	/* calculate the c coefficients */
    ccof = ccof_bwbs( 1, f1f, f2f );
	sf = sf_bwbs(1, f1f, f2f ); /* scaling factor for the c coefficients */
	ls_mixer_set_iir(chan,ccof[0]*sf,ccof[1]*sf,ccof[2]*sf,dcof[1],dcof[2]);
	free( dcof );
    free( ccof );
	return;
}



void ls_mixer_set_pitch(int chan,double pitch)
{
	if (chan >= 0) cm_set_pitch(channel[chan].src, pitch);
	return;
}

void ls_mixer_set_pan(int chan,double pan)
{
	if (chan >= 0) cm_set_pan(channel[chan].src, pan);
	return;
}

void ls_mixer_stop(int chan) // TODO: all channels/master channel
{
	cm_stop(channel[chan].src);
	return;
}

void ls_mixer_pause(int chan) // TODO: all channels/master channel
{
	cm_pause(channel[chan].src);
	return;
}

void ls_mixer_resume(int chan) // TODO: all channels/master channel
{
	cm_play(channel[chan].src);
	return;
}

int ls_mixer_play(ls_mixer_sounddata *sound,int loop, double gain, double pan, double pitch)
{
	cm_Source *src;
	src = cm_new_source_from_mem(sound->data, sound->size);
	cm_set_loop(src, loop);
	cm_set_pitch(src, pitch);
	cm_set_gain(src, gain);
	cm_set_pan(src, pan);
	int channel_i = ls_mixer_find_free_channel();
	if (channel_i < 0)
	{
		fprintf(stderr,"ls_mixer: No free channels available for sound \"%s\"!",sound->filename);
		cm_destroy_source(src);
		return -1;
	}
	src->channel = channel_i;
	cm_play(src);
	channel[channel_i].src = src;
	channel[channel_i].data = sound->data;
	//printf("Playing sound \"%s\" on channel %d...\n",sound->filename,channel_i);
	//printf("Länge: %g s\n",src->)
	return channel_i;
}

void ls_mixer_set_finished_cb_channel(int chan, void (*cb)(int))
{
	if (channel[chan].src) channel[chan].src->finished_cb = cb;
	else 
	{
		fprintf(stderr,"Should set callback for empty channel %d!\n",chan);
		exit(EXIT_FAILURE);
	}
	return;
}

void ls_mixer_set_finished_cb_all(void (*cb)(int))
{
	int i;
	for (i=0; i < LS_MIXER_NCHANNEL; i++)
	{
		if (channel[i].src == NULL) continue;
		channel[i].src->finished_cb = cb;
	}
	
	return;
}


void ls_mixer_fade(int chan,double T,double gainf)
{
	cm_Source *src = channel[chan].src;
	if (!src)
	{
		fprintf(stderr,"ls_mixer_fade: No sound playing on channel %d!\n",chan);
		return;
	}
	
	//src->fade = 1;
	src->gain0 = src->gain;
	if (src->gain > gainf) src->fade = -1;
	else src->fade = 1;
	src->gainf = gainf;
	src->fade_t0 = ls_mixer_time();
	src->fade_T = T;
	return;
}
