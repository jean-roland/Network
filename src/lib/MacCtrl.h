/**
 * \file MacCtrl.h
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

#ifndef _mac_ctrl_h
#define _mac_ctrl_h

// *** Libraries include ***
// Standard Lib
#include <stdbool.h>
#include <stdint.h>
// Custom Lib
#include <Common.h>
#include <Libip.h>

// *** Definitions ***
// --- Public Types ---
typedef struct _mac_init_desc {
    uint8_t MacCtrlNb;
} mac_init_desc_t;

typedef struct _mac_ctrl_init_desc {
    uint16_t FifoRxDescSize;
    uint32_t FifoRxSize;
} mac_ctrl_init_desc_t;

// --- Public Constants ---
// --- Public Variables ---
// --- Public Function Prototypes ---

/**
 * \fn bool MacCtrlInit(const mac_init_desc_t *pInitDesc)
 * \brief Module initialization
 *
 * \param pInitDesc pointer to the module init descriptor
 * \return bool: true if operation is successful
 */
bool MacCtrlInit(const mac_init_desc_t *pInitDesc);

/**
 * \fn bool MacCtrlAdd(uint8_t macCtrlId, const mac_ctrl_init_desc_t *pCtrlInitDesc)
 * \brief Add a mac controller instance
 *
 * \param macCtrlId mac controller id
 * \param pCtrlInitDesc pointer to the controller init descriptor
 * \return bool: true if operation is successful
 */
bool MacCtrlAdd(uint8_t macCtrlId, const mac_ctrl_init_desc_t *pCtrlInitDesc);

/**
 * \fn bool MacCtrlSetMacAddress(uint8_t macCtrlId, const uint8_t *pNewMacAddr)
 * \brief Modify the mac controller mac address /!\ WARNING /!\ Requires a reset of phy interfaces
 *
 * \param macCtrlId mac controller id
 * \param pNewMacAddr pointer to the new mac address
 * \return bool: true if operation is successful
 */
bool MacCtrlSetMacAddress(uint8_t macCtrlId, const uint8_t *pNewMacAddr);

/**
 * \fn bool MacCtrlWriteData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize)
 * \brief Store data in the rx fifo (should be called by mac controller interruption)
 *
 * \param macCtrlId mac controller id
 * \param pNewMacAddr pointer to the new mac address
 * \param buffSize buffer size
 * \return bool: true if operation is successful
 */
bool MacCtrlWriteData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize);

/**
 * \fn bool MacCtrlHasData(uint8_t macCtrlId)
 * \brief Returns if a mac controller have data stored
 *
 * \param macCtrlId mac controller id
 * \return bool: true if there is data
 */
bool MacCtrlHasData(uint8_t macCtrlId);

/**
 * \fn bool MacCtrlGetData(uint8_t macCtrlId, uint8_t *pBuffer, uint16_t *pBuffSize)
 * \brief Retrieve data from the mac controller
 *
 * \param macCtrlId mac controller id
 * \param pBuffer pointer to the buffer to contain data
 * \param pBuffSize pointer to contain the data size
 * \return bool: true if operation is successful
 */
bool MacCtrlGetData(uint8_t macCtrlId, uint8_t *pBuffer, uint16_t *pBuffSize);

/**
 * \fn bool MacCtrlSendData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize)
 * \brief Send data through the mac controller
 *
 * \param macCtrlId mac controller id
 * \param pBuffer pointer to the send data
 * \param buffSize buffer size
 * \return bool: true if operation is successful
 */
bool MacCtrlSendData(uint8_t macCtrlId, const uint8_t *pBuffer, uint16_t buffSize);

// *** End Definitions ***
#endif // _mac_ctrl_h