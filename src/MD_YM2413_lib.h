#pragma once

#include <MD_YM2413.h>

/**
 * \file
 * \brief Library only definitions for the MD_YM2413 library
 */

#ifndef LIBDEBUG
#define LIBDEBUG 0    ///< Control debugging output. See \ref pageCompileSwitch
#endif

#if LIBDEBUG
#define DEBUGS(s) { Serial.print(F(s)); }
#define DEBUGX(s, v) { Serial.print(F(s)); Serial.print(v, HEX); }
#define DEBUG(s, v) { Serial.print(F(s)); Serial.print(v); }
#else
#define DEBUGS(s)
#define DEBUGX(s, v)
#define DEBUG(s, v)
#endif

// Miscellaneous defines
#define VOL(v) (15-v)           ///< internal volume [0..15] mapped to hardware attenuation [15..0]
#define DATA_BITS 8             ///< Number of bits in the byte (for loops)
#define CLOCK_HZ  3579545UL     ///< Nominal 3.6MHz clock (3.579545MHz)

