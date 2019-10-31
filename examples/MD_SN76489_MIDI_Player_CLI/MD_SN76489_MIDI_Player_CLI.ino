// Command Line Interface (CLI or Serial) to play MIDI files from the SD card.
// 
// Enter commands on the serial monitor to control the application
//
// Dependencies
// SDFat at https://github.com/greiman?tab=repositories
// MD_MIDIFile at https://github.com/MajicDesigns/MD_MIDIFile
// MD_MusicTable at https://github.com/MajicDesigns/MD_MusicTable
// MD_cmdProcessor at https://github.com/MajicDesigns/MD_cmdProcessor
//

#include <SdFat.h>
#include <MD_MIDIFile.h>
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
const uint8_t MAX_SND_CHAN = MD_SN76489::MAX_CHANNELS - 1;

// SD chip select pin for SPI comms.
const uint8_t SD_SELECT = 9;

// Miscellaneous
void(*hwReset) (void) = 0;            // declare reset function @ address 0

// Global Data ------------------------
SdFat SD;
MD_MIDIFile SMF;
MD_MusicTable T;
#if USE_DIRECT
MD_SN76489_Direct S(D_PIN, WE_PIN, true);
#else
MD_SN76489_SPI S(LD_PIN, DAT_PIN, CLK_PIN, WE_PIN, true);
#endif

bool printMidiStream = false;   // flag to print the real time midi stream

struct channelData
{
  bool idle;
  uint8_t track;
  uint8_t chan;
  uint8_t note;
} chanData[MAX_SND_CHAN];

int8_t findChan(uint8_t track, uint8_t chan, uint8_t note)
{
  uint8_t c = -1;

  for (uint8_t i = 0; i < MAX_SND_CHAN; i++)
    if (chanData[i].track == track && 
        chanData[i].chan == chan && 
        chanData[i].note == note)
    {
      c = i;
      break;
    }

  return(c);
}

int8_t findFreeChan(void)
{
  int8_t c = -1;

  for (uint8_t i = 0; i < MAX_SND_CHAN; i++)
    if (chanData[i].idle)
    {
      c = i;
      break;
    }

  return(c);
}

void noteOff(uint8_t track, uint8_t chan, uint8_t note)
{
  int8_t c;

  c = findChan(track, chan, note);
  if (c != -1)
  {
    S.note(c, 0, MD_SN76489::VOL_OFF);
    S.setVolume(c, MD_SN76489::VOL_OFF);
    if (printMidiStream)
    {
      Serial.print(F(" -> NOTE OFF C"));
      Serial.print(c);
    }
  }
}

void noteOn(uint8_t track, uint8_t chan, uint8_t note, uint8_t vol)
{
  int8_t c;

  c = findFreeChan();

  if (c != -1)
  {
    if (T.findId(note))
    {
      uint8_t v = map(vol, 0, 0x7f, 0, 0xf);
      uint16_t f = (uint16_t)(T.getFrequency() + 0.5);  // round it up
      char buf[10];

      S.note(c, f, v);
      chanData[c].idle = false;
      chanData[c].chan = chan;
      chanData[c].track = track;
      chanData[c].note = note;
      if (printMidiStream)
      {
        Serial.print(F(" -> NOTE ON C"));
        Serial.print(c);
        Serial.print(F(" ["));
        Serial.print(T.getId());
        Serial.print(F("] "));
        Serial.print(T.getName(buf, sizeof(buf)));
        Serial.print(F(" @ "));
        Serial.print(f);
        Serial.print(F("Hz V"));
        Serial.print(v);
      }
    }
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
  if (printMidiStream)
  {
    Serial.print(F("\nT"));
    Serial.print(pev->track);
    Serial.print(F(": Ch "));
    Serial.print(pev->channel + 1);
    Serial.print(F(" Data"));
    for (uint8_t i = 0; i < pev->size; i++)
    {
      Serial.print(F(" "));
      if (pev->data[i] <= 0x0f)
        Serial.print(F("0"));
      Serial.print(pev->data[i], HEX);
    }
  }

  // Set the MIDI data to the hardware
  switch (pev->data[0])
  {
  case 0x80:  // Note off
    noteOff(pev->track, pev->channel, pev->data[1]);
    break;

  case 0x90:  // Note on
    if (pev->data[2] == 0)    // velocity == 0 -> note off
      noteOff(pev->track, pev->channel, pev->data[1]);
    else
      noteOn(pev->track, pev->channel, pev->data[1], pev->data[2]);
    break;
  }
}

void midiSilence(void)
// Turn everything off on every channel.
// Some midi files are badly behaved and leave notes hanging, so between songs turn
// off all the notes and sound
{
  S.setVolume(MD_SN76489::VOL_OFF);
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

void handlerHelp(char* param); // function prototype only

void handlerZS(char *param) { hwReset(); }
void handlerZM(char *param) { midiSilence(); Serial.print(SMFErr(-1)); }
void handlerZD(char *param) { printMidiStream = !printMidiStream; Serial.print(SMFErr(-1)); }

void handlerP(char *param)
// Play
{
  int err;

  // clean up current environment
  SMF.close(); // close old MIDI file
  midiSilence(); // silence hanging notes

  Serial.print(F("\nRead File: "));
  Serial.print(param);
  SMF.setFilename(param); // set filename
  Serial.print(F("\nSet file : "));
  Serial.print(SMF.getFilename());
  err = SMF.load(); // load the new file
  Serial.print(SMFErr(err));
}

void handlerF(char *param)
// set the current folder for MIDI files
{
  Serial.print(F("\nFolder: "));
  Serial.print(param);
  SMF.setFileFolder(param); // set folder
}

void handlerL(char *param)
// list the files in the current folder
{
  SdFile file;    // iterated file

  SD.vwd()->rewind();
  while (file.openNext(SD.vwd(), O_READ))
  {
    if (file.isFile())
    {
      char buf[20];

      file.getName(buf, ARRAY_SIZE(buf));
      Serial.print(F("\n"));
      Serial.print(buf);
    }
    file.close();
  }
  Serial.print(F("\n"));
}

const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "h", handlerHelp, "",    "help" },
  { "?", handlerHelp, "",    "help" },
  { "f", handlerF,    "fldr", "set current folder to fldr" },
  { "l", handlerL,    "",     "list files in current folder" },
  { "p", handlerP,    "file", "play the named file" },
  {"zs", handlerZS,   "",     "software reset" },
  {"zm", handlerZM,   "",     "midi silence" },
  {"zd", handlerZD,   "",     "dump real time midi stream" },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); }

void setup(void) // This is run once at power on
{
  Serial.begin(57600); // For Console I/O
  Serial.print(F("\n[MD_SN74689 Midi Player CLI]"));
  Serial.print(F("\nEnsure serial monitor line ending is set to newline."));

  // Initialise SN74689
  S.begin();

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.print(F("\nSD init fail!"));
    while (true);
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  midiSilence(); // Silence any hanging notes

  // initialise command processor
  CP.begin();

  // show the available commands
  handlerHelp(nullptr);
}

void loop(void)
{
  S.play();
  for (uint8_t i = 0; i < MAX_SND_CHAN; i++)
    chanData[i].idle = S.isIdle(i);

  if (!SMF.isEOF()) 
    SMF.getNextEvent(); // Play MIDI data

  CP.run();  // process the User Interface
}