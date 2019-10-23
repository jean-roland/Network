/**
 * \file Utils.h
 * \brief Module containing useful functions
 * \author Jean-Roland Gosse

    This file is part of Network.

    Network is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Network is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Network. If not, see <https://www.gnu.org/licenses/>
 */

#ifndef _utils_h
#define _utils_h

// *** Libraries include ***
// Standard lib
#include <stdint.h>
// Custom lib

// *** Definitions ***
// --- Public Types ---
// --- Public Constants ---
// Swap 2 bytes of a word
#define SWAP16(x)   (uint16_t)(((x & 0xff) << 8) | (x >> 8))

// --- Public Variables ---
// --- Public Function Prototypes ---

/**
 * \fn uint32_t UtilsRotrUint32(uint32_t n, uint8_t c)
 * \brief Realise a uint32_t rotation to the right
 *
 * \param n number to rotate
 * \param c number of bits of the rotation
 * \return uint32_t: result of the rotation
 */
uint32_t UtilsRotrUint32(uint32_t n, uint8_t c);

/**
 * \fn uint16_t UtilsRotrUint16(uint16_t n, uint8_t c)
 * \brief Realise a uint16_t rotation to the right
 *
 * \param n number to rotate
 * \param c number of bits of the rotation
 * \return uint16_t: result of the rotation
 */
uint16_t UtilsRotrUint16(uint16_t n, uint8_t c);

/**
 * \fn uint32_t UtilsDiffUint32(uint32_t old, uint32_t cur)
 * \brief Gives the difference between two uint32_t values with roll-over
 *
 * \param old previous value
 * \param cur current value
 * \return uint32_t: difference between the two values
 */
uint32_t UtilsDiffUint32(uint32_t old, uint32_t cur);

/**
 * \fn uint16_t UtilsDiffUint16(uint16_t old, uint16_t cur)
 * \brief Gives the difference between two uint16_t values with roll-over
 *
 * \param old previous value
 * \param cur current value
 * \return uint16_t: difference between the two values
 */
uint16_t UtilsDiffUint16(uint16_t old, uint16_t cur);

// *** End Definitions ***
#endif // _Utils_h