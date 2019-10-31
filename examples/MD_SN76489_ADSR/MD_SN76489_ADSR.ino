// MD_SN74689 Library example program.
//
// Tests different ADSR parameters playing notes.
// Input ADSR parameters on the serial monitor and test their effect.
//
// Library Dependencies
// MD_MusicTable library located at https://github.com/MajicDesigns/MD_MusicTable
// MD_cmdProcessor library located at https://github.com/MajicDesigns/MD_cmdProcessor
//

#include <MD_SN76489.h>
#include <MD_MusicTable.h>
#include <MD_cmdProcessor.h>

// Define if we are using a direct or SPI interface to the sound IC
// 1 = use direct, 0 = use SPI
#ifndef USE_DIRECT
#define USE_DIRECT 1
#endif

// Hardware Definitions ---------------
#if USE_DIRECT
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { A0, A1, A2, A3, 4, 5, 6, 7 };
#else
// Define the SPI related pins
const uint8_t LD_PIN = 10;
const uint8_t DAT_PIN = 11;
const uint8_t CLK_PIN = 13;
#endif
const uint8_t WE_PIN = 8;     // Arduino pin connected to the IC WE pin

// Miscellaneous
const uint8_t RCV_BUF_SIZE = 50;      // UI character buffer size
void(*hwReset) (void) = 0;            // declare reset function @ address 0

// Global Data ------------------------
#if USE_DIRECT
MD_SN76489_Direct S(D_PIN, WE_PIN, true);
#else
MD_SN76489_SPI S(LD_PIN, DAT_PIN, CLK_PIN, WE_PIN, true);
#endif

MD_MusicTable T;

char rcvBuf[RCV_BUF_SIZE];  // buffer for characters received from the console

uint16_t timePlay = 200;    // note playing time in ms
uint16_t volume = MD_SN76489::VOL_MAX; // note playing volume
uint8_t channel = 0;        // Channel being exercised

MD_SN76489::adsrEnvelope_t adsr[MD_SN76489::MAX_CHANNELS];

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
      
void handlerN(char *param)
// Play note
{
  uint16_t midiNote;

  getNum(midiNote, param);

  if (timePlay <= adsr[channel].Ta + adsr[channel].Td + adsr[channel].Tr)
    timePlay = adsr[channel].Ta + adsr[channel].Td + adsr[channel].Tr;
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
    S.note(channel, f, volume, timePlay);
  }
}

void handlerO(char *param)
{
  MD_SN76489::noiseType_t n = MD_SN76489::PERIODIC_0;

  switch (toupper(param[0]))
  {
  case 'P':
    switch (param[1])
    {
    case '0': n = MD_SN76489::PERIODIC_0; break;
    case '1': n = MD_SN76489::PERIODIC_1; break;
    case '2': n = MD_SN76489::PERIODIC_2; break;
    }
    break;

  case 'W':
    switch (param[1])
    {
    case '0': n = MD_SN76489::WHITE_0; break;
    case '1': n = MD_SN76489::WHITE_1; break;
    case '2': n = MD_SN76489::WHITE_2; break;
    }
    break;
  }

  if (timePlay <= adsr[channel].Ta + adsr[channel].Td + adsr[channel].Tr)
    timePlay = adsr[channel].Ta + adsr[channel].Td + adsr[channel].Tr;

  Serial.print(F("\n>Noise "));
  Serial.print(n);
  Serial.print(F(" for "));
  Serial.print(timePlay);
  Serial.print(F("ms"));
  S.noise(n, volume, timePlay);
}

void handlerC(char *param)
// set channel
{
  uint16_t n;
  
  getNum(n, param);

  if (n < MD_SN76489::MAX_CHANNELS) channel = n;
  Serial.print("\n>Channel ");
  Serial.print(channel);
}

void handlerV(char* param)
// Channel Volume
{
  getNum(volume, param);
  if (volume > MD_SN76489::VOL_MAX)
    volume = MD_SN76489::VOL_MAX;
  Serial.print("\n>Volume ");
  Serial.print(volume);
}

void handlerT(char *param)
// time duration
{
  getNum(timePlay, param);
  Serial.print("\n>Time ");
  Serial.print(timePlay);
}

void handlerI(char* param)
// Invert true/false
{
  uint16_t n;

  getNum(n, param);
  adsr[channel].invert = (n != 0);
  Serial.print("\n>Invert ");
  Serial.print(adsr[channel].invert);
}
      
void handlerA(char* param)
// Attack ms
{
  getNum(adsr[channel].Ta, param);
  Serial.print("\n>Ta ");
  Serial.print(adsr[channel].Ta);
}

void handlerD(char* param)
// Decay ms
{
  getNum(adsr[channel].Td, param);
  Serial.print("\n>Td ");
  Serial.print(adsr[channel].Td);
}

void handlerS(char* param)
// Sustain deltaVs
{
  uint16_t n;

  getNum(n, param);
  adsr[channel].deltaVs = n;
  Serial.print("\n>deltaVs ");
  Serial.print(adsr[channel].deltaVs);
}

void handlerR(char *param)
// Release ms
{
  getNum(adsr[channel].Tr, param);
  Serial.print("\n>Tr ");
  Serial.print(adsr[channel].Tr);
}

void handlerP(char* param)
{
  Serial.print(F("\n\nChan\tInv\tTa\tTd\tdVs\tTr\n"));
  for (uint8_t i = 0; i < MD_SN76489::MAX_CHANNELS; i++)
  {
    Serial.print(i);               Serial.print(F("\t"));
    Serial.print(adsr[i].invert);  Serial.print(F("\t"));
    Serial.print(adsr[i].Ta);      Serial.print(F("\t"));
    Serial.print(adsr[i].Td);      Serial.print(F("\t"));
    Serial.print(adsr[i].deltaVs); Serial.print(F("\t"));
    Serial.print(adsr[i].Tr);      Serial.print(F("\t"));
    Serial.print(F("\n"));
  }
  Serial.print(F("\n"));
}


const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "i", handlerI,   "b",  "Set Invert ADSR (invert b=1, noninvert b=0)" },
  { "a", handlerA,   "t",  "Set Attack time to t ms" },
  { "d", handlerD,   "t",  "Set Decay time to t ms" },
  { "s", handlerS,   "v",  "Set Sustain delta level to v units [0..15]" },
  { "r", handlerR,   "t",  "Set Release time to t ms" },
  { "c", handlerC,   "n",  "Set channel to n ([0..2] for note, 3 for noise)" },
  { "v", handlerV,   "v",  "Set channel volume to v [0..15]" },
  { "t", handlerT,   "t",  "Set play time duration to t ms" },
  { "n", handlerN,   "m",  "Play MIDI Note m" },
  { "o", handlerO,   "tf", "Play Noise sound type t [P,W] freq f [0..2]" },
  { "p", handlerP,    "",  "Show current ADSR parameters" },
  { "z", handlerZ,    "",  "Software reset" },
  { "h", handlerHelp, "",  "Show this help" },
  { "?", handlerHelp, "",  "Show thie help" },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); }

void setup(void)
{
  Serial.begin(57600);
  S.begin();
  for (uint8_t i = 0; i < MD_SN76489::MAX_CHANNELS; i++)
  {
    adsr[i].invert = false;
    adsr[i].deltaVs = 3;
    adsr[i].Ta = 20;
    adsr[i].Td = 30;
    adsr[i].Tr = 50;
    S.setADSR(i, &adsr[i]);
    S.setVolume(i, MD_SN76489::VOL_MAX);
  };

  CP.begin();

  Serial.print(F("\n[MD_SN76489 ADSR Envelope]"));
  Serial.print(F("\nEnsure serial monitor line ending is set to newline."));
  handlerHelp(nullptr);
}

void loop(void)
{
  S.play();     // run the sound machine every time through loop()
  CP.run();     // check the user input
}