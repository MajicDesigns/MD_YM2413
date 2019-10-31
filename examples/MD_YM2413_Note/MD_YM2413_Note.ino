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
#include <MD_MusicTable.h>

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

// Global Data ------------------------
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);
MD_MusicTable T;

// Code -------------------------------
void setup(void)
{
  Serial.begin(57600);
  Serial.println(F("\n[MD_YM2413 Note Tester]"));

  S.begin();
}

void loop(void)
{
  // testing range for octaves 0 through 7
  const uint8_t START_NOTE = 12;    // first sensible note
  const uint8_t END_NOTE =  107;     // last sensible note

  // Timing constants
  const uint16_t PAUSE_TIME = 200;  // pause between note in ms
  const uint16_t PLAY_TIME = 100;   // note playing time in ms
  const uint8_t CHAN_COUNT = 1; // S.countChannels();

  // Note on/off FSM variables
  static enum { PAUSE, NOTE_ON, NOTE_OFF } state = PAUSE; // current state
  static uint32_t timeStart = 0;  // millis() timing marker
  static uint8_t noteId = START_NOTE;  // the next note to play
  static bool playToggle = true;  // toggle between play by freq and play by octave/note
  
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
      if (T.findId(noteId))
      {
        if (playToggle) // play by freq
        {
          uint16_t f = (uint16_t)(T.getFrequency() + 0.5);  // round it up
          char buf[10];

          Serial.print(F("\n\n["));
          Serial.print(noteId);
          Serial.print(F("] "));
          Serial.print(T.getName(buf, sizeof(buf)));
          Serial.print(F(" @ "));
          Serial.print(f);
          Serial.print(F("Hz"));
          for (uint8_t i = 0; i < CHAN_COUNT; i++)
            S.noteOn(i, f, PLAY_TIME);
        }
        else  // play by note/octave
        {
          uint8_t o = T.getOctave();
          uint8_t n = T.getNoteId();
          char buf[10];

          Serial.print(F("\n["));
          Serial.print(noteId);
          Serial.print(F("] "));
          Serial.print(T.getName(buf, sizeof(buf)));
          Serial.print(F(" @ O"));
          Serial.print(o);
          Serial.print(F(" N"));
          Serial.print(n);
          for (uint8_t i = 0; i < CHAN_COUNT; i++)
            S.noteOn(i, o, n, PLAY_TIME);
        }

        playToggle = !playToggle;
      }

      // wraparound the note number if reached end midi notes
      if (playToggle) noteId++;   // only do it once every 2 plays
      if (noteId > END_NOTE)
        noteId = START_NOTE;

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