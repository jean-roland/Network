/**
 * \file Utils.c
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

// *** Libraries include ***
// Standard lib
// Custom lib
#include <Utils.h>

// *** Definitions ***
// --- Private Types ---
// --- Private Constants ---
// --- Private Function Prototypes ---
// --- Private Variables ---
// *** End Definitions ***

// *** Private Functions ***

// *** Public Functions ***

inline uint32_t UtilsRotrUint32(uint32_t n, uint8_t c) {
    return (n >> c) | (n << (32 - c));
}

inline uint16_t UtilsRotrUint16(uint16_t n, uint8_t c) {
    return (n >> c) | (n << (16 - c));
}

inline uint32_t UtilsDiffUint32(uint32_t old, uint32_t cur) {
    if(cur >= old) {
        return cur - old;
    } else {
        return (0xFFFFFFFF - (old - cur));
    }
}

inline uint16_t UtilsDiffUint16(uint16_t old, uint16_t cur) {
    if(cur >= old) {
        return cur - old;
    } else {
        return (0x0000FFFF - (old - cur));
    }
}