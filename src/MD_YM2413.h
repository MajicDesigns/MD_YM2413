#pragma once

#include <Arduino.h>

/**
 * \file
 * \brief Main header file for the MD_YM2413 library
 */

/**
\mainpage YM2413 Sound Synthesizer Library
The YM2413 Synthesizer IC
-------------------------

![YM2413 IC] (YM2413_IC.png "YM2413 IC")

The YM2413, OPLL, is a cost-reduced FM synthesis sound chip manufactured 
by Yamaha Corporation and based on their YM3812 (OPL2).

The simplifications mean that the YM2413 can only play one user-defined 
instrument at a time, with an additional 15 read-only hard-coded instrument
profiles available. The IC can operate as 9 channels of instruments or 6 
channels with melodic instruments and 5 with hard-coded percussion instruments.

Its main historical application was the generation of music and sound effects in 
microprocessor systems. It was extensively used in early game consoles, arcade games, 
home computers and low-cost synthesizer keyboards.

This library implements functions that manage the sound and noise generation interface
to the YM2413 IC through a clean API encapsulating the basic functionality provided
by the hardware.

Topics
------
- \subpage pageHardware
- \subpage pageLibrary
- \subpage pageCustom
- \subpage pageCompileSwitch
- \subpage pageRevisionHistory
- \subpage pageCopyright
- \subpage pageDonation

References
----------
- On-line IC data sheet at http://map.grauw.nl/resources/sound/yamaha_ym2413_frs.pdf
- Additional technical information from http://www.smspower.org/Development/YM2413

\page pageRevisionHistory Revision History
Feb 2020 version 1.0.0
- Initial implementation.

\page pageHardware Hardware Connections
YM2413 IC Description
---------------------

![YM2413 Block Diagram] (YM2413_BlockDiagram.png "YM2413 Block Diagram")

This is the block diagram of the YM2413. It has an 8 bit bus interface with one 
address line to select one of two write-only registers. One register is used 
to select the byte register to access 8 of the 271 internal register bits and 
one for writing data to the selected register.

On the analog side there are two outputs, one each for the Rhythm and Melody
outputs.

The IC DIP package has 18 pins with the following pinout:

     GND ->  1 +-----+ 18 <- D1
      D2 ->  2 |  Y  | 17 <- D0
      D3 ->  3 |  M  | 16 <- Vcc
      D4 ->  4 |  2  | 15 <- RO
      D5 ->  5 |  4  | 14 <- MO
      D6 ->  6 |  1  | 13 <- /IC
      D7 ->  7 |  3  | 12 <- /CS
     XIN ->  8 |     | 11 <- /WE
    XOUT ->  9 +-----+ 10 <- /AO


| Signal| Description
|-------|---------------
|D0-D7  | Command byte inputs for OPLL command
|/WE    | Active low Write Enable (latches data)
|/CS    | Active low Chip Select (connect to GND or MCU output if more than one IC shares D0-D7)
|/IC    | Chip reset (pull up to Vcc or connect MCU reset)
|AO     | Address/Data discriminator
|MO     | Melody Output
|RO     | Rhythm Output
|CLK    | 4MHz clock signal
|VCC    | 5V
|GND    | Ground

Note: If multiple ICs are interfaced, then the ICs CS line must also be used
to select the right device and share the data lines.

| /CS | /WE | AO  | Action
|:---:|:---:|:---:|:------------
| 1   | x   | x   | Chip not selected, inactive mode
| 0   | 1   | x   | Data bus inhibited mode
| 0   | 0   | 0   | Address write mode
| 0   | 0   | 1   | Data write mode

Hardware Direct Connection to the MCU
-------------------------------------
The library uses 8 digital output data lines from the Arduino MCU and 
additional AO and WE digital outputs to load the data into the YM2413 IC.

The data pins used are for the application to specify as part of the 
object initialization parameters. The D array parameter has pin numbers 
arranged to correspond to the IC pins (ie, pin D[0] is connected to IC 
pin D0, D[1] to D1, etc). The AO and WE pins can be any arbitrary pins.

Connections between the MCU and YM2413 are mapped as shown below.
The Arduino pins are arbitrary and those the pins shown are used 
in the library examples.

| Arduino Pin | YM2413                |
|-------------|------------------------|
| D0   [D8]   | D0    [17]             |
| D1   [D9]   | D1    [18]             |
| D2   [D7]   | D2    [ 2]             |
| D3   [D6]   | D3    [ 3]             |
| D4   [A0]   | D4    [ 4]             |
| D5   [A1]   | D5    [ 5]             |
| D6   [A2]   | D6    [ 6]             |
| D7   [A3]   | D7    [ 7]             |
| AO   [D4]   | /AO   [10]             |
| WE   [ 8]   | /WE   [ 5]             |
|             | /CS   [ 6] (GND)       |
|             | RO    [15] (Amplifier) |
|             | MO    [14] (Amplifier) |
|             | /IC   [13] (MCU Reset) |

Audio Output
------------
The Audio output from pins 14 and 15 (MO, RO) of the IC can combined into 
a mono signal to directly feed an amplifier and external speaker.

\page pageLibrary Using the Library
Defining the object
-------------------
The object definition include a list of all the I/O pins that are used
to connect the MCU to the YM2413 IC.

setup()
-------
The setup() function must include the begin() method. All the I/O pins are
initialized at this time.

loop()
------
Automatic note off events (see below) are managed by the library. 
For this to happen in a timely manner, the loop() function must invoke
run() every iteration through loop(). The run() method executes very quickly 
if the library has nothing to process, imposing minimal overheads on the user
application. 

run() may be omitted if the application manages all the noteOn() and noteOff() 
events within the application.

Playing a Note
--------------
A note starts with a __note on__ event and ends with a __note off__ event.
The note on event is generated when the noteOn() method is invoked in the 
application code.

Note Off Events
---------------
The library provides flexibility on how the note off events are generated.

Invoking noteOn() __without a duration parameter__ means the user code 
needs to generate the note off for the channel. This method is suited 
to applications that directly link a physical event to playing the note (eg, 
switch on for note on and switch off for note off), or where the music being 
played includes its own note on and off events (eg, a MIDI score).

Invoking noteOn() __with a duration parameter__ causes the library to 
generate a note off event at the end of the specified total duration. This 
method suits applications where the sound duration is defined by the music 
being played (eg, RTTTL tunes). In this case the user code can determine 
if the noteOff() event has been generated by using the isIdle() method.

\page pageCustom Custom Instruments
Defining and using Custom Instruments
--------------------------------------

The library implements methods to load custom instruments into the single 
available customizable slot.

The library is able to take two styles of instrument definitions
- OPL2/OPL3 style definitions that can be downloaded from from various sites. The 
OPL2/3 definitions are translated into an OPLL compatible format. The major 
difference is in the way that wave types are handled - OPL2/3 definitions can have
up to 7 different wave types while OPLL is limited to full or half sine wave - and
the library has a mapping between the two.
- Native OPLL (YM2413) definitions if these are known.

To play the custom instrument, use the loadInstrumentOPL2() or loadInstrument() methods
to load the data for the custom instrument and then set the channel that will use this
instrument to I_CUSTOM. 

\page pageCompileSwitch Compiler Switches

LIBDEBUG
--------
Controls debugging output to the serial monitor from the library. If set to
1 debugging is enabled and the main program must open the Serial port for output

\page pageDonation Support the Library
If you like and use this library please consider making a small donation 
using [PayPal](https://paypal.me/MajicDesigns/4USD)

\page pageCopyright Copyright
Copyright (C) 2020 Marco Colli. All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*/

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))  ///< Standard method to work out array size

/**
 * Base class for the MD_YM2413 library
 */
class MD_YM2413
{
  public:
    static const uint8_t VOL_OFF = 0x0;     ///< Convenience constant for volume off
    static const uint8_t VOL_MAX = 0xf;     ///< Convenience constant for volume maximum

    static const uint8_t MIN_OCTAVE = 1;    ///< smallest octave playable
    static const uint8_t MAX_OCTAVE = 8;    ///< largest playable octave

    static const uint8_t CH_UNDEFINED = 255;  ///< undefined channel indicator
    static const uint8_t OPL2_DATA_SIZE = 12; ///< OPL2 instrument definition size

    static const uint8_t PERC_CHAN_BASE = 6;            ///< Base channel number for percussion instruments if enabled
    static const uint8_t CH_HH = PERC_CHAN_BASE + 0;    ///< HI HAT channel number
    static const uint8_t CH_TCY = PERC_CHAN_BASE + 1;   ///< TOP CYMBAL channel number
    static const uint8_t CH_TOM = PERC_CHAN_BASE + 2;   ///< TOM TOM channel number
    static const uint8_t CH_SD = PERC_CHAN_BASE + 3;    ///< SNARE DRUM channel number
    static const uint8_t CH_BD = PERC_CHAN_BASE + 4;    ///< BASS DRUM channel number

   /** 
    * Predefined musical and percussion instrument definitions
    * The IC has a number of predefined profiles instruments with IDs that 
    * are defined here.
    */
    typedef enum
    {
      // These instrument definitions match those for the 
      // hardware register.
      I_CUSTOM = 0,
      I_VIOLIN = 1,
      I_GUITAR = 2,
      I_PIANO = 3,
      I_FLUTE = 4,
      I_CLARINET = 5,
      I_OBOE = 6,
      I_TRUMPET = 7,
      I_ORGAN = 8,
      I_HORN = 9,
      I_SYNTH = 10,
      I_HARPSICHORD = 11,
      I_VIBRAPHONE = 12,
      I_SYNTH_BASS = 13,
      I_ACOUSTIC_BASS = 14,
      I_EGUITAR = 15,
      // These percussion definitions are offset to match 
      // the bit position for the 0E register and are the 
      // channel numbers for these.
      P_HI_HAT = 16 + 0,
      P_TOP_CYMBAL = 16 + 1,
      P_TOM_TOM = 16 + 2,
      P_SNARE_DRUM = 16 + 3,
      P_BASS_DRUM = 16 + 4,

      I_UNDEFINED = 0xff,
    } instrument_t;

   /**
    * Class Constructor.
    *
    * Instantiate a new instance of this derived class. The parameters passed are used to
    * connect the software to the hardware. Multiple instances may co-exist.
    *
    * The D array is arranged to correspond to the IC pins (ie, pin D[0] is connected
    * to IC pin D0, D[1] to D1, etc). D0 is the MSB in the data byte, D7 the LSB.
    *
    * The we and a0 pins are used for handshaking the data over the data bus.
    *
    * \sa \ref pageHardware
    *
    * \param D       pointer to array of 8 pins used as the data bus interface
    * \param we      pin number used as write enable
    * \param a0      pin number used as address/data selector
    */
    MD_YM2413(const uint8_t* D, uint8_t we, uint8_t a0);

   /**
    * Class Destructor.
    *
    * Does the necessary to clean up once the object is no longer required.
    */
    ~MD_YM2413(void) {};

   /**
    * Initialize the object.
    *
    * Initialize the object data. This needs to be called during setup() to initialize
    * new data for the class that cannot be done during the object creation.
    * 
    * All the I/O is initialize, percussion mode is disabled and all instruments are set 
    * as I_PIANO at VOL_MAX volume by default.
    *
    * \sa setVolume(), setInstrument(), setPercussion()
    */
    void begin(void);
    
   //--------------------------------------------------------------
   /** \name Hardware and Library Management.
    * @{
    */
   /**
    * Return the number of channels.
    *
    * Return the number of channels depending on whether the percussion instruments
    * are set.
    *
    * \sa setPercussion()
    *
    * \return the number of channels in the current configuration.
    */
    uint8_t countChannels(void);

   /**
    * Return the current library mode
    *
    * Returns the current library/hardware operating mode set by setPercussion().
    * 
    * \sa setPercussion()
    *
    * \return true if percussion channels are enabled, false otherwise.
    */
    bool isPercussion(void) { return(_enablePercussion); }

   /**
    * Check if the channel is for percussion
    *
    * Returns whether the specified channel is allocated to a percussion 
    * instrument. Percussion instruments are allocated to specific channels as
    * explained in setPercussion().
    *
    * \sa setPercussion()
    *
    * \param chan  channel to check [0..countChannels()-1].
    * \return true if specified is for percussion, false otherwise.
    */
    bool isPercussion(uint8_t chan);

   /**
    * Set the current library/hardware operating mode.
    *
    * The library can operate with or without percussion instruments configured.
    * Without percussion instruments, there are 9 channels(0..8] available for
    * general instruments. With percussion enabled, there are 6 channels[0..5]
    * available for general instruments and 5 channels[6..10], for a total of 11,
    * allocated to percussion instruments.
    *
    * The method countChannels() will always return the total number of channels for
    * the current configuration.
    * 
    * \sa isPercussion(), countChannels()
    *
    * \param bEnable true to enable percussion mode, false otherwise.
    */
    void setPercussion(bool bEnable);

   /**
    * Define the parameters for a custom instrument.
    *
    * Define the playing envelope in OPL2/OPL3 format for the custom instrument.
    * The parameters are in higher OPL format, which is readily available and very
    * close to the YM2413 format (OPLL). Translations are made as required.
    *
    * \sa \ref pageCustom
    *
    * \param ins         an array of OPL2_DATA_SIZE uint8_t bytes in OPL2 format data.
    * \param fromPROGMEM true if the data is loaded from PROGMEM false otherwise.
    */
    void loadInstrumentOPL2(const uint8_t* ins, bool fromPROGMEM = true);

   /**
    * Define direct parameters for a custom instrument.
    *
    * Define the data for a custom instrument directly. The data is preformatted
    * for the YM2413 device and will be passed through without further processing.
    *
    * This method is used to set instrument definitions that are in OPLL format
    * and held in the application as a compact byte sequence.
    *
    * \sa \ref pageCustom
    *
    * \param data  an array of 8 bytes in RAM that will be written to registers 0x00 through 0x07.
    */
    void loadInstrument(const uint8_t* data) { for (uint8_t i = 0; i < 8; i++) send(i, data[i]); }

   /**
    * Standardize the instrument release phase
    *
    * For the specific channel, set the release value to a standarized mid-point
    * for this instance of the instrument.
    *
    * \sa setInstrument()
    *
    * \param chan    channel number on which sustain is set [0..countChannels()-1].
    * \param sustain set true to standardize the release phase to a mid level value.
    */
    void setSustain(uint8_t chan, bool sustain) { if (chan < countChannels()) _C[chan].sustain = sustain; }

   /**
    * Return the idle state of a channel.
    *
    * Used to check if a channel is currently idle (ie, not playing a note).
    *
    * \param chan  channel to check [0..countChannels()-1].
    * \return true if the channel is idle, false otherwise.
    */
    bool isIdle(uint8_t chan);

   /**
    * Run the music machine.
    *
    * Runs the automatic note off timing for all channels. This should be called
    * from the main loop() as frequently as possible to allow the library to execute
    * the note required timing for each channel.
    *
    * This method is not required if the application does not use durations when
    * invoking noteOn().
    */
    void run(void);

   /**
    * Write a byte directly to the device
    *
    * This method should be used with caution, as it bypasses all the checks
    * and buffering built into the library. It is provided to support applications
    * that are a collection of register setting to be written to hardware at set
    * time intervals (eg, VGM files).
    *
    * \param addr  the 8 bit device address to write the data.
    * \param data  the 8 bit data value to write to the device.
    */
    inline void write(uint8_t addr, uint8_t data) { send(addr, data); }
    
   /** @} */

   //--------------------------------------------------------------
   /** \name Sound Management.
    * @{
    */
   /**
    * Set the playing instrument for a channel.
    *
    * Set the instrument for the channel to be one the value specified.
    * Valid instruments are given by the enumerated type instrument_t.
    *
    * If percussion is not enabled, the valid range of instruments is 
    * one of the I_* values on channels [0..9]. The percussion instruments 
    * P_* are automatically set up on the channels CH_* [6..11] when 
    * percussion mode is enabled, and instruments I_* can be set up on 
    * channels [0..5].
    *
    * Custom instruments must be defined before they can be used.
    *
    * \sa setPercussion(), instrument_t, setSustain(), defineInstrument()
    *
    * \param chan    channel number on which volume is set [0..countChannels()-1].
    * \param instr   one of the instruments I_* from instrument_t.
    * \param vol     volume to set for the specified channel in range [VOL_MIN..VOL_MAX].
    * \return true if the instrument was set correctly.
    */
    bool setInstrument(uint8_t chan, instrument_t instr, uint8_t vol = VOL_MAX);

   /**
    * Get the current instrument setting
    *
    * Get the instrument set for the specified channel. This will be one of
    * the instrument_t values.
    *
    * \sa setInstrument(), instrument_t
    *
    * \param chan    channel number on which volume is set [0..countChannels()-1].
    * \return the value of the instrument set for the channel.
    */
    instrument_t getInstrument(uint8_t chan) 
    { if (chan < countChannels()) return(_C[chan].instrument); else return(I_UNDEFINED); }

   /**
    * Get the volume for a channel.
    *
    * Get the current volume for a channel.
    *
    * \param chan  channel number on which volume is set [0..countChannels()-1].
    * \return the current volume in the range [VOL_MIN..VOL_MAX].
    */
    uint8_t getVolume(uint8_t chan) { return(chan < countChannels() ? _C[chan].vol : 0); }

   /**
    * Set the volume for a channel.
    *
    * Set the volume for a channel to be the value specified.
    * Valid values are all the values in the range [VOL_MIN..VOL_MAX].
    *
    * \param chan  channel number on which volume is set [0..countChannels()-1].
    * \param v     volume to set for the specified channel in range [VOL_MIN..VOL_MAX].
    */
    void setVolume(uint8_t chan, uint8_t v);

   /**
    * Set the volume for all channels.
    *
    * Set the volume for all channels to be the specified value. Valid values are all
    * the values in the range [VOL_MIN..VOL_MAX].
    *
    * \param v     volume to set for all channels in range [VOL_MIN..VOL_MAX].
    */
    void setVolume(uint8_t v);

   /**
    * Play a note (frequency)
    *
    * Output a note with frequency freq on the specified channel using the
    * instrument currently defined for the channel.
    *
    * If specified, the duration will cause an automatic note off
    * event when the total time has expired. If duration is 0 the 
    * note will be sustained until it is turned off by the application.
    *
    * \sa noteOff(), run()
    *
    * \param chan     channel number on which to play this note [0..countChannels()-1].
    * \param freq     frequency to play.
    * \param vol     volume to set this note in range [VOL_MIN..VOL_MAX].
    * \param duration length of time in ms for the whole note to last.
    */
    void noteOn(uint8_t chan, uint16_t freq, uint8_t vol, uint16_t duration = 0);

   /**
    * Play a note (octave and note#)
    *
    * Output a note defined by octave and note number on the specified 
    * channel using the instrument currently defined for the channel.
    *
    * Middle C is the first note in octave 4 (C4). Notes
    * [C, C#, D, D#, E, F, F#, G, G#, A, A#, B] are numbered sequentially
    * 0 to 11 within the octave.
    * 
    * If specified, the duration will cause an automatic note off
    * event when the total time has expired. If duration is 0 the
    * note will be sustained until it is turned off by the application.
    *
    * \sa noteOff(), run()
    *
    * \param chan    channel number on which to play this note [0..countChannels()-1].
    * \param octave  the octave block for this note [MIN_OCTAVE..MAX_OCTAVE].
    * \param note    the note number to play [0..11] as defined above.
    * \param vol     volume to set this note in range [VOL_MIN..VOL_MAX].
    * \param duration length of time in ms for the whole note to last.
    */
    void noteOn(uint8_t chan, uint8_t octave, uint8_t note, uint8_t vol, uint16_t duration = 0);

   /**
    * Stop playing a note
    *
    * Stop the note currently playing on the specified channel
    * 
    * Causes a key-off event to be processed by the channel.
    *
    * \sa noteOn(), run()
    *
    * \param chan    channel number on which to stop this note [0..countChannels()-1].
    */
    void noteOff(uint8_t chan);

   /** @} */
  private:
    // channels sizing definitions
    static const uint8_t ALL_INSTR_CHANNELS = 9;  ///< Number of instrument channels when all instruments
    static const uint8_t PART_INSTR_CHANNELS = 6; ///< Number of instrument channels when shared with percussion
    static const uint8_t PERC_CHANNELS = 5;       ///< Number of percussion channels if they are defined
    static const uint8_t MAX_CHANNELS = (PART_INSTR_CHANNELS+PERC_CHANNELS); ///< Worst case channel slots needed
    static const instrument_t DEFAULT_INSTRUMENT = I_PIANO;  ///< USed as the default instrument for initialization

    // Hardware register definitions
    static const uint8_t R_RHYTHM_CTL_REG = 0x0e;      ///< Rhythm control register address
    static const uint8_t R_RHYTHM_SET_BIT = 5;         ///< Rhythm control register mode set bit position

    static const uint8_t R_TEST_CTL_REG = 0x0f;        ///< Test mode register - just reset this to 0 always
    static const uint8_t R_FNUM_BASE_REG = 0x10;       ///< F-Num bits 0-7 base register address

    static const uint8_t R_INST_CTL_BASE_REG = 0x20;   ///< Instruments control base address
    static const uint8_t R_INST_SUSTAIN_BIT = 5;       ///< Instrument sustain mode bit position
    static const uint8_t R_INST_KEY_BIT = 4;           ///< Instrument key on/off bit position
    static const uint8_t R_INST_OCTAVE_BIT = 1;        ///< Instrument octave 3 bits lsb bit position
    static const uint8_t R_INST_FNUM_BIT = 0;          ///< Instrument F-Num msb bit position

    static const uint8_t R_CHAN_CTL_BASE_REG = 0x30;   ///< Channel control selection base address
    static const uint8_t R_CHAN_INST_BIT = 4;          ///< Channel instrument id 4 bits lsb bit position
    static const uint8_t R_CHAN_VOL_BIT = 0;           ///< Channel volume 4 bits lsb bit position

    static const uint8_t R_PERC_VOL_BD_REG = 0x36;     ///< Base Drum volume register
    static const uint8_t R_PERC_VOL_BD_BIT = 0;        ///< Base Drum volume lsb bit position
    static const uint8_t R_PERC_VOL_HHSD_REG = 0x37;   ///< Hi Hat/Snare Drum volume register
    static const uint8_t R_PERC_VOL_HH_BIT = 4;        ///< Hi Hat drum volume lsb bit position
    static const uint8_t R_PERC_VOL_SD_BIT = 0;        ///< Snare Drum volume lsb bit position
    static const uint8_t R_PERC_VOL_TOMTCY_REG = 0x38; ///< Tom Tom/Top Cymbal volume register
    static const uint8_t R_PERC_VOL_TOM_BIT = 4;       ///< Tom Tom volume lsb bit position
    static const uint8_t R_PERC_VOL_TCY_BIT = 0;       ///< Top Cymbal volume lsb bit position

    // Dynamic data held per tone channel
    enum channelState_t 
    {
      IDLE,     ///< doing nothing waiting a noteOn
      SUSTAIN,  ///< playing a note 
    };

    struct channelData_t
    {
      instrument_t instrument;  ///< the instrument assigned to this channel
      bool sustain;             ///< true if the instrument needs to be sustained after playing
      uint8_t vol;              ///< volume set point for this channel, 0-15 (map to attenuator 15-0)
      uint16_t frequency;       ///< the frequency being played, 0 if not specified this way
      uint8_t octave;           ///< the octave for this note
      uint16_t fNum;            ///< the note frequency offset
      uint16_t duration;        ///< the total playing duration in ms

      // FSM tracking variables
      channelState_t  state;  ///< current note playing state
      uint32_t timeBase;      ///< base time for current time operation
    };
    
    channelData_t _C[MAX_CHANNELS];   ///< real-time tracking data for each channel

    // Variables
    const uint8_t* _D; ///< YM2413 IC pins D0-D7 in that order
    uint8_t _we;       ///< YM2413 write Enable output pin (active low)
    uint8_t _a0;       ///< YM2413 address selector output pin

    bool _enablePercussion;   ///< true if percussion instruments are enabled
    uint8_t _lastAddress;     ///< used by send() to remember the last address and not repeat send if same

    // External static data
    static const uint16_t _fNumTable[12];
    static const uint16_t _blockTable[8];

    // Methods
    void initChannels(void);
    uint16_t calcFNum(uint16_t freq, uint8_t block);
    uint8_t calcBlock(uint16_t freq);
    uint8_t buildReg2x(bool susOn, bool keyOn, uint8_t octave, uint16_t fNum);
    uint8_t buildReg0e(bool enable, instrument_t instr, uint8_t keyOn);
    void send(uint8_t addr, uint8_t data);
};

