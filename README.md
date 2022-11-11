# ls_mixer
A lightweight `C` sound mixing library that I've written for myself to use instead of SDL_Mixer.

The key differences to SDL_Mixer are:

* No distinction between music and sounds, everything is just audio, whether it be long or short
* No external library dependencies other than SDL2
* The playback speed per channel can be contolled like on a turntable
* An arbitrary number of Ogg/Vorbis files can be decoded on the fly, no sound data is kept in decoded form in memory
* One arbitrary second order IIR filter available for each channel, with convenience functions for low/high-pass and band pass/stop filters
* One master second order IIR filter available for the mixed signal
* Can only play back Ogg/Vorbis or WAVE files
* The maximum number of audio channels is set during compile time via pre-processor definition in `ls_mixer.h`

With this library you can:

* Play sounds with adjustable
	* Gain
	* Panning
	* Pitch
	* IIR-filter coefficients
* Seamlessly loop / pause / resume audio
* Apply IIR filters to your audio channels
* Create callback functions for when a channel stopped
* Automatically fade channels in or out

This project reuses code from:

* `cmixer.c` (https://github.com/rxi/cmixer)
* `stb_vorbis.c` (https://github.com/nothings/stb)
* `liir.c` (https://exstrom.com/journal/sigproc/dsigproc.html)


## Documentation
See `ls_mixer.h` for a documented list of functions and `demo.c` for a demonstration of most features.
Use CMake to compile the demo program (only tested on GNU/Linux so far...)

## Usage
Add the `*.c` and `*.h` files to your project and include `ls_mixer.h` in your code. No need to link to anything else except SDL2.
