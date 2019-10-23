/**
 * \file MacCtrl.c
 * \brief Mac controller manager
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
// Standard lib
#include <string.h>
// Custom lib
#include <Utils.h>
#include <MemAlloc.h>
#include <Fifo.h>
#include <MacCtrl.h>

// *** Definitions ***
// --- Private Types ---
typedef struct _mac_ctrl_info {
    const mac_ctrl_init_desc_t *pInitDesc;
    fifo_desc_t* pMsgFifoRx;
    fifo_desc_t* pMsgFifoRxDesc;
} mac_ctrl_info_t;

typedef struct _mac_ctrl_module_info {
    const mac_init_desc_t *pInitDesc;
    mac_ctrl_info_t *pMacCtrlInfoTable;
} mac_ctrl_module_info_t;

// --- Private Constants ---
// --- Private Function Prototypes ---
// --- Private Variables ---
static mac_ctrl_module_info_t MacCtrlInfo;

// *** End Definitions ***

// *** Private Functions ***

// *** Public Functions ***

bool MacCtrlInit(const mac_init_desc_t *pInitDesc) {
    if (pInitDesc != NULL) {
        MacCtrlInfo.pInitDesc = pInitDesc;
        MacCtrlInfo.pMacCtrlInfoTable = MemAllocCalloc((uint32_t)sizeof(mac_ctrl_info_t) * pInitDesc->MacCtrlNb);
        return true;
    } else {
        return false;
    }
}

bool MacCtrlAdd(uint8_t macCtrlId, const mac_ctrl_init_desc_t *pCtrlInitDesc) {
    if ((macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) && (pCtrlInitDesc != NULL)) {
        MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pInitDesc = pCtrlInitDesc;
        MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc = FifoCreate(pCtrlInitDesc->FifoRxDescSize, sizeof(uint16_t));
        MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRx = FifoCreate(pCtrlInitDesc->FifoRxSize, sizeof(uint8_t));
        return true;
    } else {
        return false;
    }
}

bool MacCtrlSetMacAddress(uint8_t macCtrlId, const uint8_t *pNewMacAddr) {
    if ((macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) && (pNewMacAddr != NULL)) {
        // TODO: Set mac address in component registers
        return true;
    } else {
        return false;
    }
}

bool MacCtrlWriteData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize) {
    if ((macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) && (pBuffer != NULL)) {
        if (FifoFreeSpace(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc) > 0) {
            if (FifoWrite(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRx, pBuffer, buffSize)) {
                if(FifoWrite(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc, &buffSize, 1)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool MacCtrlHasData(uint8_t macCtrlId) {
    if (macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) {
        return (FifoItemCount(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc) != 0);
    } else {
        return false;
    }
}

bool MacCtrlGetData(uint8_t macCtrlId, uint8_t *pBuffer, uint16_t *pBuffSize) {
    if ((macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) && (pBuffer != NULL) && (pBuffSize != NULL)) {
        // TODO: Deactivate interruptions
        // Attempt to get message descriptor
        if (FifoRead(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc, pBuffSize, 1, false)) {
            // Attempt to get message data
            if (FifoRead(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRx, pBuffer, *pBuffSize, true)) {
                FifoConsume(MacCtrlInfo.pMacCtrlInfoTable[macCtrlId].pMsgFifoRxDesc, 1);
                return true;
            }
        }
        // TODO: Reactivate interruptions
    }
    return false;
}

bool MacCtrlSendData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize) {
    if ((macCtrlId < MacCtrlInfo.pInitDesc->MacCtrlNb) && (pBuffer != NULL)) {
        // TODO: Send data to component
        return true;
    } else {
        return false;
    }
}