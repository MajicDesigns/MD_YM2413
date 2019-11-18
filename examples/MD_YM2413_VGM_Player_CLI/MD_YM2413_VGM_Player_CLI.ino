// Command Line Interface (Serial) to play VGM files from the SD card.
// 
// Enter commands on the serial monitor to control the application
//
// Dependencies
// SDFat at https://github.com/greiman?tab=repositories
// MD_cmdProcessor at https://github.com/MajicDesigns/MD_cmdProcessor
//
// VGM file format at https://vgmrips.net/wiki/VGM_Specification
// VGM sound files at https://vgmrips.net/packs/chip/ym2413

#include <SdFat.h>
#include <MD_YM2413.h>
#include <MD_cmdProcessor.h>

#define SHOW_MORE_INFO 1   // set to 1 to show all more info while running

const uint8_t DEFAULT_LOOP_REPEAT = 1;  // number of time to repeat loops by default

// Hardware Definitions ---------------
// All the pins directly connected to D0-D7 on the IC, in sequential order 
// so that pin D_PIN[0] is connected to D0, D_PIN[1] to D1, etc.
const uint8_t D_PIN[] = { 8, 9, 7, 6, A0, A1, A2, A3 };
const uint8_t WE_PIN = 5;     // Arduino pin connected to the IC WE pin
const uint8_t A0_PIN = 4;     // Arduino pin connected to the A0 pin

// SD chip select pin for SPI comms.
const uint8_t SD_SELECT = 10;

// Miscellaneous
void(*hwReset) (void) = 0;            // declare reset function @ address 0
const uint16_t SAMPLE_uS = 23; // 1e6 us/sec at 44100 samples/sec = 22.67us per sample

// Global Data ------------------------
SdFat SD;
SdFile FD;    // file descriptor
MD_YM2413 S(D_PIN, WE_PIN, A0_PIN);

uint32_t dataOffset = 0;    // offset to start of music data in VGM file
bool playingVGM = false;    // flag true when in playing mode
uint32_t loopCount = 0;     // loop count for the section to be looped
uint32_t loopOffset = 0;    // loop offset into the file

void playVGM(void)
{
  uint32_t samples = 0;
  int cmd = FD.read();

  if (cmd == -1) cmd = 0x66;    // make this stop
  
  switch (cmd)
  {
    case 0x51: // 0x51 aa dd : YM2413, write value dd to register aa
      {
        uint8_t aa = FD.read();
        uint8_t dd = FD.read();
        S.write(aa, dd);
      }
      break;

    case 0x61: // 0x61 nn nn : Wait n samples, n can range from 0 to 65535
      samples = FD.read() & 0x00FF;
      samples |= (FD.read() << 8) & 0xFF00;
      delay((samples * SAMPLE_uS) / 1000);
      break;

    case 0x62: // wait 735 samples (60th of a second)
      delay(17);  // SAMPLE_uS * 735 = 16.905ms
      break;

    case 0x63: // wait 882 samples (50th of a second)
      delay(20);  // SAMPLE_uS * 882 = 20.286ms
      break;

    case 0x70 ... 0x7f: // 0x7n : wait n+1 samples, n can range from 0 to 15
      samples = cmd + 0x70 + 1;
      delayMicroseconds(samples * SAMPLE_uS);
      break;

    case 0x66: // 0x66 : end of sound data
      if (loopCount != 0)
      {
#if SHOW_MORE_INFO
        Serial.print("\nLooping");
#endif
        loopCount--;
        FD.seekSet(loopOffset); // loop back
      }
      else
        handlerS(nullptr);    // end it now
      break;

    default:
#if SHOW_MORE_INFO
      Serial.print(F("\nUnhandled 0x")); 
      Serial.print(cmd, HEX);
#endif
      break;
  } //end switch
}

uint32_t readVGMDword(uint16_t fileOffset)
// All integer values are *unsigned* and written in 
// "Intel" byte order (Little Endian), so for example 
// 0x12345678 is written as 0x78 0x56 0x34 0x12.
{
  uint8_t data[4];
  uint32_t result;

  FD.seekSet(fileOffset);
  for (uint8_t i = 0; i < ARRAY_SIZE(data); i++)
    data[i] = FD.read() & 0xff;

  result = 0;
  for (int8_t i = ARRAY_SIZE(data) - 1; i >= 0; i--)
    result = (result << 8) | data[i];

  return(result);
}

uint16_t checkVGMHeader(char* file)
{
  uint32_t offset = 0;
  uint32_t x, version;

  // try to open the file
  if (file[0] == '\0')
    return(0);

  if (!FD.open(file, O_READ))
  {
    Serial.print(F("\nFile not found."));
    return(0);
  }

  // read the header for VGM signature (0x00)
  x = readVGMDword(0x00);
  if (x != 0x206d6756) // " mgV"
  {
    Serial.print(F("\nNot a VGM file: 0x"));
    Serial.print(x, HEX);
    return(0);
  }

  // get the VGM version (0x08)
  version = readVGMDword(0x08);
  Serial.print(F("\nVGM version: 0x"));
  Serial.print(version, HEX);

  // verify YM2413 clock speed (0x10)
  x = readVGMDword(0x10);
  Serial.print(F("\nYM2413 clock: "));
  Serial.print(x);

  // get the relative data offset value (0x34)
  if (version < 0x150)
    offset = 0x40;
  else
  {
    offset = readVGMDword(0x34);
    offset += 0x34;  // make it absolute file offset
  }
#if SHOW_MORE_INFO
  Serial.print(F("\nData offset: 0x"));
  Serial.print(offset, HEX);
#endif

  // loop count and loop offset (0x1c)
  loopOffset = readVGMDword(0x1c);
  if (loopOffset == 0)
    loopCount = 0;
  else
  {
    loopOffset += 0x1c;  // make it absolute file offset
    loopCount = DEFAULT_LOOP_REPEAT;   // number of loops
  }
#if SHOW_MORE_INFO
  Serial.print(F("\nLoop x"));
  Serial.print(loopCount);
  Serial.print(F(" from offset 0x"));
  Serial.print(loopOffset, HEX);
#endif

  // set up the next character read to be at start of data
  FD.seekSet(offset);

  return(offset);
}

void handlerHelp(char* param); // function prototype only

void handlerZ(char *param) { hwReset(); }

void handlerS(char* param)
// Stop play
{ 
  S.setVolume(MD_YM2413::VOL_OFF);
  playingVGM = false;
  FD.close();
  Serial.print(F("\nStopped."));
}

void handlerP(char *param)
// Play
{
  Serial.print(F("\n\nVGM file: "));
  Serial.print(param);
  dataOffset = checkVGMHeader(param); // load the new file
  playingVGM = (dataOffset != 0);
}

void handlerF(char *param)
// set the current folder for MIDI files
{
  Serial.print(F("\nFolder: "));
  Serial.print(param);
  if (!SD.chdir(param, true)) // set folder
    Serial.print(F(" unable to set."));
}

void handlerL(char *param)
// list the files in the current folder
{
  SdFile file;    // iterated file

  SD.vwd()->rewind();
  while (file.openNext(SD.vwd(), O_READ))
  {
    char buf[20];

    file.getName(buf, ARRAY_SIZE(buf));
    Serial.print(F("\n"));
    Serial.print(buf);
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
  { "s", handlerS,    "",     "stop playing current file" },
  { "z", handlerZ,    "",     "software reset" },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); }

void setup(void) // This is run once at power on
{
  Serial.begin(57600); // For Console I/O
  Serial.print(F("\n[MD_SN74689 VGM Player CLI]"));
  Serial.print(F("\nEnsure serial monitor line ending is set to newline."));

  // Initialise YM2413
  S.begin();
  S.setVolume(MD_YM2413::VOL_OFF);

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.print(F("\nSD init fail!"));
    while (true);
  }

  // initialise command processor
  CP.begin();

  // show the available commands
  handlerHelp(nullptr);
}

void loop(void)
{
  if (playingVGM) 
    playVGM();
  // else
    CP.run();  // process the User Interface
}