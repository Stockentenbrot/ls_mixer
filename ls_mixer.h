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
#ifndef LSMIXER_H
#define LSMIXER_H


/**
 * \brief Maximum number of channels
 * 
 * Number of maximum audio channels that can be played simultaneously
 */
#define LS_MIXER_NCHANNEL 32

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <SDL2/SDL.h>

#include "cmixer.h"



struct ls_mixer_channel
{
	cm_Source *src;
	void *data;
};

struct ls_mixer_sounddata
{
	void *data;
	int size;
	char *filename;
};

/**
 * \brief Data structure for sound data
 */
typedef struct ls_mixer_sounddata ls_mixer_sounddata;

/**
 * \brief Initialize the library.
 *
 * Initializes the library and must be called before any other function.
 *
 * \param freq The audio sample frequency in Hertz.
 * \param samples The number of samples in the internal buffer. Smaller numbers give lower latency but higher CPU load.
 *
 */
void ls_mixer_init(uint16_t freq,uint16_t samples);

/**
 * \brief Closes the library.
 *
 * Closes the library, as well as the SDL audio device.
 */
void ls_mixer_close();

/**
 * \brief Finds a free channel.
 *
 * Returns the index of the next best free audio channel.
 * \return The index of a free channel.
 */
int ls_mixer_find_free_channel();

/**
 * \brief Loads an audio file into memory.
 *
 * Loads an audio file into memory. The file format can be either .ogg or .wav. 
 * OGG files are decoded on the fly and are only stored in memory in encoded form.
 * 
 * \param filename The path to the file that is to be loaded.
 * 
 * \return The loaded sound data.
 */
ls_mixer_sounddata *ls_mixer_load(const char *filename);

/**
 * \brief Delete sound data from memory.
 *
 * Deletes sound data from memory.
 * 
 * \param sound The sound data to be deleted. All channels that are using this sound data are stopped.
 */
void ls_mixer_delete(ls_mixer_sounddata *sound);



/**
 * \brief Plays a sound.
 *
 * Plays a sound on the next available free channel.
 * 
 * \param sound A sound loaded via ls_mixer_load()
 * \param loop Whether the sound should be looped (1) or not (0)
 * \param gain Playback gain of the sound (1.0 = original, 2.0 = twice the amplitude, 0.0 = silent ...)
 * \param pan The stereo position (0.0 = center 1.0 = full right, -1.0 = full left)
 * \param pitch The playback speed like on a turntable (1.0 = original speed, 2.0 = twice as fast, 0.5 = half speed ...)
 * 
 * \return The index of the channel the sound is playing on.
 */
int ls_mixer_play(ls_mixer_sounddata *sound,int loop, double gain, double pan, double pitch);

/**
 * \brief Pauses a channel.

 * \param chan The index as returned by ls_mixer_play()
 */
void ls_mixer_pause(int chan);
/**
 * \brief Resumes playback on a channel.

 * \param chan The index as returned by ls_mixer_play()
 */
void ls_mixer_resume(int chan);
/**
 * \brief Stops playback on a channel.

 * \param chan The index as returned by ls_mixer_play()
 */
void ls_mixer_stop(int chan);

/**
 * \brief Gets playback position of a channel.
 * Returns the current playback position in seconds of a sound playing on a channel
 * \param chan The index as returned by ls_mixer_play()
 * \return The playback position in seconds.
 */
double ls_mixer_get_position(int chan);

/**
 * \brief Register callback function for a channel.
 * Register a callback function to be called when the playback on this channel has stopped.
 * \param chan The index as returned by ls_mixer_play()
 * \param cb The callback function which is called with the channel index as integer argument
 */
void ls_mixer_set_finished_cb_channel(int chan, void (*cb)(int));

/**
 * \brief Register callback function for all channels.
 * Register a callback function to all channels to be called when the playback on any channel has stopped.
 * \param cb The callback function which is called with the index of an expired channel as integer argument
 */
void ls_mixer_set_finished_cb_all(void (*cb)(int));

/**
 * \brief Fades a channel.
 * Automatically fades a channel from the current gain value to a given final value over a specified time.
 * \param chan The index as returned by ls_mixer_play()
 * \param T The time in seconds for the fading process
 * \param gainf The final gain, can be higher or lower than the current gain.
 */
void ls_mixer_fade(int chan,double T,double gainf);

/**
 * \brief Sets the gain of a channel.
 * \param chan The index as returned by ls_mixer_play()
 * \param gain The playback gain (1.0 = original, 2.0 = twice the amplitude, 0.0 = silent ...)
 */
void ls_mixer_set_gain(int chan,double gain);

/**
 * \brief Sets the panning of a channel.
 * \param chan The index as returned by ls_mixer_play()
 * \param pan The panning (0.0 = center 1.0 = full right, -1.0 = full left)
 */
void ls_mixer_set_pan(int chan,double pan);

/**
 * \brief Sets the pitch of a channel.
 * \param chan The index as returned by ls_mixer_play()
 * \param pitch The pitch (1.0 = original speed, 2.0 = twice as fast, 0.5 = half speed ...)
 */
void ls_mixer_set_pitch(int chan,double pitch);


/**
 * \brief Sets the IIR filter coefficients of a channel.
 * 
 * Setting b0=1.0 and all other coefficients to 0.0 removes any filtering.
 * 
 * \param chan The index as returned by ls_mixer_play()
 * \param b0 Feedforward filter coefficient
 * \param b1 Feedforward filter coefficient
 * \param b2 Feedforward filter coefficient
 * \param a1 Feedback filter coefficient
 * \param a2 Feedback filter coefficient
 */
void ls_mixer_set_iir(int chan, double b0, double b1, double b2, double a1, double a2);

/* old versions that don't work as well as the Butterworth filters
void ls_mixer_set_lowpassRC(int chan, double cutoff); // calculate IIR coefficients for a simple RC-lowpass
void ls_mixer_set_highpassRC(int chan, double cutoff); // calculate IIR coefficients for a simple RC-highpass
*/

/**
 * \brief Sets IIR filter coefficients of a channel to lowpass.
 * 
 * Sets the IIR filter coefficients of a channel to a Butterworth lowpass filter of order one or two
 * 
 * \param chan The index as returned by ls_mixer_play()
 * \param n Filter order (1 or 2)
 * \param fc Cut off frequency in Hz
 */
void ls_mixer_set_lowpass(int chan, int n, double fc); // calculate IIR coefficients for a Butterworth lowpass of order n <= 2

/**
 * \brief Sets IIR filter coefficients of a channel to highpass.
 * 
 * Sets the IIR filter coefficients of a channel to a Butterworth highpass filter of order one or two
 * 
 * \param chan The index as returned by ls_mixer_play()
 * \param n Filter order (1 or 2)
 * \param fc Cut off frequency in Hz
 */
void ls_mixer_set_highpass(int chan, int n, double fc); // calculate IIR coefficients for a Butterworth highpass of order n <= 2

/**
 * \brief Sets IIR filter coefficients of a channel to bandpass.
 * 
 * Sets the IIR filter coefficients of a channel to a first order Butterworth bandpass filter
 * 
 * \param chan The index as returned by ls_mixer_play()
 * \param f1 Low frequency border of pass band in Hz
 * \param f2 High frequency border of pass band in Hz
 */
void ls_mixer_set_bandpass(int chan, double f1, double f2); // calculate IIR coefficients for a first order Butterworth bandpass

/**
 * \brief Sets IIR filter coefficients of a channel to bandstop.
 * 
 * Sets the IIR filter coefficients of a channel to a first order Butterworth bandstop filter
 * 
 * \param chan The index as returned by ls_mixer_play()
 * \param f1 Low frequency border of stop band in Hz
 * \param f2 High frequency border of stop band in Hz
 */
void ls_mixer_set_bandstop(int chan, double f1, double f2); // calculate IIR coefficients for a first order Butterworth bandstop

#endif
