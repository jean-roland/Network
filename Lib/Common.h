/**
 * \file Common.h
 * \brief Module containing common declarations
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

#ifndef _Common_h
#define _Common_h

// *** Libraries include ***
// Standard lib
#include <stdint.h>
#include <stdbool.h>
// Custom lib

// *** Definitions ***
// --- Public Types ---
typedef void error_notify_ft(uint16_t);
typedef uint32_t timer_get_time_ft(void);
typedef bool timer_is_passed_ft(uint32_t);
typedef void system_connect_ft(bool connect);
typedef void it_handler_ft(void);

// --- Public Constants ---
// --- Public Variables ---
// --- Public Function Prototypes ---

// *** End Definitions ***
#endif // _Common_h