
// MD_YM2413 Library example program.
//
// Plays plays RTTTL (RingTone Text Transfer Language) songs.
// Cycles through all the includes songs in sequence.
//
// RTTTL format definition https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language
// Lots of RTTTL files at http://www.picaxe.com/RTTTL-Ringtones-for-Tune-Command/
//
// Dependencies
// MD_RTTLParser can be found at https://github.com/MajicDesigns/MD_RTTTLParser
//

#include <MD_YM2413.h>
#include <MD_RTTTLParser.h>
#include "MD_YM2413_RTTTL_Player.h" // RTTL song data in a separate file

#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define PRINT(s,v)  { Serial.print(F(s)); Serial.print(v); }
#define PRINTX(s,v) { Serial.print(F(s)); Serial.print("0x"); Serial.print(v, HEX); }
#define PRINTS(s)   { Serial.print(F(s)); }
#define PRINTC(c)   { Serial.print(c); }
#else
#define PRINT(s,v)
#define PRINTX(s,v)
#define PRINTS(s)
#define PRINTC(c)
#endif

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

const uint8_t PLAY_VOL = MD_YM2413::VOL_MAX;

// Number of playing channels and the instruments for each
const uint8_t NUM_CHAN = 3;   
const MD_YM2413::instrument_t instr[NUM_CHAN] =
{ MD_YM2413::I_PIANO, MD_YM2413::I_SYNTH_BASS, MD_YM2413::I_ACOUSTIC_BASS };

// Global Data ------------------------
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);
MD_RTTTLParser P;

void RTTTLhandler(uint8_t octave, uint8_t noteId, uint32_t duration, bool activate)
// If activate is true, play a note (octave, noteId) for the duration (ms) specified.
// If activate is false, the note should be turned off (other parameters are ignored).
{
  for (uint8_t i = 0; i < NUM_CHAN; i++)
  {
    if (activate)
      S.noteOn(i, octave, noteId, duration);
    else
      S.noteOff(i);
  }
}

void setup(void)
{
  Serial.begin(57600);
  PRINTS("\n[MD_YM2413 RTTL Player]");

  P.begin();
  P.setCallback(RTTTLhandler);

  S.begin();
  S.setVolume(PLAY_VOL);

  for (uint8_t i = 0; i < NUM_CHAN; i++)
    S.setInstrument(i, instr[i]);
}

void loop(void)
{
  // Note on/off FSM variables
  const uint16_t PAUSE_TIME = 1500;  // pause time between melodies

  static enum { START, PLAYING, WAIT_BETWEEN } state = START; // current state
  static uint32_t timeStart = 0;    // millis() timing marker
  static uint8_t idxTable = 0;      // index of next song to play

  S.run(); // run the sound machine every time through loop()

  // Manage reading and playing the note
  switch (state)
  {
    case START: // starting a new melody
      PRINTS("\n-->IDLE");
      P.setTune_P(songTable[idxTable]);
      Serial.print(F("\n"));
      Serial.print(P.getTitle());
      
      // set up for next song
      idxTable++;
      if (idxTable == ARRAY_SIZE(songTable)) 
        idxTable = 0;

      PRINTS("\n-->START to PLAYING");
      state = PLAYING;
      break;

    case PLAYING:     // playing a melody - get next note
      if (P.run())
      {
        timeStart = millis();
        state = WAIT_BETWEEN;
      }
      break;

    case WAIT_BETWEEN:  // wait at the end of a melody
      if (millis() - timeStart >= PAUSE_TIME)
        state = START;   // start a new melody
      break;
  }
}