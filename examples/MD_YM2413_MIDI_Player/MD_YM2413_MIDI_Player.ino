// Play MIDI files from the SD card.
// 
// Dependencies
// SDFat at https://github.com/greiman?tab=repositories
// MD_MIDIFile at https://github.com/MajicDesigns/MD_MIDIFile
// MD_MusicTable at https://github.com/MajicDesigns/MD_MusicTable
//

#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <MD_YM2413.h>
#include <MD_MusicTable.h>
#include "MD_YM2413_MIDI_Map.h"

#define DEBUG 0               // flag to turn on general debug
#define PRINT_MIDI_STREAM 0   // flag to print the real time midi stream

#if DEBUG || PRINT_MIDI_STREAM
#define PRINT(s,v)  { Serial.print(F(s)); Serial.print(v); }
#define PRINTS(s)   { Serial.print(F(s)); }
#define PRINTX(s,v) { Serial.print(F(s)); Serial.print(F("0x")); Serial.print(v, HEX); }
#define PRINTF(s,v) { Serial.print(F(s)); Serial.print(v, 2); }
#else
#define PRINT(s,v)
#define PRINTS(s)
#define PRINTX(s,v)
#define PRINTF(s,v)
#endif

const uint16_t TIME_PAUSE = 2000;     // pause time between songs in ms
const uint8_t FILE_NAME_SIZE = 13;    // 8.3 + nul character
const uint8_t MIDI_PERC_CHANNEL = 9;  // MIDI percussion channel 10 as zero based channel
const uint8_t MIDI_VOL_DEFAULT = 5;   // MIDI default volume
const uint8_t PITCHBEND_RANGE = 2;    // +/- range in semitones

static const char PROGMEM PLAYLIST[][FILE_NAME_SIZE] =
{
"MARBLEMC.MID",
//"birthday.mid",
//"Mario.mid",
//"pacman.mid",
//"JZBUMBLE.MID",
//"LOOPDEMO.MID",
//"MINUET.MID",
//"TWINKLE.MID",
//"FIRERAIN.MID",
//"ELISE.MID",
//"XMAS.MID",
//"AIR.MID",
//"BANDIT.MID",
//"CHATCHOO.MID",
//"FERNANDO.MID",
//"FUGUEGM.MID",
//"GANGNAM.MID",
//"GBROWN.MID",
//"IPANEMA.mid",
//"MOZART.MID",
//"POPCORN.MID",
//"PRDANCER.MID",
"PROWLER.MID",
//"SKYFALL.MID",
//"SONATAC.MID",
};

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

const uint8_t MAX_MIDI_CHANNEL = 24;
const uint8_t MAX_MIDI_VOICE = 11;

// SD chip select pin for SPI comms.
const uint8_t SD_SELECT = 10;

// Global Data ------------------------
SdFat SD;
MD_MIDIFile SMF;
MD_MusicTable T;
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);

// Define what a channel the instrument it plays 
// and the volume at which it plays. The channel number
// is implied by the array subscript.
struct midiChannel_t
{
  MD_YM2413::instrument_t instr;
  uint8_t vol;
} midiChannel[MAX_MIDI_CHANNEL];

// Define a midi voice as a temporary allocation of a YM2413 sound
// channel to a MIDI channel so that a note can be played. There 
// can be many notes on a channel and they are released as soon as 
// the note is played.
struct midiVoice_t
{
  uint8_t midiChan;
  uint8_t ymChan;
  uint8_t note;
  uint16_t freq;
} midiVoice[MAX_MIDI_VOICE];

void resetMIDIVoices(void)
{
  for (uint8_t i = 0; i < MAX_MIDI_CHANNEL; i++)
  {
    midiChannel[i].instr = MD_YM2413::I_UNDEFINED;
    midiChannel[i].vol = MIDI_VOL_DEFAULT;
  }

  for (uint8_t i = 0; i < MAX_MIDI_VOICE; i++)
  {
    midiVoice[i].midiChan = MD_YM2413::CH_UNDEFINED;
    midiVoice[i].ymChan = MD_YM2413::CH_UNDEFINED;
  }
}

int8_t findFreeYMChan(void)
// return an available YM channel of -1 if none
{
  int8_t c = -1;

  // first see if there is an idle channel
  for (uint8_t i = 0; i < S.countChannels(); i++)
  {
    if (S.isIdle(i))
    {
      c = i;
      break;
    }
  }

  return(c);
}

int8_t findPercYMChan(uint8_t midiPercNote)
{
  int8_t c;
  uint8_t note = midiPercNote - MIDI_PMAP_BASE;
  MD_YM2413::instrument_t pinstr;
  
  if (note < ARRAY_SIZE(MidiPMap) - MIDI_PMAP_BASE)
    pinstr = pgm_read_byte(&MidiIMap[note]);

  switch (pinstr)
  {
  case MD_YM2413::P_BASS_DRUM:  c = MD_YM2413::CH_BD; break;
  case MD_YM2413::P_SNARE_DRUM: c = MD_YM2413::CH_SD; break;
  case MD_YM2413::P_HI_HAT:     c = MD_YM2413::CH_HH; break;
  case MD_YM2413::P_TOP_CYMBAL: c = MD_YM2413::CH_TCY; break;
  case MD_YM2413::P_TOM_TOM:    c = MD_YM2413::CH_TOM; break;
  default: c = -1; break;
  }
   
  return(c);
}

int8_t findFreeVoice(void)
// find a a free voice slot
{
  int8_t c = -1;

  for (uint8_t i = 0; i < MAX_MIDI_VOICE; i++)
    if (midiVoice[i].midiChan == MD_YM2413::CH_UNDEFINED &&
      midiVoice[i].ymChan == MD_YM2413::CH_UNDEFINED)
    {
      c = i;
      break;
    }

  return(c);
}

void noteOff(uint8_t chan, uint8_t note)
{
  for (uint8_t i = 0; i < MAX_MIDI_VOICE; i++)
  {
    if (midiVoice[i].midiChan == chan && midiVoice[i].note == note)
    {
      S.noteOff(midiVoice[i].ymChan);     // turn the note off
      // free up midiVoice as it has completed
      PRINT(" free v", i);
      midiVoice[i].ymChan = MD_YM2413::CH_UNDEFINED;
      midiVoice[i].midiChan = MD_YM2413::CH_UNDEFINED;
      break;
    }
  }
}

void noteOn(uint8_t chan, uint8_t note, uint8_t vol)
{
  int8_t v, c;

  v = findFreeVoice();
  if (v == -1)
  {
    PRINTS(" no voice **");
    return; // not much we can do except skip this note
  }

  if (c == MIDI_PERC_CHANNEL)
  {
    c = findPercYMChan(note);
    if (c == -1)
    {
      PRINTS(" no YM perc **");
      return; // not much we can do except skip this note
    }
  }
  else
  {
    c = findFreeYMChan();
    if (c == -1)
    {
      PRINTS(" no YM chan **");
      return; // not much we can do except skip this note
    }
  }

  PRINT(" alloc v", v);
  PRINT(" c", c);
  midiVoice[v].midiChan = chan;
  midiVoice[v].ymChan = c;

  // set the instrument if currently undefeind and not 
  // already set to this instrument
  if (midiChannel[chan].instr != MD_YM2413::I_UNDEFINED &&
     midiChannel[chan].instr != S.getInstrument(midiVoice[v].ymChan))
    S.setInstrument(midiVoice[v].ymChan, midiChannel[chan].instr, midiChannel[chan].vol);

  vol += midiChannel[chan].vol;

  if (T.findId(note))
  {
    uint16_t f = (uint16_t)(T.getFrequency() + 0.5);  // round it up

    S.noteOn(midiVoice[v].ymChan, f, vol);
    midiVoice[v].note = note;
    midiVoice[v].freq = f;
#if PRINT_MIDI_STREAM
    char buf[10];
    PRINT(" -> [", T.getId());
    PRINT("] ", T.getName(buf, sizeof(buf)));
    PRINT(" @ ", f);
    PRINT("Hz V", vol);
#endif
  }
}

void pitchBend(uint8_t chan, uint16_t bend)
// How to bend taken from
// https://sites.uci.edu/camp2014/2014/04/30/managing-midi-pitchbend-messages/
{
  PRINT(" bend ", bend);

  // calculate the adjustment factor
  int32_t temp = (bend - 8192UL) * PITCHBEND_RANGE;
  float factor = temp / 8192.0 / 12.0;
  factor = pow(2, factor);

  PRINTF(" fact ", factor);

  // now apply that factor to all the notes on this channel
  for (uint8_t v = 0; v < MAX_MIDI_VOICE; v++)
    if (midiVoice[v].midiChan == chan && !S.isPercussion(midiVoice[v].ymChan))
    {
      float freq = midiVoice[v].freq * factor;

      S.noteOn(midiVoice[v].ymChan, (uint16_t)freq, S.getVolume(midiVoice[v].ymChan));
    }
}

void midiCallback(midi_event* pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
// Data is passed using a pointer to this structure:
// typedef struct
//{
//  uint8_t track;    ///< the track this was on
//  uint8_t channel;  ///< the midi channel
//  uint8_t size;     ///< the number of data bytes
//  uint8_t data[4];  ///< the data. Only 'size' bytes are valid
//} midi_event;
{
  // Print the data if enabled
  PRINTS("\n"); // start of a new line
#if PRINT_MIDI_STREAM
  PRINT("T", pev->track);
  PRINT(" C", pev->channel + 1);
  PRINTS(":");
  for (uint8_t i = 0; i < pev->size; i++)
  {
    PRINTS(" ");
    if (pev->data[i] <= 0x0f)
      PRINTS("0");
    PRINTX("", pev->data[i]);
  }
  PRINTS(" | ");
#endif

  // Set the MIDI data to the hardware
  switch (pev->data[0])
  {
  case 0x80:  // Note off
    PRINTS("NOTE OFF");
    noteOff(pev->channel, pev->data[1]);
    break;

  case 0x90:  // Note on
    if (pev->data[2] == 0)    // velocity == 0 -> note off
    {
      PRINTS("NOTE OFF");
      noteOff(pev->channel, pev->data[1]);
    }
    else
    {
      PRINTS("NOTE_ON");
      noteOn(pev->channel, pev->data[1], map(pev->data[2], 0, 0x7f, 0, 0xf));
    }
    break;

  case 0xc0:  // Patch change
    PRINT("PATCH ", pev->data[1]+1);    // agree with published lists that start at one
    if (pev->channel < MAX_MIDI_CHANNEL)
    {
      if (pev->channel == MIDI_PERC_CHANNEL)    // the percussion channel 10 (zero based)
      {
        PRINTS(" PERC IGNORED");
      }
      else      // just an instrument channel
      {
        if (pev->data[1] < ARRAY_SIZE(MidiIMap))
          midiChannel[pev->channel].instr = pgm_read_byte(&MidiIMap[pev->data[1]]);
        else
          midiChannel[pev->channel].instr = MD_YM2413::I_UNDEFINED;

        PRINT(" +> ", midiChannel[pev->channel].instr);
      }
    }
    else
    {
      PRINTS("[INVALID CHAN]");
    }
    break;

  case 0xe0:  // Pitch Bend
    {
      uint16_t bend;
      PRINTS("PITCH BEND");

      bend = (pev->data[2] << 7) | pev->data[1];
      pitchBend(pev->channel, bend);
    }
    break;

  case 0xb0:  // Controller change
    PRINTS("CONTROLLER ");
    switch (pev->data[1])
    {
    case 0x7:
      PRINT("VOLUME ", pev->data[2]);
      midiChannel[pev->channel].vol = map(pev->data[2], 0, 0x7f, 0, 0xf);
      break;

    case 0x79: // reset all controllers
      PRINTS("RESET");
      midiSilence();
      resetMIDIVoices();
      break;

    case 0x64: // sustain
      PRINTS("SUSTAIN");
      break;

    case 0x7b:  // all notes off
      PRINTS("ALL OFF");
      midiSilence();
      break;

    default:
      PRINT("NOT HANDLED ", pev->data[1]);
      break;
    }
    break;
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  S.setVolume(MD_YM2413::VOL_OFF);
}

const char* SMFErr(int err)
{
  const char* DELIM_OPEN = "[";
  const char* DELIM_CLOSE = "]";

  static char szErr[30];
  char szFmt[10];

  static const char PROGMEM E_OK[] = "\nOK";  // -1
  static const char PROGMEM E_FILE_NUL[] = "Empty filename"; // 0 
  static const char PROGMEM E_OPEN[] = "Cannot open"; // 1
  static const char PROGMEM E_FORMAT[] = "Not MIDI"; // 2
  static const char PROGMEM E_HEADER[] = "Header size"; // 3
  static const char PROGMEM E_FILE_FMT[] = "File unsupproted"; // 5
  static const char PROGMEM E_FILE_TRK0[] = "File fmt 0; trk > 1"; // 6
  static const char PROGMEM E_MAX_TRACK[] = "Too many tracks"; // 7
  static const char PROGMEM E_NO_CHUNK[] = "no chunk"; // n0
  static const char PROGMEM E_PAST_EOF[] = "past eof"; // n1
  static const char PROGMEM E_UNKNOWN[] = "Unknown";

  static const char PROGMEM EF_ERR[] = "\n%d %s";    // error number
  static const char PROGMEM EF_TRK[] = "Trk %d,";  // for errors >= 10

  if (err == -1)    // this is a simple message
    strcpy_P(szErr, E_OK);
  else              // this is a complicated message
  {
    strcpy_P(szFmt, EF_ERR);
    sprintf(szErr, szFmt, err, DELIM_OPEN);
    if (err < 10)
    {
      switch (err)
      {
      case 0: strcat_P(szErr, E_FILE_NUL); break;
      case 1: strcat_P(szErr, E_OPEN); break;
      case 2: strcat_P(szErr, E_FORMAT); break;
      case 3: strcat_P(szErr, E_HEADER); break;
      case 4: strcat_P(szErr, E_FILE_FMT); break;
      case 5: strcat_P(szErr, E_FILE_TRK0); break;
      case 6: strcat_P(szErr, E_MAX_TRACK); break;
      case 7: strcat_P(szErr, E_NO_CHUNK); break;
      default: strcat_P(szErr, E_UNKNOWN); break;
      }
    }
    else      // error encoded with a track number
    {
      char szTemp[10];

      // fill in the track number
      strcpy_P(szFmt, EF_TRK);
      sprintf(szTemp, szFmt, err / 10);
      strcat(szErr, szTemp);

      // now do the message
      switch (err % 10)
      {
      case 0: strcat_P(szErr, E_NO_CHUNK); break;
      case 1: strcat_P(szErr, E_PAST_EOF); break;
      default: strcat_P(szTemp, E_UNKNOWN); break;
      }
    }
    strcat(szErr, DELIM_CLOSE);
  }

  return(szErr);
}

void setup(void) // This is run once at power on
{
#if DEBUG || PRINT_MIDI_STREAM
  Serial.begin(57600);
#endif
  PRINTS("\n[MD_YM2413 Midi Player]");

  // Initialise channel global data
  resetMIDIVoices();

  // Initialise SN74689
  S.begin();
  S.setPercussion(true);

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    PRINTS("\nSD init fail!");
    while (true);
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  midiSilence(); // Silence any hanging notes
}

void loop(void)
{
  static enum { IDLE, STARTING, PLAYING, ENDING } state = IDLE;
  static uint32_t timeStart;
  static uint8_t songIndex = 0;

  S.run();      // run the music machine

  switch (state)
  {
  case IDLE:        // pause a while and then start the next song
  {
    if (millis() - timeStart >= TIME_PAUSE)
    {
      PRINTS("\n* IDLE->STARTING");
      state = STARTING;
    }
  }
  break;

  case STARTING:    // load the file and start the playing process
  {
    int err;
    char sz[FILE_NAME_SIZE];

    strcpy_P(sz, PLAYLIST[songIndex]);
    PRINT("\nPlaying ", sz);
    SMF.setFilename(sz); // set filename
    if ((err = SMF.load()) != -1) // load the new file
    {
      PRINT(" SD Error: ", SMFErr(err));
      SMF.close();
      PRINTS("\n* STARTING->IDLE");
      state = IDLE;
    }
    else
    {
      PRINTS("\n* STARTING->PLAYING");
      state = PLAYING;
    }

    // increment to next song
    songIndex++;
    if (songIndex >= ARRAY_SIZE(PLAYLIST)) songIndex = 0;
  }
  break;

  case PLAYING:   // playing a song, keep the callback going
  {
    if (!SMF.isEOF())
      SMF.getNextEvent(); // Play MIDI data
    else
    {
      PRINTS("\n* PLAYING->ENDING");
      state = ENDING;
    }
  }
  break;

  case ENDING:    // just ended, clean up stuff
  {
    // clean up current environment
    SMF.close(); // close old MIDI file
    midiSilence(); // silence hanging notes
    PRINTS("\n* ENDING->IDLE");
    timeStart = millis();
    state = IDLE;
  }
  break;

  default:
    state = IDLE;
    break;
  }
}