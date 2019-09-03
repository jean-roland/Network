/**
 * \file Network.h
 * \brief Ethernet UDP/IP Network manager
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

#ifndef _network_h
#define _network_h

//*** Libraries include ***
// Standard Lib
#include <stdbool.h>
#include <stdint.h>
// Custom Lib
#include <Common.h>
#include <Libip.h>

// *** Definitions ***
// --- Public Types ---
typedef void network_mac_ctrl_set_mac_addr_ft(uint8_t ctrlId, const uint8_t *pNewMacAddr);
typedef bool network_mac_ctrl_has_msg_ft(uint8_t ctrlId);
typedef bool network_mac_ctrl_get_msg_ft(uint8_t ctrlId, uint8_t *message, uint16_t *messageSize);
typedef bool network_mac_ctrl_send_msg_ft(uint8_t ctrlId, const uint8_t *message, uint16_t messageSize);

typedef struct _network_gen_itfc {
    error_notify_ft *pFnErrorNotify; // function called in case of errors (optional)
    timer_get_time_ft *pFnTimerGetTime;
    timer_is_passed_ft *pFnTimerIsPassed;
} network_gen_itfc_t;

typedef struct _network_init_desc {
    network_gen_itfc_t GenInterface;
    uint16_t ErrorCode; // module unique error code
    uint8_t CtrlNb; // number of network controlers
    uint8_t PortNb; // number of network ports
} network_init_desc_t;

typedef struct _network_com_itfc {
    network_mac_ctrl_set_mac_addr_ft *MacCtrlSetMacAddr;
    network_mac_ctrl_has_msg_ft* MacCtrlHasMsg;
    network_mac_ctrl_get_msg_ft* MacCtrlGetMsg;
    network_mac_ctrl_send_msg_ft* MacCtrlSendMsg;
} network_com_itfc_t;

typedef struct _network_ctrl_desc {
    network_com_itfc_t ComInterface;
    uint8_t DefaultMacAddr[MAC_ADDR_LENGTH];
    uint8_t DefaultIpAddr[IP_ADDR_LENGTH];
    uint8_t DefaultSubnetMask[IP_ADDR_LENGTH];
    uint8_t MacCtrlId; // mac controller id associated to this controller
    uint8_t ArpEntryNb; // number of ARP entries in the controller ARP table   
} network_ctrl_desc_t;

typedef struct _network_port_desc {
    uint8_t NetworkCtrlId; // network controller id associated to this port
    uint8_t Protocol;
    uint8_t DefaultDstIpAddr[IP_ADDR_LENGTH];
    uint16_t DefaultInPortNb;
    uint16_t DefaultOutPortNb;
    uint16_t RxFifoSize; // Rx fifo size (in bytes)
    uint16_t RxDescFifoSize; // Rx fifo size (in message number), if 0 reception will be in COM port mode
    uint16_t TxFifoSize; // Tx fifo size (in bytes)
    uint16_t TxDescFifoSize; // Tx fifo size (in message number), if 0 transmission will be in COM port mode
} network_port_desc_t;

// --- Public Constants ---
// --- Public Variables ---
// --- Public Function Prototypes ---

/**
 * \fn bool NetworkInit(const network_init_desc_t *pInitDesc)
 * \brief Initialize the module
 *
 * \param pInitDesc module init descriptor
 * \return bool: true if operation successfull
 */
bool NetworkInit(const network_init_desc_t *pInitDesc);

/**
 * \fn bool NetworkCtrlAdd(uint8_t ctrlId, const network_ctrl_desc_t *pCtrlDesc)
 * \brief Add a network controller 
 *
 * \param ctrlId network controller id
 * \param pCtrlDesc pointer to the controller descriptor
 * \return bool: true if operation successfull
 */
bool NetworkCtrlAdd(uint8_t ctrlId, const network_ctrl_desc_t *pCtrlDesc);

/**
 * \fn bool NetworkPortAdd(uint8_t portId, const network_port_desc_t *pPortDesc)
 * \brief Add a network port 
 *
 * \param portId network port id
 * \param pPortDesc pointer to the port descriptor
 * \return bool: true if operation successfull
 */
bool NetworkPortAdd(uint8_t portId, const network_port_desc_t *pPortDesc);

/**
 * \fn void NetworkCtrlArpDecayProcess(uint8_t ctrlId)
 * \brief Network controller arp decay process
 *
 * \param ctrlId network controller id
 * \return void
 */
void NetworkCtrlArpDecayProcess(uint8_t ctrlId);

/**
 * \fn void NetworkCtrlRxProcess(uint8_t ctrlId)
 * \brief Network controller reception process
 *
 * \param ctrlId network controller id
 * \return void
 */
void NetworkCtrlRxProcess(uint8_t ctrlId);

/**
 * \fn void NetworkCtrlTxProcess(uint8_t ctrlId)
 * \brief Network controller transmission process
 *
 * \param ctrlId network controller id
 * \return void
 */
void NetworkCtrlTxProcess(uint8_t ctrlId);

/**
 * \fn void NetworkCtrlMainProcess(uint8_t ctrlId)
 * \brief Network controller main process
 *
 * \param ctrlId network controller id
 * \return void
 */
void NetworkCtrlMainProcess(uint8_t ctrlId);

/**
 * \fn bool NetworkCtrlAddArpEntry(uint8_t ctrlId, const uint8_t *pIpAddress, const uint8_t *pMacAddress, bool hasDecay)
 * \brief Add an arp entry in a network controller arp table
 *
 * \param ctrlId network controller id
 * \param pIpAddress pointer to the entry ip address
 * \param pMacAddress pointer to the entry mac address
 * \param hasDecay arp entry is suseptible to decay or not
 * \return bool: true if entry was accepted
 */
bool NetworkCtrlAddArpEntry(uint8_t ctrlId, const uint8_t *pIpAddress, const uint8_t *pMacAddress, bool hasDecay);

/**
 * \fn bool NetworkCtrlForceRequestARP(uint8_t ctrlId, const uint8_t *pIpAddress)
 * \brief Force an arp request to a given ip address
 *
 * \param ctrlId network controller id
 * \param pIpAddress pointer to the target ip address
 * \return bool: true if request is sent
 */
bool NetworkCtrlForceRequestARP(uint8_t ctrlId, const uint8_t *pIpAddress);

/**
 * \fn bool NetworkCtrlIsArpValid(uint8_t ctrlId, const uint8_t *pIpAddress)
 * \brief Returns the arp entry validity of a given ip address
 *
 * \param ctrlId network controller id
 * \param pIpAddress pointer to the concerned ip address
 * \return bool: true if the corresponding arp entry is valid
 */
bool NetworkCtrlIsArpValid(uint8_t ctrlId, const uint8_t *pIpAddress);

/**
 * \fn bool NetworkCtrlSendPingIcmp(uint8_t ctrlId, const uint8_t *pIpAddress)
 * \brief Send an icmp echo request to a given ip (ip must be arp resolved beforehand)
 *
 * \param ctrlId network controller id
 * \param pIpAddress recipient ip address
 * \return bool: true if successfully sent
 */
bool NetworkCtrlSendPingIcmp(uint8_t ctrlId, const uint8_t *pIpAddress);

/**
 * \fn bool NetworkCtrlCheckPingReply(uint8_t ctrlId, uint32_t *pDelayValue)
 * \brief Returns if we received a reply to the last icmp echo request 
 *
 * \param ctrlId network controller id
 * \param pDelayValue pointer to contain the request-response delay (optional, valid only when function returns true)
 * \return bool: true if a reply was received
 */
bool NetworkCtrlCheckPingReply(uint8_t ctrlId, uint32_t *pDelayValue);

/**
 * \fn uint32_t NetworkPortTxFreeSpace(uint8_t portId)
 * \brief Returns the free space in a network port transmit fifo
 *
 * \param portId network port id
 * \return uint32_t: amount of free space (bytes)
 */
uint32_t NetworkPortTxFreeSpace(uint8_t portId);

/**
 * \fn bool NetworkPortIsTxEmpty(uint8_t portId)
 * \brief Indicates if a network port had data to send
 *
 * \param portId network port id
 * \return bool: true if there is no data
 */
bool NetworkPortIsTxEmpty(uint8_t portId);

/**
 * \fn bool NetworkPortSendByte(uint8_t portId, uint8_t data, const uint8_t *pIpDest)
 * \brief Send a byte of data through a network port
 *
 * \param portId network port id
 * \param data data to send
 * \param pIpDest recipient ip address (optional)
 * \return bool: true if stored successfully in the send fifo
 */
bool NetworkPortSendByte(uint8_t portId, uint8_t data, const uint8_t *pIpDest);

/**
 * \fn bool NetworkPortSendString(uint8_t portId, const char *str, const uint8_t *pIpDest)
 * \brief Send a string through a network port
 *
 * \param portId network port id
 * \param str string to send (\0 terminated)
 * \param pIpDest recipient ip address (optional)
 * \return bool: true if stored successfully in the send fifo
 */
bool NetworkPortSendString(uint8_t portId, const char *str, const uint8_t *pIpDest);

/**
 * \fn bool NetworkPortSendBuff(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest)
 * \brief Send a buffer through a network port
 *
 * \param portId network port id
 * \param pBuffer pointeur to the data buffer to send
 * \param buffSize buffer size
 * \param pIpDest recipient ip address (optional)
 * \return bool: true if stored successfully in the send fifo
 */
bool NetworkPortSendBuff(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest);

/**
 * \fn bool NetworkPortIsRxEmpty(uint8_t portId)
 * \brief Indicates if a network port has received data or not
 *
 * \param portId network port id
 * \return bool: true if there is no data
 */
bool NetworkPortIsRxEmpty(uint8_t portId);

/**
 * \fn bool NetworkPortReadByte(uint8_t portId, uint8_t *pBuffer)
 * \brief Read a byte from a network port (Port must be virtual com)
 *
 * \param portId network port id
 * \param pBuffer pointeur to the buffer to contain the data
 * \return bool: true if a byte was read and stored in the buffer
 */
bool NetworkPortReadByte(uint8_t portId, uint8_t *pBuffer);

/**
 * \fn bool NetworkPortReadBuff(uint8_t portId, uint8_t *pBuffer, uint16_t *pDataSize, uint16_t buffSize, uint8_t *pSrcIp)
 * \brief Read data from a network port
 *
 * \param portId network port id
 * \param pBuffer pointeur to the buffer to contain the data
 * \param pDataSize pointer to contain the size of the data read
 * \param buffSize size of the buffer
 * \param srcIp pointer to contain source ip address (optional)
 * \return bool: true if data was read and stored in the buffer
 */
bool NetworkPortReadBuff(uint8_t portId, uint8_t *pBuffer, uint16_t *pDataSize, uint16_t buffSize, uint8_t *pSrcIp);

/**
 * \fn uint8_t *NetworkCtrlGetMacAddr(uint8_t ctrlId)
 * \brief Returns the current mac address of a network controller
 *
 * \param ctrlId network controller id
 * \return uint8_t*: pointer to the mac address
 */
uint8_t *NetworkCtrlGetMacAddr(uint8_t ctrlId);

/**
 * \fn bool NetworkCtrlSetMacAddr(uint8_t ctrlId, const uint8_t *pNewMacAddr)
 * \brief Modify the mac address of a network controller /!\ WARNING /!\ THIS FUNCTION REQUIRES A RESET OF THE NETWORK PHY
 *
 * \param ctrlId network controller id
 * \param pNewMacAddr poiner to the new mac address 
 * \return bool: true if modified successfully
 */
bool NetworkCtrlSetMacAddr(uint8_t ctrlId, const uint8_t *pNewMacAddr);

/**
 * \fn uint8_t *NetworkCtrlGetIpAddress(uint8_t ctrlId)
 * \brief Returns the current ip address of a network controller
 *
 * \param ctrlId network controller id
 * \return uint8_t *: pointer to the ip address
 */
uint8_t *NetworkCtrlGetIpAddress(uint8_t ctrlId);

/**
 * \fn bool NetworkCtrlSetIpAddress(uint8_t ctrlId, const uint8_t *pNewIpAddr)
 * \brief Modify the ip address of a network controller /!\ WARNING /!\ MIGHT CHANGE THE SUB NETWORK
 *
 * \param ctrlId network controller id
 * \param pNewIpAddr pointer to the new ip address
 * \return bool: true if modified successfully
 */
bool NetworkCtrlSetIpAddress(uint8_t ctrlId, const uint8_t *pNewIpAddr);

/**
 * \fn uint8_t *NetworkCtrlGetSubnetMask(uint8_t ctrlId)
 * \brief Returns the current subnet mask of a network controller
 *
 * \param ctrlId network controller id
 * \return uint8_t *: pointer to the subnetmask
 */
uint8_t *NetworkCtrlGetSubnetMask(uint8_t ctrlId);

/**
 * \fn bool NetworkCtrlSetSubnetMask(uint8_t ctrlId, uint8_t const *pNewSubnetMask)
 * \brief Modify the subnet mask of a network controller
 *
 * \param ctrlId network controller id
 * \param pNewSubnetMask pointer to the new subnet mask
 * \return bool: true if modified successfully
 */
bool NetworkCtrlSetSubnetMask(uint8_t ctrlId, uint8_t const *pNewSubnetMask);

/**
 * \fn uint8_t *NetworkPortGetDstIpAddress(uint8_t portId)
 * \brief Return a network port current dest ip address
 *
 * \param portId network port id
 * \return uint8_t *: pointer to the ip address
 */
uint8_t *NetworkPortGetDstIpAddress(uint8_t portId);

/**
 * \fn bool NetworkPortSetDstIpAddress(uint8_t portId, const uint8_t *pNewIpAddr)
 * \brief Modify a network port dest ip address
 *
 * \param portId network port id
 * \param pNewIpAddr pointer to the new dest ip address
 * \return bool: true if ip address successfully modified
 */
bool NetworkPortSetDstIpAddress(uint8_t portId, const uint8_t *pNewIpAddr);

/**
 * \fn uint16_t NetworkPortGetInPortNb(uint8_t portId)
 * \brief Return a network port current local port
 *
 * \param portId network port id
 * \return uint16_t: network port local port value
 */
uint16_t NetworkPortGetInPortNb(uint8_t portId);

/**
 * \fn bool NetworkPortSetInPortNb(uint8_t portId, uint16_t newInPortNb)
 * \brief Modify a network port local port
 *
 * \param portId network port id
 * \param newInPortNb new local port value
 * \return bool: true if local port successfully modified
 */
bool NetworkPortSetInPortNb(uint8_t portId, uint16_t newInPortNb);

/**
 * \fn uint16_t NetworkPortGetOutPortNb(uint8_t portId)
 * \brief Return a network port current distant port
 *
 * \param portId network port id
 * \return uint16_t: network port distant port value
 */
uint16_t NetworkPortGetOutPortNb(uint8_t portId);

/**
 * \fn bool NetworkPortSetOutPortNb(uint8_t portId, uint16_t newOutPortNb)
 * \brief Modify a network port distant port
 *
 * \param portId network port id
 * \param newOutPortNb new distant port value
 * \return bool: true if distant port successfully modified
 */
bool NetworkPortSetOutPortNb(uint8_t portId, uint16_t newOutPortNb);

// *** End Definitions ***
#endif // _network_h