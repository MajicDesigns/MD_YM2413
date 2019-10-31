# MD_YM2413

The SN76489 Digital Complex Sound Generator (DCSG) is a TTL-compatible programmable
sound generator chip from Texas Instruments. It provides:
- 3 programmable square wave tone generators (122Hz to 125kHz)
- 1 noise generator (white noise and periodic noise at 3 different frequencies)
- 16 different volume levels
- Simultaneous sounds

This library implements functions that manage the sound and noise generation interface
to the SN76489 IC through a clean API encapsulating the basic functionality provided
by the hardware.

Additionally, the library provides programmable ADSR envelope management of the sounds
produced, allowing a more versatile sound output with minimal programming effort.

[Library Documentation](https://majicdesigns.github.io/MD_YM2413/)