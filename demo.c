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
#include <stdlib.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "ls_mixer.h"

#define wait() printf("Press [Return] to continue...\n");getchar()

int main()
{
	ls_mixer_init(44100,441); // initialize the sound system with a sample rate of 44100Hz and a buffer size of 441 samples
	
	
	
	
	printf("Let's start by playing a music file in the Ogg/Vorbis format.\nThe file is loaded into memory and decoded on the fly. This is true for every .ogg file you load, there is no distinction between music and sounds.\nThe audio will be looped automatically.\n");
	wait();
	
	printf("The song is \"Blue Ska\" by Kevin MacLeod, released under the Creative Commons BY 3.0 license, see https://creativecommons.org/licenses/by/3.0/\n");
	ls_mixer_sounddata *music;
	music = ls_mixer_load("audio/Blue_Ska_(ISRC_USUAN1600011).ogg"); // load a sound, file format can be either Ogg/Vorbis or Wave-Audio
	
	int loop = 1; // 0 means play the sound only once, 1 means continous looping
	double pan = 0.0; // 0 is center, -1 is full left, +1 is full right
	double pitch = 1.0; // 1 is original speed, 2 is twice as fast (double pitch), 0.5 is half as fast ...
	double gain = 1.0; // 1 is original gain, 2 is twice as loud, 0.5 is half as loud ...
	
	double f1,f2;
	
	int music_channel = ls_mixer_play(music,loop,gain,pan,pitch); // start playing a loaded sound with the parameters given as arguments, returns the mixing channel that has been assigned to the sound
	wait();
	
	printf("We can pan the audio around by calling ls_mixer_set_pan():\n");
	wait();
	
	for (pan = 0.0; pan <= 1.0; pan +=0.01)
	{
		printf("Pan: %.3f\n",pan);
		ls_mixer_set_pan(music_channel,pan); // update the panning of a channel while playing a sound
		SDL_Delay(30); // Sleep for 30 ms
	}
	printf("Full right...\n");
	wait();
	for (pan = 1.0; pan >= -1.0; pan -=0.01)
	{
		printf("Pan: %.3f\n",pan);
		ls_mixer_set_pan(music_channel,pan); // update the panning of a channel while playing a sound
		SDL_Delay(30);
	}
	printf("Full left...\n");
	wait();
	for (pan = -1.0; pan < 0.0; pan +=0.01)
	{
		printf("Pan: %.3f\n",pan);
		ls_mixer_set_pan(music_channel,pan); // update the panning of a channel while playing a sound
		SDL_Delay(30);
	}
	
	pan = 0.0;
	printf("Pan: %.3f\n",pan);
	ls_mixer_set_pan(music_channel,pan); // reset the pan to center
	printf("Neutral\n");
	wait();
	
	printf("\nWe can change the pitch by calling ls_mixer_set_pitch(). This affects both speed and frequency, like on a turntable:\n");
	wait();
	
	for (pitch = 1.0; pitch < 2.0; pitch +=0.01)
	{
		printf("Pitch: %.3f\n",pitch);
		ls_mixer_set_pitch(music_channel,pitch); // update the pitch of a channel while playing a sound
		SDL_Delay(30);
	}
	printf("Playback twice as fast...\n");
	wait();
	
	for (pitch = 2.0; pitch > 0.5; pitch -=0.01)
	{
		printf("Pitch: %.3f\n",pitch);
		ls_mixer_set_pitch(music_channel,pitch); // update the pitch of a channel while playing a sound
		SDL_Delay(30);
	}
	printf("Playback with half speed...\n");
	wait();
	
	for (pitch = 0.5; pitch < 1.0; pitch +=0.01)
	{
		printf("Pitch: %.3f\n",pitch);
		ls_mixer_set_pitch(music_channel,pitch); // update the pitch of a channel while playing a sound
		SDL_Delay(30);
	}
	
	pitch = 1.0;
	printf("Pitch: %.3f\n",pitch);
	ls_mixer_set_pitch(music_channel,pitch); // reset the pitch to original speed
	printf("Back to normal speed\n");
	wait();
	
	printf("\nNow let's play around with the IIR filter on a channel.\nWe can change the IIR coefficients manually, or via several convenience functions.\nWe start with a second order Butterworth highpass. A highpass filter with a given cutoff frequency can be created by calling ls_mixer_set_highpass():\n");
	wait();
	
	double cutoff = 20.0; // Cutoff frequency in Hz
	
	for (cutoff = 20.0; cutoff < 10000.0; cutoff *=1.01)
	{
		printf("Highpass cutoff: %.3f Hz\n",cutoff);
		ls_mixer_set_highpass(music_channel, 2, cutoff); // apply a second order Butterworth highpass to a playing channel with a given cutoff frequency in Hz
		SDL_Delay(30);
	}
	SDL_Delay(2000);
	for (cutoff = 10000.0; cutoff > 20.0; cutoff /=1.01)
	{
		printf("Highpass cutoff: %.3f Hz\n",cutoff);
		ls_mixer_set_highpass(music_channel, 2, cutoff); // apply a second order Butterworth highpass to a playing channel with a given cutoff frequency in Hz
		SDL_Delay(30);
	}
	SDL_Delay(1000);
	
	printf("Now we reset the IIR coefficients to all pass manually by calling ls_mixer_set_iir() with the appropriate numbers. The sound is now played unaltered again.\n");
	ls_mixer_set_iir(music_channel, 1.0, 0.0, 0.0, 0.0, 0.0); // manually set the IIR filter coefficients of the channel, in this case: reset the IIR filter coefficients of the channel to all pass
	wait();
	
	printf("Now let's create a second order Butterworth lowpass with a given cutoff frequency by calling ls_mixer_set_lowpass():\n");
	wait();
	
	for (cutoff = 18000.0; cutoff > 100.0; cutoff /=1.01)
	{
		printf("Lowpass cutoff: %.3f Hz\n",cutoff);
		ls_mixer_set_lowpass(music_channel, 2, cutoff); // apply a second order Butterworth lowpass to a playing channel with a given cutoff frequency in Hz
		SDL_Delay(30);
	}
	SDL_Delay(2000);
	for (cutoff = 100.0; cutoff < 18000.0; cutoff *=1.01)
	{
		printf("Lowpass cutoff: %.3f Hz\n",cutoff);
		ls_mixer_set_lowpass(music_channel, 2, cutoff); // apply a second order Butterworth lowpass to a playing channel with a given cutoff frequency in Hz
		SDL_Delay(30);
	}
	
	
	printf("Let's reset the IIR coefficients to all pass again.\n");
	wait();
	ls_mixer_set_iir(music_channel, 1.0, 0.0, 0.0, 0.0, 0.0); // manually set the IIR filter coefficients of the channel, in this case: reset the IIR filter coefficients of the channel to all pass
	
	
	printf("We can create a first order Butterworth band pass with passband frequencies f1 and f2 by calling ls_mixer_set_bandpass():\n");
	wait();
	
	for (f1 = 100.0; f1 < 10000.0; f1 *=1.0075)
	{
		f2 = f1*1.25;
		printf("Bandpass: f1=%.1f Hz\tf2=%.1f Hz\n",f1,f2);
		ls_mixer_set_bandpass(music_channel, f1, f2); // set the IIR filter coefficients of a playing channel to a first order Butterworth bandpass with given passband frequencies f1 and f2 in Hz
		SDL_Delay(30);
	}
	SDL_Delay(2000);
	for (f1 = 10000.0; f1 > 100.0; f1 /=1.0075)
	{
		f2 = f1*1.25;
		printf("Bandpass: f1=%.1f Hz\tf2=%.1f Hz\n",f1,f2);
		ls_mixer_set_bandpass(music_channel, f1, f2); // set the IIR filter coefficients of a playing channel to a first order Butterworth bandpass with given passband frequencies f1 and f2 in Hz
		SDL_Delay(30);
	}
	
	
	printf("Let's reset the IIR coefficients to all pass again.\n");
	wait();
	ls_mixer_set_iir(music_channel, 1.0, 0.0, 0.0, 0.0, 0.0); // manually set the IIR filter coefficients of the channel, in this case: reset the IIR filter coefficients of the channel to all pass

	
	printf("We can do the same with a first order Butterworth bandstop filter by calling ls_mixer_set_bandstop():\n");
	wait();
	
	f1 = 60.0;
	f2 = 8000.0;
	printf("Bandstop: f1=%.1f Hz\tf2=%.1f Hz\n",f1,f2);
	ls_mixer_set_bandstop(music_channel, f1, f2); // set the IIR filter coefficients of a playing channel to a first order Butterworth bandstop with given stopband frequencies f1 and f2 in Hz
	wait();
	
	printf("And back to normal...\n");
	ls_mixer_set_iir(music_channel, 1.0, 0.0, 0.0, 0.0, 0.0); // manually set the IIR filter coefficients of the channel, in this case: reset the IIR filter coefficients of the channel to all pass
	wait();
	
	printf("Now we automatically fade out the channel over a time of 5 seconds.\nThis happens in the background and starts from whatever gain is currently set on the channel.\n");
	wait();
	ls_mixer_fade(music_channel,5.0,0.0); // fade channel gain from current value to 0.0 over 5 seconds
	
	printf("Nothing needs to be done while the fading is in progress, it will stop automatically.\n");
	wait();
	
	printf("And now the channel is faded back in over a time of 2 seconds.\n");
	ls_mixer_fade(music_channel,2.0,1.0); // fade channel gain from current volume to 1.0 over 2 seconds
	wait();
	
	
	printf("Now we stop the music playback on the channel by calling ls_mixer_stop() and delete the audio data from memory by calling ls_mixer_delete()\n");
	wait();
	
	ls_mixer_stop(music_channel); // stop playback of a channel
	ls_mixer_delete(music); // delete the sound source from memory
	wait();
	
	printf("\nNow we'll load a loop of a fire engine siren.\nThe sample is taken from user \"Sandermotions\" on freesound.org (see https://freesound.org/people/Sandermotions/sounds/377126/) where it was released under the Creative Commons BY 4.0 license, see https://creativecommons.org/licenses/by/4.0/\nThe looping was done by me.\nThis file is in the .wav format since it is so short that I didn't bother with compression.\n");
	
	
	ls_mixer_sounddata *firetruck;
	firetruck = ls_mixer_load("audio/377126__sandermotions__fire-truck-small_loop.wav"); // load a sound, file format can be either Ogg/Vorbis or Wave-Audio
	wait();
	
	printf("\nWe will now simulate the Doppler effect of a fire engine truck passing with 50 km/h on a street 10 m next to the listener by adjusting pitch, pan and gain accordingly:\n");
	wait();
	
	double x = -100.0; //m, position of vehicle
	double h = 10.0; // m, distance from street
	double vx = 13.889; // m/s (50 km/h), speed of vehicle
	double c = 343.2; // m/s, speed of sound
	
	pitch = 1.0 - vx*x/(c*sqrt(h*h+x*x)); // calculate the change of pitch due to the Doppler effect
	pan = atan(x/h)/1.570796327; // calculate the panning from geometry and scale it to the intervall -1 .. +1
	gain = exp(-fabs(log(h/(sqrt(h*h+x*x))))); // calculate the gain from the distance to the observer
	loop = 1; // let the sample loop seamlessly
	int firetruck_channel = ls_mixer_play(firetruck,loop,gain,pan,pitch); // play the looping sample on the next free channel
	
	for (x=-100.0;x < 100.0; x+=vx/100.0)
	{
		// update the parameters based on the position of the audio source:
		pitch = 1.0 - vx*x/(c*sqrt(h*h+x*x));
		ls_mixer_set_pitch(firetruck_channel,pitch);
		pan = atan(x/h)/1.570796327;
		ls_mixer_set_pan(firetruck_channel,pan);
		gain = exp(-fabs(log(h/(sqrt(h*h+x*x)))));
		ls_mixer_set_gain(firetruck_channel,gain);
		
		printf("Doppler-Effect: x=%.1fm\tpitch=%f\tpan=%.1f°\tgain=%.3f\n",x,pitch,90.0*pan,gain);
		SDL_Delay(10);
	}
	SDL_Delay(2000);
	
	
	printf("\nNow we'll load a sound loop of a car engine and play it back on a different channel alongside the siren.\nThe sample is taken from user \"soundjoao\" on freesound.org (see https://freesound.org/people/soundjoao/sounds/325808/) where it was released under the Creative Commons CC0 1.0 license, see https://creativecommons.org/publicdomain/zero/1.0/\n");
	ls_mixer_sounddata *motor;
	motor = ls_mixer_load("audio/325808__soundjoao__motor-loop16bit.wav"); // load a motor sound, file format can be either Ogg/Vorbis or Wave-Audio
	wait();

	printf("The firetruck now turns around and will pass the listener in the opposite direction, this time with engine sound on top. We play the engine sample with a larger gain than the siren.\n");
	wait();
	int motor_channel = ls_mixer_play(motor,loop,1.9*gain,pan,pitch); // play the looping motor sample on the next free channel, with doubled gain compared to the siren
	
	
	vx = -13.889; // m/s (50 km/h), speed of vehicle, now going in the opposite direction
	
	for (x=100.0;x > -100.0; x+=vx/100.0)
	{
		// update the parameters of both channels based on the position of the audio source:
		pitch = 1.0 - vx*x/(c*sqrt(h*h+x*x));
		ls_mixer_set_pitch(firetruck_channel,pitch);
		ls_mixer_set_pitch(motor_channel,pitch);
		pan = atan(x/h)/1.570796327;
		ls_mixer_set_pan(firetruck_channel,pan);
		ls_mixer_set_pan(motor_channel,pan);
		gain = exp(-fabs(log(h/(sqrt(h*h+x*x)))));
		ls_mixer_set_gain(firetruck_channel,0.9*gain); // fine tune the gains to avoid clipping
		ls_mixer_set_gain(motor_channel,1.5*gain);
		
		printf("Doppler-Effect: x=%.1fm\tpitch=%f\tpan=%.1f°\trealative gain=%.3f\n",x,pitch,90.0*pan,gain);
		SDL_Delay(10);
	}
	SDL_Delay(2000);
	
	printf("\nWe are done now and delete the sample data of the siren and engine loops.\n");
	wait();
	
	ls_mixer_stop(firetruck_channel); // stop playback of a channel
	ls_mixer_stop(motor_channel); // stop playback of a channel
	ls_mixer_delete(firetruck); // delete audio data from memory
	ls_mixer_delete(motor); // delete audio data from memory
	
	return 0;
}
