// MD_YM2413 Library example program.
//
// Plays MIDI notes START_NOTE to END_NOTE in sequence over and over again.
// Tests tone/note playing functions of the library using timed notes
// and supplying frequency or octave/note number for comparison of the way
// each note is generated.
//
// Library Dependencies
// MusicTable library located at https://github.com/MajicDesigns/MD_MusicTable
//

#include <MD_YM2413.h>

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

// Global Data ------------------------
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);

// Code -------------------------------
void setup(void)
{
  Serial.begin(57600);
  Serial.println(F("\n[MD_YM2413 Note Tester]"));

  S.begin();
  S.setPercussion(true);
}

void loop(void)
{
  // testing range for octaves 0 through 7
  const uint8_t START_CHANNEL = MD_YM2413::CH_HH;    // first percussion channel
  const uint8_t END_CHANNEL = MD_YM2413::CH_BD;     // last percussion channel

  // Timing constants
  const uint16_t PAUSE_TIME = 1000;  // pause between note in ms
  const uint16_t PLAY_TIME = 300;   // note playing time in ms

  // Note on/off FSM variables
  static enum { PAUSE, NOTE_ON, NOTE_OFF } state = PAUSE; // current state
  static uint32_t timeStart = 0;  // millis() timing marker
  static uint8_t chanId = START_CHANNEL;  // the next note to play
  
  S.run(); // run the sound machine every time through loop()

  // Manage the timing of notes on and off depending on 
  // where we are in the rotation/playing cycle
  switch (state)
  {
    case PAUSE: // pause between notes
      if (millis() - timeStart >= PAUSE_TIME)
        state = NOTE_ON;
      break;

    case NOTE_ON:  // play the next MIDI note
      Serial.print(F("\nChan "));
      Serial.print(chanId);
      S.noteOn(chanId, MD_YM2413::MIN_OCTAVE, 0, MD_YM2413::VOL_MAX, PLAY_TIME);

      // wraparound the note number if reached end midi notes
      chanId++;   // only do it once every 2 plays
      if (chanId > END_CHANNEL)
        chanId = START_CHANNEL;

      // next state
      state = NOTE_OFF;
      break;

    case NOTE_OFF:  // wait for note to complete
      if (S.isIdle(0))
      {
        timeStart = millis();
        state = PAUSE;
      }
      break;
  }
}