// MD_YM2413 Library example program.
//
// Allows interactive CLI setting of instrument parameters for exploring
// what effect they can produce.
//
// Library Dependencies
// MD_MusicTable library located at https://github.com/MajicDesigns/MD_MusicTable
// MD_cmdProcessor library located at https://github.com/MajicDesigns/MD_cmdProcessor
//

#include <MD_YM2413.h>
#include <MD_MusicTable.h>
#include <MD_cmdProcessor.h>
#include "midi_instruments.h"
#include "midi_drums.h"

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

// Miscellaneous
const uint8_t RCV_BUF_SIZE = 50;      // UI character buffer size
void(*hwReset) (void) = 0;            // declare reset function @ address 0

// Global Data ------------------------
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);
MD_MusicTable T;

char rcvBuf[RCV_BUF_SIZE];  // buffer for characters received from the console

uint16_t timePlay = 1500;    // note playing time in ms
uint16_t volume = MD_YM2413::VOL_MAX; // note playing volume

const uint8_t CHANNEL = 0;   // Channel being exercised

// Code -------------------------------
void handlerHelp(char* param); // function prototype only

char *getNum(uint16_t &n, char *psz)
{
  n = 0;

  while (*psz >= '0' && *psz <= '9')
  {
    n = (n * 10) + (*psz - '0');
    psz++;
  }
  
  if (*psz != '\0') psz--;
  
  return(psz);
}

void handlerZ(char* param) { hwReset(); }
      
void handlerP(char *param)
// Play note
{
  uint16_t midiNote;

  param = getNum(midiNote, param);

  if (T.findId(midiNote));
  {
    uint16_t f = (uint16_t)(T.getFrequency() + 0.5);  // round it up
    char buf[10];

    Serial.print(F("\n>Play "));
    Serial.print(midiNote);
    Serial.print(" (");
    Serial.print(T.getName(buf, sizeof(buf)));
    Serial.print(F(") @ "));
    Serial.print(f);
    Serial.print(F("Hz"));
    S.noteOn(CHANNEL, f, volume, timePlay);
  }
}

void handlerV(char* param)
// Channel Volume
{
  param = getNum(volume, param);
  if (volume > MD_YM2413::VOL_MAX)
    volume = MD_YM2413::VOL_MAX;
  Serial.print("\n>Volume ");
  Serial.print(volume);
}

void handlerT(char *param)
// time duration
{
  param = getNum(timePlay, param);
  Serial.print("\n>Time ");
  Serial.print(timePlay);
}

void handlerL(char* param)
{
  uint16_t inst;

  param = getNum(inst, param);
  inst &= 127;  // ensure in range
  Serial.print("\n>Instrument ");
  Serial.print(inst);
  S.loadInstrumentOPL2(midiInstruments[inst], true);
}

const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "h", handlerHelp, "", "Show this help" },
  { "?", handlerHelp, "", "Show this help" },
  { "v", handlerV,   "v", "Set channel volume to v [0..15]" },
  { "t", handlerT,   "t", "Set play time duration to t ms" },
  { "l", handlerL,   "n", "Load MIDI instrument n"},
  { "p", handlerP,   "m", "Play MIDI Note m" },
  { "z", handlerZ,    "", "Software reset" },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); }

void setup(void)
{
  Serial.begin(57600);
  S.begin();
  S.setVolume(CHANNEL, volume);
  S.setInstrument(CHANNEL, MD_YM2413::I_CUSTOM);
  S.loadInstrumentOPL2(0);

  CP.begin();

  Serial.print(F("\n[MD_YM2413 Custom]"));
  Serial.print(F("\nEnsure serial monitor line ending is set to newline."));
  handlerHelp(nullptr);
}

void loop(void)
{
  S.run();      // run the sound machine every time through loop()
  CP.run();     // check the user input
}