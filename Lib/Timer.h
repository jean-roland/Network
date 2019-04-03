/**
 * \file Timer.h
 * \brief Timer manager
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

#ifndef _Timer_h
#define _Timer_h

//*** Libraries include ***
// Standard Lib
#include <stdbool.h>
#include <stdint.h>
// Custom Lib
#include <Common.h>

// *** Definitions ***
// --- Public Types ---
typedef struct _timer_init_desc {
    uint8_t TimerNb;
} timer_init_desc_t;

// --- Public Constants ---
// --- Public Variables ---
// --- Public Function Prototypes ---

/**
 * \fn bool TimerInit(const timer_init_desc_t *pInitDesc)
 * \brief Initialize the module
 *
 * \param pInitDesc module init descriptor
 * \return bool: true if operation successfull
 */
bool TimerInit(const timer_init_desc_t *pInitDesc);

/**
 * \fn bool TimerAdd(uint8_t timerId, bool isRef)
 * \brief Add a timer instance
 *
 * \param timerId timer instance id
 * \param isRef makes this instance the reference timer
 * \return bool: true if operation successfull
 */
bool TimerAdd(uint8_t timerId, bool isRef);

/**
 * \fn bool TimerIncrement(uint8_t timerId)
 * \brief Increment a timer counter (should be called on clock/counter interruptuon)
 *
 * \param timerId timer instance id
 * \return bool: true if operation successfull
 */
bool TimerIncrement(uint8_t timerId);

/**
 * \fn uint32_t TimerRefGetTime(void)
 * \brief Returns the reference timer counter
 *
 * \return uint32_t: reference counter value
 */
uint32_t TimerRefGetTime(void);

/**
 * \fn bool TimerRefIsPassed(uint32_t timeValue)
 * \brief Returns if a time is passed (using the reference timer)
 *
 * \param timeValue time value we want to test
 * \return bool: true if time is passed
 */
bool TimerRefIsPassed(uint32_t timeValue);

/**
 * \fn void TimerRefWait(uint32_t waitTime)
 * \brief Wait actively for a period of time
 *
 * \param waitTime waiting time value (ms)
 * \return void
 */
void TimerRefWait(uint32_t waitTime);

/**
 * \fn uint32_t TimerGetTime(uint8_t timerId)
 * \brief Returns a timer counter
 *
 * \param timerId timer instance id
 * \return uint32_t: counter value
 */
uint32_t TimerGetTime(uint8_t timerId);

/**
 * \fn bool TimerIsPassed(uint8_t timerId, uint32_t timeValue)
 * \brief Returns if a time is passed
 *
 * \param timerId timer instance id
 * \param timeValue time value we want to test
 * \return bool: true if time is passed
 */
bool TimerIsPassed(uint8_t timerId, uint32_t timeValue);

// *** End Definitions ***
#endif // _Timer_h
