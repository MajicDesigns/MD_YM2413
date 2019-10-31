/*
MD_YM2413 - Library for using a SN74689 sound generator

See header file for copyright and licensing comments.
*/
#include <MD_YM2413.h>
#include <MD_YM2413_lib.h>

/**
* \file
* \brief Implements hardware related methods
*/

// Global data tables
// FNum lookup table for note play within a block (octave)
// The data is organised by note number [0..11] corresponding to 
// notes C, C# .. A, A#, B
const uint16_t PROGMEM MD_YM2413::_fNumTable[] =
{
  // C,  C#,   D,  D#,   E,   F,  F#,   G,  G#,   A,  A#,   B
   172, 183, 194, 205, 217, 230, 244, 258, 274, 290, 307, 326
};

// Define the upper boundary frequency for each block 0 through 7.
// These boundaries are the Hz frequency for the first C of the 
// next block. Anything above the highest boundary is taken to 
// be one above the highest defined. 
// 3 bits allowed for in the hardware --> blocks 1 through 7 only
const uint16_t PROGMEM MD_YM2413::_blockTable[] =
{
// 0,   1,   2,   3,   4,    5,    6,    7
   0,  65, 130, 261, 523, 1046, 2093, 4186
};

uint8_t MD_YM2413::calcBlock(uint16_t freq)
// look up the block table to determine where this frequency sits
{
  uint8_t block = ARRAY_SIZE(_blockTable) - 1;

  for (uint8_t i = 0; i < ARRAY_SIZE(_blockTable); i++)
    if (freq < pgm_read_word(&_blockTable[i]))
    {
      block = i;
      break;
    }

  //DEBUG("\nBlock: F", freq); DEBUG(" = ", block);

  return(block);
}

uint16_t MD_YM2413::calcFNum(uint16_t freq, uint8_t block)
// FNum is calculated using the formula in the YM2413 application manual
// FNum = freq * (2^18/fsam) * 1/2^(block-1))
// and fsam = (Clock Freq/72)
// and octave = block_data - 1
{
  const uint32_t fsam = (CLOCK_HZ / 72UL);
  uint32_t fn = freq * (1UL << (19-block)) / fsam;

  //DEBUG("\nFNum: F", freq); DEBUG(" B", block);
  //DEBUG(" = ", fn); DEBUGX(" (0x", fn); DEBUGS(")");

  return(fn);
}

uint8_t MD_YM2413::buildReg2x(bool susOn, bool keyOn, uint8_t octave, uint16_t fNum)
{
  uint8_t b = 0;

  if (susOn) b |= (1 << R_INST_SUSTAIN_BIT);
  if (keyOn) b |= (1 << R_INST_KEY_BIT);
  b |= (octave & 0x7) << R_INST_OCTAVE_BIT;
  if (fNum & 0x100) b |= (1 << R_INST_FNUM_BIT);

  return(b);
}

uint8_t MD_YM2413::buildReg0e(bool enable, instrument_t instr, uint8_t keyOn)
{
  uint8_t b = 0;

  if (enable) b |= (1 << R_RHYTHM_SET_BIT);
  if (instr != I_UNDEFINED)   // it has been specified
  {
    // set the current state for this.
    // Note percussion channels are defined in the right order for this
    for (uint8_t i = 0; i < PERC_CHANNELS; i++)
      if (_C[i].state != IDLE)
        b |= (1 << i);

    // now set the new state
    b &= ~(1 << (instr - P_HI_HAT));  // clear the bit
    if (keyOn) b |= (1 << (instr - P_HI_HAT));  // set it if required
  }

  return(b);
}

void MD_YM2413::send(uint8_t addr, uint8_t data)
{
  // From the datasheet
  //  /WE A0
  //   1  x  = Write inhibited
  //   0  0  = Write register address
  //   0  1  = Write register content 

  //DEBUGS("\nsend");

  if (_lastAddress != addr)
  {
    //DEBUGX(" A 0x", addr);
    // write register address
    digitalWrite(_a0, LOW);
    for (uint8_t i = 0; i < DATA_BITS; i++)
      digitalWrite(_D[i], (addr & (1 << i)) ? HIGH : LOW);

    // Toggle !WE LOW then HIGH to latch it in the IC
    // wait for 12 master clock cycles (@3.6Mhz ~ 4us)
    digitalWrite(_we, LOW);
    delayMicroseconds(4);
    digitalWrite(_we, HIGH);

    _lastAddress = addr;    // remember for next time
  }

  //DEBUGX(" D 0x", data);
  digitalWrite(_a0, HIGH);
  for (uint8_t i = 0; i < DATA_BITS; i++)
    digitalWrite(_D[i], (data & (1 << i)) ? HIGH : LOW);

  // Toggle !WE LOW then HIGH to latch it in the IC
  // wait for 84 master clock cycles (@3.6Mhz ~ 25us)
  digitalWrite(_we, LOW);
  delayMicroseconds(25);
  digitalWrite(_we, HIGH);
}
