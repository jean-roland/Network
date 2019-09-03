/**
 * \file Timer.c
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

//*** Libraries include ***
// Standard Lib
#include <stdlib.h>
#include <string.h>
// Custom Lib
#include <MemAlloc.h>
#include <Timer.h>

// *** Definitions ***
// --- Private Types ---
typedef struct _timer_inst_info {
    uint32_t Counter;
} timer_inst_info_t;

typedef struct _timer_module_info {
    const timer_init_desc_t *pDesc;
    timer_inst_info_t *pTimerInfoList;
    uint8_t RefTimerId;
} timer_module_info_t;

// --- Private Constants ---
// --- Private Function Prototypes ---
// --- Private Variables ---
static timer_module_info_t TimerInfo;

// *** End Definitions ***

// *** Private Functions ***

// *** Public Functions ***

bool TimerInit(const timer_init_desc_t *pInitDesc) {
    if (pInitDesc != NULL) {
        TimerInfo.pDesc = pInitDesc;
        TimerInfo.pTimerInfoList = MemAllocCalloc(pInitDesc->TimerNb * sizeof(timer_inst_info_t));        
        TimerInfo.RefTimerId = 0;
        return true;
    } else {
        return false;
    }
}

bool TimerAdd(uint8_t timerId, bool isRef) {
    if (timerId < TimerInfo.pDesc->TimerNb) {
        TimerInfo.pTimerInfoList[timerId].Counter = 0;

        if (isRef) {
            TimerInfo.RefTimerId = timerId;
        }
        return true;
    } else {
        return false;
    }
}

bool TimerIncrement(uint8_t timerId) {    
    if (timerId < TimerInfo.pDesc->TimerNb) {
        TimerInfo.pTimerInfoList[timerId].Counter += 1;
        return true;
    } else {
        return false;
    }
}

uint32_t TimerRefGetTime(void) {
    return TimerInfo.pTimerInfoList[TimerInfo.RefTimerId].Counter;
}

bool TimerRefIsPassed(uint32_t timeValue) {
    uint32_t currTime = TimerRefGetTime();
    uint32_t diffTime = (currTime <= timeValue) ? timeValue - currTime : currTime - timeValue;

    return (diffTime < 0x80000000) ? (currTime >= timeValue) : (currTime <= timeValue);
}

void TimerRefWait(uint32_t waitTime) {
   volatile uint32_t currTime = TimerRefGetTime();
    volatile uint32_t endTime = currTime + waitTime;

   while (TimerRefIsPassed(endTime) != true) {
        currTime = TimerRefGetTime();
    }
}

uint32_t TimerGetTime(uint8_t timerId) {
    if (timerId < TimerInfo.pDesc->TimerNb) {
        return TimerInfo.pTimerInfoList[timerId].Counter;
    } else {
        return 0;
    }
}

bool TimerIsPassed(uint8_t timerId, uint32_t timeValue) {
    if (timerId < TimerInfo.pDesc->TimerNb) {
        uint32_t currTime = TimerGetTime(timerId);
        uint32_t diffTime = (currTime <= timeValue) ? timeValue - currTime : currTime - timeValue;
    
        return (diffTime < 0x80000000) ? (currTime >= timeValue) : (currTime <= timeValue);
    } else {
        return false;
    }
}