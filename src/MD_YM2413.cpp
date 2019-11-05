/*
MD_YM2413 - Library for using a SN74689 sound generator

See header file for copyright and licensing comments.
*/
#include <MD_YM2413.h>
#include <MD_YM2413_lib.h>

/**
* \file
* \brief Implements class definition and general methods
*/

// Class methods
MD_YM2413::MD_YM2413(const uint8_t* D, uint8_t we, uint8_t a0):
_D(D), _we(we), _a0(a0)
{ }

void MD_YM2413::begin(void)
{
  // Set all pins to outputs and initialise
  for (int8_t i = 0; i < DATA_BITS; i++)
    pinMode(_D[i], OUTPUT);
  pinMode(_we, OUTPUT);
  pinMode(_a0, OUTPUT);

  digitalWrite(_we, HIGH);

  // initialise the hardware defaults
  send(R_TEST_CTL_REG, 0);    // never test mode
  setPercussion(false);       // all instruments to default (below)
}

void MD_YM2413::initChannels(void)
{
  for (uint8_t i = 0; i < countChannels(); i++)
  {
    setInstrument(i, DEFAULT_INSTRUMENT, VOL_MAX);
    _C[i].state = IDLE;
  }
}

bool MD_YM2413::isIdle(uint8_t chan)
{
  bool b = false;

  if (chan < countChannels())
    b = (_C[chan].state == IDLE);

  return(b);
}

uint8_t MD_YM2413::countChannels(void)
{
  if (isPercussion())
    return(MAX_CHANNELS);
  else
    return(ALL_INSTR_CHANNELS);
}

bool MD_YM2413::isPercussion(uint8_t chan)
// return true if the channel is a percussion channel
{
  return(isPercussion() && chan >= PERC_CHAN_BASE && chan < countChannels());
}

void MD_YM2413::setPercussion(bool enable)
// Set the library and hardware to include or exclude percussion instruments
{
  uint8_t x = 0;

  _enablePercussion = enable;

  // enable/disable the mode in hardware
  x = buildReg0e(enable, I_UNDEFINED, false);
  send(R_RHYTHM_CTL_REG, x);

  // now set up the instruments to suit new mode
  if (enable)
  {
    // all the percussion instruments are set up one per channel
    for (uint8_t i = PERC_CHAN_BASE; i < MAX_CHANNELS; i++)
      setInstrument(i, (instrument_t)(P_HI_HAT + i - PERC_CHAN_BASE), VOL_MAX);

    // set registers as per Application Manual section (III-1-7)
    send(0x16, 0x20);
    send(0x17, 0x50);
    send(0x18, 0xc0);
    send(0x26, 0x07);
    send(0x27, 0x05);
    send(0x28, 0x01);
  }
  else
    initChannels();
}

bool MD_YM2413::setInstrument(uint8_t chan, instrument_t instr, uint8_t vol)
// 'attach' the specified instruiment to the channel
{
  if (chan >= countChannels() ||    // not a valid channel
     (!isPercussion() && instr >= P_HI_HAT))  // not a valid instrument for this mode
    return(false);

  _C[chan].instrument = instr;
  _C[chan].vol = vol;
  if (!isPercussion() ||      // not in percussion mode or ...
    (isPercussion() && (chan < PART_INSTR_CHANNELS))) // ... percussion on, but not a percussion channel
  {
    send(R_CHAN_CTL_BASE_REG + chan, (_C[chan].instrument << R_CHAN_INST_BIT) | (VOL(_C[chan].vol) << R_CHAN_VOL_BIT));
  }

  return(true);
}

void MD_YM2413::setVolume(uint8_t chan, uint8_t v)
// Set the volume set point for channel and remember the setting
// Application values are 0-15 for min to max. Attenuator values
// are the complement of this (15-0).
{
  uint8_t addr, data = 0;

  if (chan >= countChannels())
    return;

  if (v > VOL_MAX) v = VOL_MAX;   // sanity bound the volume
  _C[chan].vol = v;

  if (!isPercussion() ||      // not in percussion mode
    (isPercussion() && (chan < PERC_CHAN_BASE))) // percussion on, but not a percussion channel
  {
    addr = R_CHAN_CTL_BASE_REG + chan;
    data = (_C[chan].instrument << R_CHAN_INST_BIT) | (VOL(_C[chan].vol) << R_CHAN_VOL_BIT);
  }
  else
  {
    // Percussion mode is on and this is a percussion channel. 
    // These need to be sent in pairs as the registers
    // are organised in nybbles for different percussion instruments
    switch (_C[chan].instrument)
    {
    case P_BASS_DRUM:
      addr = R_PERC_VOL_BD_REG;
      data = (VOL(_C[CH_BD - PERC_CHAN_BASE].vol) << R_PERC_VOL_BD_BIT);
      break;

    case P_HI_HAT:
    case P_SNARE_DRUM:
      addr = R_PERC_VOL_HHSD_REG;
      data = (VOL(_C[CH_HH - PERC_CHAN_BASE].vol) << R_PERC_VOL_HH_BIT);
      data |= (VOL(_C[CH_SD - PERC_CHAN_BASE].vol) << R_PERC_VOL_SD_BIT);
      break;

    case P_TOM_TOM:
    case P_TOP_CYMBAL:
      addr = R_PERC_VOL_TOMTCY_REG;
      data = (VOL(_C[CH_TOM - PERC_CHAN_BASE].vol) << R_PERC_VOL_TOM_BIT);
      data |= (VOL(_C[CH_TCY - PERC_CHAN_BASE].vol) << R_PERC_VOL_TCY_BIT);
      break;

    default:    // remove compiler warnings
      break;
    }
  }

  // finally send what we have assembled
  send(addr, data);
}

void MD_YM2413::setVolume(uint8_t v)
// Set the same volume set point for all channels
{
  for (int8_t i = 0; i < countChannels(); i++)
    setVolume(i, v);
}

void MD_YM2413::noteOn(uint8_t chan, uint16_t freq, uint8_t vol, uint16_t duration)
// turn on a note by specifying a frequency
{
  uint8_t data;

  DEBUG("\nnoteOn C", chan);
  DEBUG(" F", freq);

  if (chan > countChannels())
    return;

  setVolume(chan, vol);
  if (!isPercussion(chan))
  {
    _C[chan].octave = calcBlock(freq);
    _C[chan].fNum = calcFNum(freq, _C[chan].octave);
    DEBUG(" -> B", _C[chan].octave);
    DEBUG(" FNum", _C[chan].fNum);
    data = buildReg2x(_C[chan].sustain, true, _C[chan].octave, _C[chan].fNum);

    // send the fnum data and then the note on request
    send(R_FNUM_BASE_REG + chan, _C[chan].fNum & 0xff);
    send(R_INST_CTL_BASE_REG + chan, data);
  }
  else
  {
    // this is a percussion channel
    data = buildReg0e(true, _C[chan].instrument, true);

    // send the data across
    send(R_RHYTHM_CTL_REG, data);
  }

  // common data 
  _C[chan].frequency = freq;
  _C[chan].duration = duration;
  _C[chan].timeBase = millis();
  _C[chan].state = SUSTAIN;
}

void MD_YM2413::noteOn(uint8_t chan, uint8_t octave, uint8_t note, uint8_t vol, uint16_t duration)
// turn on a note by speficying the octave and note number
{
  uint8_t data;

  DEBUG("\nnoteOn C", chan);
  DEBUG(" O", octave);
  DEBUG(" N", note);

  if (note >= ARRAY_SIZE(_fNumTable) || chan >= countChannels())
    return;

  if (!isPercussion(chan))
  {
    if (octave < MIN_OCTAVE) octave = MIN_OCTAVE;
    if (octave > MAX_OCTAVE) octave = MAX_OCTAVE;
    _C[chan].octave = octave;
    note = min(note, ARRAY_SIZE(_fNumTable)-1);   // bound it;
    _C[chan].fNum = pgm_read_word(&_fNumTable[note]);
    DEBUG(" -> B", _C[chan].octave);
    DEBUG(" FNum", _C[chan].fNum);
    data = buildReg2x(_C[chan].sustain, true, _C[chan].octave, _C[chan].fNum);

    // send the fnum data and then the note on request
    send(R_FNUM_BASE_REG + chan, _C[chan].fNum & 0xff);
    send(R_INST_CTL_BASE_REG + chan, data);
  }
  else
  {
    // this is a percussion channel
    data = buildReg0e(true, _C[chan].instrument, true);

    // send the data across
    send(R_RHYTHM_CTL_REG, data);
  }

  // common data
  setVolume(chan, vol);
  _C[chan].frequency = 0;   // not used
  _C[chan].duration = duration;
  _C[chan].timeBase = millis();
  _C[chan].state = SUSTAIN;
}

void MD_YM2413::noteOff(uint8_t chan)
// turn off a note
{
  uint8_t data;

  DEBUG("\nnoteOff C", chan);

  setVolume(chan, VOL_OFF);   // silence it  as well as turn off
  if (!isPercussion(chan))
  {
    data = buildReg2x(_C[chan].sustain, false, _C[chan].octave, _C[chan].fNum);
    send(R_INST_CTL_BASE_REG + chan, data);
  }
  else
  {
    // this is a percussion channel
    data = buildReg0e(true, _C[chan].instrument, false);

    // send the data across
    send(R_RHYTHM_CTL_REG, data);
  }

  // common data
  _C[chan].state = IDLE;
}

void MD_YM2413::run(void)
// If channel has duration configured, wait for the duration to 
// expire and turn the note off.
{
  for (uint8_t chan = 0; chan < countChannels(); chan++)
    if (_C[chan].state == SUSTAIN && _C[chan].duration != 0)
    {
      if (millis() - _C[chan].timeBase >= _C[chan].duration)
        noteOff(chan);
    }
}


