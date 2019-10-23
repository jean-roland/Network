/**
 * \file Network.c
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

//*** Libraries include ***
// Standard lib
#include <string.h>
// Custom lib
#include <Utils.h>
#include <MemAlloc.h>
#include <Fifo.h>
#include <Network.h>

// *** Definitions ***
// --- Private Types ---
typedef struct _network_msg {
    uint8_t DstIP[IP_ADDR_LENGTH]; // [4 bytes]
    uint16_t SrcPort; // [2 bytes]
    uint16_t DstPort; // [2 bytes]
    uint16_t DataSize; // [2 bytes]
    uint16_t HeaderSize; // [2 bytes]
} network_msg_info_t; // total: 12 bytes, 0 padding

typedef struct _arp_status {
    uint8_t IsInitialised: 1; // [1 bit]
    uint8_t IsValid: 1; // [1 bit]
    uint8_t IsRequested: 1; // [1 bit]
    uint8_t HasDecay: 1; // [1 bit]
    uint8_t Unused: 4; // [4 bits] Unused
} arp_status_t; // total: 1 byte, 4 bits of padding

typedef struct _arp_entry {
    uint32_t DecayTimer; // [4 bytes]
    uint8_t MacAddr[MAC_ADDR_LENGTH]; // [6 bytes]
    uint8_t IpAddr[IP_ADDR_LENGTH]; // [4 bytes]
    arp_status_t Status; // [1 byte]
} arp_entry_t; // total: 16 bytes, 1 byte of padding

typedef struct _ip_msg_desc {
    uint16_t MsgSize; // [2 bytes]
    uint8_t IpAddr[IP_ADDR_LENGTH]; // [4 bytes]
} network_msg_desc_t; // total: 8 bytes, 2 bytes of padding

typedef struct _network_port_info {
    const network_port_desc_t *pDesc;
    void* pFifoRxMsg;
    void* pFifoRxMsgDesc;
    void* pFifoTxMsg;
    void* pFifoTxMsgDesc;
    uint32_t TimerRequestARP;
    uint16_t InPortNb;
    uint16_t OutPortNb;
    uint8_t CounterARP;
    bool IsVirtualComRx;
    bool IsVirtualComTx;
    uint8_t DstIpAddr[IP_ADDR_LENGTH];
} network_port_info_t;

typedef struct _network_ctrl_info {
    const network_ctrl_desc_t *pDesc;
    arp_entry_t *pArpArray;
    uint32_t TimerDecayARP;
    uint32_t IcmpReplyDelay; // Contains the delay between ICMP echo and its response (valid only when ICMPReplyReceived is true)
    bool IcmpReplyReceived; // Indicates if we received an answer to the last ICMP echo sent by this controller
    uint8_t IpAddr[IP_ADDR_LENGTH];
    uint8_t SubnetMask[IP_ADDR_LENGTH];
    uint8_t MacAddr[MAC_ADDR_LENGTH];
} network_ctrl_info_t;

typedef struct _network_module_info {
    const network_init_desc_t *pInitDesc; // Module initialisation descriptor
    network_ctrl_info_t *pCtrlInfoList; // Network controller list
    network_port_info_t *pPortInfoList; // Network port list
    uint8_t *pBuffer; // Tx/Rx buffer
} network_module_info_t;

// --- Private Constants ---
#define NETWORK_ICMP_DATA_SIZE 14 // Arbritary data size value for icmp packets
#define NETWORK_ARP_REQ_GROUP_NB 3 // arp request number in a request group
#define NETWORK_ARP_REQUEST_COOLDOWN 2000 // Max time between two arp requests
#define NETWORK_ARP_DECAY_COOLDOWN 1000 // Min time between two arp table decay refresh
#define NETWORK_ARP_DECAY_TIME 60000 // Max time without activity before decaying an arp entry

// --- Private Function Prototypes ---
// Useful functions
static void NetworkInitMsgInfo(network_msg_info_t *pMsgInfo, const uint8_t *pIpAddr, uint16_t srcPort, uint16_t dstPort, uint16_t dataSize);
static bool NetworkIsIpValid(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask);
static bool NetworkIsIpBroadcast(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask);
static bool NetworkAcceptIncIpPacket(uint8_t ctrlId, ipv4_header_t *pIpHeader);
// Arp functions
static arp_entry_t *NetworkGetArpEntry(uint8_t ctrlId, const uint8_t *pIpAddr);
static arp_entry_t *NetworkCreateArpEntry(uint8_t ctrlId);
static bool NetworkRequestArp(uint8_t ctrlId, const uint8_t *pIpAddr);
static bool NetworkStoreArp(uint8_t ctrlId, const uint8_t *pIpAddr, const uint8_t *pMacAddr, bool hasDecay);
static bool NetworkUpdateArpTable(uint8_t ctrlId, const uint8_t *pSourceIp, const uint8_t *pSourceMac, bool hasDecay);
static bool NetworkProcessArpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize);
// Data send functions
static bool NetworkSendEthPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo);
static bool NetworkSendIpPacket(uint8_t ctrlId, uint8_t protocol, uint8_t *pBuffer, network_msg_info_t msgInfo);
static bool NetworkSendUdpPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo);
// Icmp functions
static uint16_t NetworkIcmpChecksum(const uint16_t *pBuffer, uint16_t buffSize);
static uint16_t NetworkIcmpLength(uint8_t *pHeader, uint16_t ipMsgSize);
static bool NetworkSendIcmpEchoRequest(uint8_t ctrlId, const uint8_t *pIpAdress);
static bool NetworkProcessIcmpEchoRequest(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize);
static bool NetworkProcessIcmpEchoReply(uint8_t ctrlId);
static bool NetworkProcessIcmpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize);
// Store data functions
static uint8_t *NetworkDecodeUdpPacket(uint8_t *pBuffer, uint16_t *pDataSize, uint16_t *pDestPort);
static bool NetworkStoreSendData(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest);
static bool NetworkStoreIncMsg(const uint8_t *pBuffer, uint16_t buffSize, uint16_t destPort, uint8_t protocol, uint8_t *pIpSrc);
// Process functions
static bool NetworkProcessSendMsg(uint8_t portId, uint8_t *pBuffer);
static bool NetworkProcessIpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize);
static bool NetworkProcessEthPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize);
// Check functions
static bool NetworkCtrlValid(uint8_t ctrlId);
static bool NetworkPortValid(uint8_t portId);
static bool NetworkCheckGenItfc(const network_gen_itfc_t *pGenItfc);
static bool NetworkCheckComItfc(const network_com_itfc_t *pComItfc);

// --- Private Variables ---
static network_module_info_t NetworkInfo;

// *** End Definitions ***

// *** Private Functions ***

/**
 * \fn static void NetworkInitMsgInfo(network_msg_info_t *pMsgInfo, const uint8_t *pIpAddr, uint16_t srcPort, uint16_t dstPort, uint16_t dataSize)
 * \brief Initialize a network message info structure
 *
 * \param pMsgInfo pointer to the message info structure to fill
 * \param pIpAddr pointer to destination ip address
 * \param srcPort source port
 * \param dstPort destination port
 * \param dataSize message data size
 * \return void
 */
static void NetworkInitMsgInfo(network_msg_info_t *pMsgInfo, const uint8_t *pIpAddr, uint16_t srcPort, uint16_t dstPort, uint16_t dataSize) {
    pMsgInfo->SrcPort = srcPort;
    pMsgInfo->DstPort = dstPort;
    pMsgInfo->DataSize = dataSize;
    pMsgInfo->HeaderSize = 0;
    memcpy(pMsgInfo->DstIP, pIpAddr, IP_ADDR_LENGTH);
}

/**
 * \fn static bool NetworkIsIpValid(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask)
 * \brief Returns the validity of an ip address for a given subnet
 *
 * \param pIpAddr pointer to the ip address to check
 * \param pRefIpAddr pointer to the ip address to use as reference
 * \param pSubnetMask pointer to the subnet mask
 * \return bool: true if ip address valid for this subnet
 */
static bool NetworkIsIpValid(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask) {
    uint8_t maskedIpAddr[IP_ADDR_LENGTH] = {0,0,0,0};
    uint8_t maskedRefIpAddr[IP_ADDR_LENGTH] = {0,0,0,0};

    // Mask the address
    for (uint8_t i = 0; i < IP_ADDR_LENGTH; i++) {
        maskedIpAddr[i] = pIpAddr[i] & pSubnetMask[i];
        maskedRefIpAddr[i] = pRefIpAddr[i] & pSubnetMask[i];
    }
    // Compare them
    if (memcmp(maskedRefIpAddr, maskedIpAddr, sizeof(maskedIpAddr)) == 0) {
        return true;
    } else {
        return false;
    }
}

/**
 * \fn static bool NetworkIsIpBroadcast(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask)
 * \brief Indicates if an ip is a broadcast address
 *
 * \param pIpAddr pointer to the ip address to check
 * \param pRefIpAddr pointer to the ip address to use as reference
 * \param pSubnetMask pointer to the subnet mask
 * \return bool: true if ip address is broadcast address
 */
static bool NetworkIsIpBroadcast(const uint8_t *pIpAddr, const uint8_t *pRefIpAddr, const uint8_t *pSubnetMask) {
    uint8_t broadcastIpAddr[IP_ADDR_LENGTH] = {0,0,0,0};

    // Calculate the broadcast address
    for (uint8_t i = 0; i < IP_ADDR_LENGTH; i++) {
        broadcastIpAddr[i] = pRefIpAddr[i] | ~pSubnetMask[i];
    }
    // Compare them
    if (memcmp(broadcastIpAddr, pIpAddr, sizeof(broadcastIpAddr)) == 0) {
        return true;
    } else {
        return false;
    }
}

/**
 * \fn static bool NetworkAcceptIncIpPacket(uint8_t ctrlId, ipv4_header_t *pIpHeader)
 * \brief Returns if an incoming ip packet has to be processed or not
 *
 * \param ctrlId: network controller id
 * \param pIpHeader: pointer to ipv4 packet header
 * \return bool: true if we must process the packet
 */
static bool NetworkAcceptIncIpPacket(uint8_t ctrlId, ipv4_header_t *pIpHeader) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

    // Check if sender has valid ip for this subnet
    if (NetworkIsIpValid(pIpHeader->srcIp, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
        // Check if this packet concerns us
        bool isBroadcast = NetworkIsIpBroadcast(pIpHeader->dstIp, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask);
        bool destIsThis = (memcmp(pIpHeader->dstIp, pNetworkCtrl->IpAddr, IP_ADDR_LENGTH) == 0);
        if (isBroadcast || destIsThis) {
            return true;
        }
    }
    return false;
}

/**
 * \fn static arp_entry_t *NetworkGetArpEntry(uint8_t ctrlId, const uint8_t *pIpAddr)
 * \brief Lookup for a given IP address in a network controller arp table
 *
 * \param ctrlId network controller id
 * \param pIpAddr pointer to the ip address to identify
 * \return arp_entry_t *: pointer to the corresponding arp entry (NULL if IP not found)
 */
static arp_entry_t *NetworkGetArpEntry(uint8_t ctrlId, const uint8_t *pIpAddr) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

    // Look for the ip address in the arp table
    for (uint8_t i = 0; i < pNetworkCtrl->pDesc->ArpEntryNb; i++) {
        if (pNetworkCtrl->pArpArray[i].Status.IsInitialised) {
            if (memcmp(pNetworkCtrl->pArpArray[i].IpAddr, pIpAddr, IP_ADDR_LENGTH) == 0) {
                return &(pNetworkCtrl->pArpArray[i]);
            }
        }
    }
    return NULL;
}

/**
 * \fn static arp_entry_t *NetworkCreateArpEntry(uint8_t ctrlId)
 * \brief Creates an arp entry
 *
 * \param ctrlInfo network controller id
 * \return arp_entry_t *: pointer to the new arp entry (NULL if it wasn't created)
 */
static arp_entry_t *NetworkCreateArpEntry(uint8_t ctrlId) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    uint8_t freeIdx;

    // Get first free slot in arp table
    for (freeIdx = 0; freeIdx < pNetworkCtrl->pDesc->ArpEntryNb; freeIdx++) {
        if (!pNetworkCtrl->pArpArray[freeIdx].Status.IsInitialised) {
            break;
        }
    }
    if (freeIdx < pNetworkCtrl->pDesc->ArpEntryNb) {
        // Get entry address and clear data
        return &(pNetworkCtrl->pArpArray[freeIdx]);
    } else {
        // Arp table full, can't create the request, notify error
        if (NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify != NULL) {
            NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify(NetworkInfo.pInitDesc->ErrorCode);
        }
        return NULL;
    }
}

/**
 * \fn static bool NetworkRequestArp(uint8_t ctrlId, const uint8_t *pIpAddr)
 * \brief Send an arp request
 *
 * \param ctrlId: network controller id
 * \param pIpAddr: pointer to the ip address to send the request to
 * \return bool: true if the request is sent
 */
static bool NetworkRequestArp(uint8_t ctrlId, const uint8_t *pIpAddr) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    arp_entry_t *pArpEntry = NetworkGetArpEntry(ctrlId, pIpAddr);

    if (pArpEntry == NULL) {
        // Create arp entry if needed
        pArpEntry = NetworkCreateArpEntry(ctrlId);
        if (pArpEntry != NULL) {
            memcpy(pArpEntry->IpAddr, pIpAddr, IP_ADDR_LENGTH);
            pArpEntry->Status.IsRequested = true;
            pArpEntry->Status.IsInitialised = true;
            pArpEntry->Status.IsValid = false;
            pArpEntry->Status.HasDecay = false;
            pArpEntry->DecayTimer = 0;
        } else {
            // Can't send if we can't create the entry
            return false;
        }
    }
    // Create arp request
    uint8_t msgBuffer[ETH_HEADER_SIZE + ARP_HEADER_SIZE]; // 42 bytes
    ethernet_header_t *pEthHeader = (ethernet_header_t *)msgBuffer;
    arp_header_t *pArpHeader = (arp_header_t *)(msgBuffer + ETH_HEADER_SIZE);

    // Fill mac address
    for (uint8_t i = 0; i < MAC_ADDR_LENGTH; i++) {
        pEthHeader->dstMac[i] = 0xFF;
        pEthHeader->srcMac[i] = pNetworkCtrl->MacAddr[i];
        pArpHeader->senderMac[i] = pNetworkCtrl->MacAddr[i];
        pArpHeader->targetMac[i] = 0x00;
    }
    // Fill ip address
    for (uint8_t i = 0; i < IP_ADDR_LENGTH; i++) {
        pArpHeader->senderIp[i] = pNetworkCtrl->IpAddr[i];
        pArpHeader->targetIp[i] = pIpAddr[i];
    }
    // Fill misc
    pEthHeader->lengthOrType = UtilsRotrUint16(0x0806, 8);
    pArpHeader->hardwareTpe = UtilsRotrUint16(0x0001, 8);
    pArpHeader->protocolType = UtilsRotrUint16(0x0800, 8);
    pArpHeader->hardwareLength = 6;
    pArpHeader->protocolLength = 4;
    pArpHeader->operation = UtilsRotrUint16(0x0001, 8); // Arp request
    // Arp entry invalidation
    pArpEntry->Status.IsValid = false;
    // Send packet
    return pNetworkCtrl->pDesc->ComInterface.MacCtrlSendMsg(pNetworkCtrl->pDesc->MacCtrlId, msgBuffer, sizeof(msgBuffer));
}

/**
 * \fn static bool NetworkStoreArp(uint8_t ctrlId, const uint8_t *pIpAddr, const uint8_t *pMacAddr, bool hasDecay)
 * \brief Store arp information
 *
 * \param ctrlId network controller id
 * \param pIpAddr pointer to the entry ip address
 * \param pMacAddr pointer to the entry mac address
 * \param hasDecay arp is subject or not to decay
 * \return bool: true if arp is stored
 */
static bool NetworkStoreArp(uint8_t ctrlId, const uint8_t *pIpAddr, const uint8_t *pMacAddr, bool hasDecay) {
    arp_entry_t *pArpEntry = NetworkGetArpEntry(ctrlId, pIpAddr);

    if (pArpEntry == NULL) {
        // Create arp entry if needed
        pArpEntry = NetworkCreateArpEntry(ctrlId);
        if (pArpEntry != NULL) {
            memcpy(pArpEntry->IpAddr, pIpAddr, IP_ADDR_LENGTH);
            memcpy(pArpEntry->MacAddr, pMacAddr, MAC_ADDR_LENGTH);
            pArpEntry->Status.IsRequested = true;
            pArpEntry->Status.IsInitialised = true;
            pArpEntry->Status.IsValid = true;
            pArpEntry->Status.HasDecay = hasDecay;
            pArpEntry->DecayTimer = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime();
            return true;
        } else {
            return false;
        }
    }
    else if (!pArpEntry->Status.IsValid) {
        // Complete arp entry
        memcpy(pArpEntry->MacAddr, pMacAddr, MAC_ADDR_LENGTH);
        pArpEntry->Status.IsValid = true;
        pArpEntry->Status.HasDecay = hasDecay;
        pArpEntry->DecayTimer = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime();
        return true;
    } else {
        // No modification if entry is valid
        return true;
    }
}

/**
 * \fn static bool NetworkUpdateArpTable(uint8_t ctrlId, const uint8_t *pSourceIp, const uint8_t *pSourceMac, bool hasDecay)
 * \brief Update the arp table with info from received packets
 *
 * \param ctrlId network controller id
 * \param pSourceIp pointer to sender ip address
 * \param pSourceMac pointer to sender mac address
 * \param hasDecay arp is subject or not to decay
 * \return bool: true if updated successfully
 */
static bool NetworkUpdateArpTable(uint8_t ctrlId, const uint8_t *pSourceIp, const uint8_t *pSourceMac, bool hasDecay) {
    arp_entry_t *pArpEntry = NetworkGetArpEntry(ctrlId, pSourceIp);

    // Store arp information if needed
    if ((pArpEntry == NULL) || (!pArpEntry->Status.IsValid)) {
        return NetworkStoreArp(ctrlId, pSourceIp, pSourceMac, hasDecay);
    } else {
        // Arp timer update
        pArpEntry->DecayTimer = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime();
        // Update mac address if needed
        if (memcmp(pArpEntry->MacAddr, pSourceMac, MAC_ADDR_LENGTH) != 0) {
            memcpy(pArpEntry->MacAddr, pSourceMac, MAC_ADDR_LENGTH);
        }
        return true;
    }
}

/**
 * \fn static bool NetworkProcessArpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize)
 * \brief Process incoming arp packets
 *
 * \param ctrlId: network controller id
 * \param pBuffer: pointer to the buffer to process
 * \param buffSize: size of the buffer
 * \return bool: true if arp packet was processed successfully
 */

 #include <stdio.h>

 static bool NetworkProcessArpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    ethernet_header_t *pEthHeader = (ethernet_header_t*) pBuffer;
    arp_header_t *pArpHeader = (arp_header_t *)(pBuffer + ETH_HEADER_SIZE);
    uint16_t operation = SWAP16(pArpHeader->operation);

    // Process only if ip address valid for the subnet
    if (NetworkIsIpValid(pArpHeader->senderIp, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
        switch(operation) {
            case ARP_REQUEST:
                // Check if arp concerns the controller
                if (memcmp(pArpHeader->targetIp, pNetworkCtrl->IpAddr, IP_ADDR_LENGTH) == 0) {
                    pArpHeader->operation = SWAP16(ARP_REPLY);
                    // Swap destination and source mac address
                    for (uint8_t i = 0; i < 6; i++) {
                        pEthHeader->dstMac[i] = pEthHeader->srcMac[i];
                        pEthHeader->srcMac[i] = pNetworkCtrl->MacAddr[i];
                        pArpHeader->targetMac[i] = pArpHeader->senderMac[i];
                        pArpHeader->senderMac[i] = pNetworkCtrl->MacAddr[i];
                    }
                    // Swap destination and source ip address
                    for (uint8_t i = 0; i < 4; i++) {
                        pArpHeader->targetIp[i] = pArpHeader->senderIp[i];
                        pArpHeader->senderIp[i] = pNetworkCtrl->IpAddr[i];
                    }
                    // Send reply
                    return pNetworkCtrl->pDesc->ComInterface.MacCtrlSendMsg(pNetworkCtrl->pDesc->MacCtrlId, pBuffer, buffSize);
                } else {
                    // Packet doesn't concern us
                    return true;
                }
            break;

            case ARP_REPLY:
                return NetworkStoreArp(ctrlId, pArpHeader->senderIp, pArpHeader->senderMac, false);
            break;

            default:
                // No process if unknown operation
                return true;
            break;
        }
    } else {
        return true;
    }
}

/**
 * \fn static bool NetworkSendEthPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo)
 * \brief Send an ethernet packet to the mac interface
 *
 * \param ctrlId  network controller id
 * \param pBuffer pointer to the buffer to send
 * \param msgInfo message network parameters
 * \return bool: true if packet is sent successfully
 */
static bool NetworkSendEthPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    arp_entry_t *pArpEntry = NetworkGetArpEntry(ctrlId, (uint8_t *)msgInfo.DstIP);
    ethernet_header_t *pEthHeader = (ethernet_header_t *) pBuffer;

    // Check if we know where to send the packet
    if (((pArpEntry != NULL) && pArpEntry->Status.IsValid) || NetworkIsIpBroadcast(msgInfo.DstIP, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
        // Set the source and destination mac adresses
        memcpy(pEthHeader->srcMac, pNetworkCtrl->MacAddr, MAC_ADDR_LENGTH);
        if (!NetworkIsIpBroadcast(msgInfo.DstIP, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask))
            memcpy(pEthHeader->dstMac, pArpEntry->MacAddr, MAC_ADDR_LENGTH);
        else
            memset(pEthHeader->dstMac, 0xFF, MAC_ADDR_LENGTH);
        // Network type 2 frame
        pEthHeader->lengthOrType = 0x0008; // UtilsRotrUint16(0x0800, 8);
        msgInfo.HeaderSize += (uint16_t)ETH_HEADER_SIZE;
        // Send packet
        return pNetworkCtrl->pDesc->ComInterface.MacCtrlSendMsg(pNetworkCtrl->pDesc->MacCtrlId, pBuffer, msgInfo.HeaderSize + msgInfo.DataSize);
    } else {
        return false;
    }
}

/**
 * \fn static bool NetworkSendIpPacket(uint8_t ctrlId, uint8_t protocol, uint8_t *pBuffer, network_msg_info_t msgInfo)
 * \brief Send an ip packet to the eth function
 *
 * \param ctrlId  network controller id
 * \param protocol ip message protocole (eg: udp)
 * \param pBuffer pointer to the buffer to send
 * \param msgInfo message network parameters
 * \return bool: true if packet is sent successfully
 */
static bool NetworkSendIpPacket(uint8_t ctrlId, uint8_t protocol, uint8_t *pBuffer, network_msg_info_t msgInfo) {
    ipv4_header_t *pIpHeader = (ipv4_header_t *)(pBuffer + ETH_HEADER_SIZE);
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

    // Fill ip header
    pIpHeader->ihl = 5; // internet header length (uint32_t)
    pIpHeader->version = 4; // ipv4
    pIpHeader->ecn = 0; // Do not support explicit congestion notification
    pIpHeader->dscp = 0; // Best effort
    pIpHeader->length = UtilsRotrUint16(((uint16_t)IPV4_HEADER_SIZE + msgInfo.DataSize + msgInfo.HeaderSize), 8); // Total size (data + header)
    pIpHeader->identification = 0; // No id
    pIpHeader->fragmentOffsetAndFlags = 64; // (2 << 5) + 0; // No fragment offset nor fragmentation
    pIpHeader->ttl = 128;  // Time to live (hop count)
    pIpHeader->protocol = protocol; // Ip message protocole
    pIpHeader->checksum = 0; // Packet checksum (hw calculated)
    memcpy(pIpHeader->srcIp, pNetworkCtrl->IpAddr, IP_ADDR_LENGTH); // Source ip address
    memcpy(pIpHeader->dstIp, msgInfo.DstIP, IP_ADDR_LENGTH); // Recipient ip address
    msgInfo.HeaderSize += (uint16_t)IPV4_HEADER_SIZE; // We take into account the ipv4 header
    // Send packet
    return NetworkSendEthPacket(ctrlId, pBuffer, msgInfo);
}

/**
 * \fn static bool NetworkSendUdpPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo)
 * \brief Send an udp packet to the ip function
 *
 * \param ctrlId  network controller id
 * \param pBuffer pointer to the buffer to send
 * \param msgInfo message network parameters
 * \return bool: true if packet is sent successfully
 */
static bool NetworkSendUdpPacket(uint8_t ctrlId, uint8_t *pBuffer, network_msg_info_t msgInfo) {
    udp_header_t *pUdpHeader = (udp_header_t *)(pBuffer + IPV4_HEADER_SIZE + ETH_HEADER_SIZE);

    pUdpHeader->srcPort = UtilsRotrUint16(msgInfo.SrcPort, 8); // Source port
    pUdpHeader->dstPort = UtilsRotrUint16(msgInfo.DstPort, 8); // Destination port
    pUdpHeader->length = UtilsRotrUint16((msgInfo.DataSize + (uint16_t)UDP_HEADER_SIZE), 8); // Total size (data + header)
    pUdpHeader->checksum = 0; // Packet checksum (hw calculated)
    msgInfo.HeaderSize = UDP_HEADER_SIZE; // We take into account the udp header

    return NetworkSendIpPacket(ctrlId, IP_PROT_UDP, pBuffer, msgInfo);
}

/**
 * \fn static uint16_t NetworkIcmpChecksum(const uint16_t *pBuffer, uint16_t buffSize)
 * \brief Return an icmp packet checksum
 *
 * \param pBuffer: pointer to the data buffer
 * \param buffSize: buffer size
 * \return uint16_t: icmp checksum
 */
static uint16_t NetworkIcmpChecksum(const uint16_t *pBuffer, uint16_t buffSize) {
    uint32_t checksum = 0;

    for (uint16_t i = 0; i < buffSize; i++, pBuffer++) {
        checksum += SWAP16(*pBuffer);
    }
    checksum = (checksum & 0xffff) + (checksum >> 16);
    return (uint16_t)(~checksum);
}

/**
 * \fn static uint16_t NetworkIcmpLength(uint8_t *pHeader, uint16_t ipMsgSize)
 * \brief Return an icmp packet length
 *
 * \param pHeader pointer to icmp header
 * \param ipMsgSize ip message size
 * \return uint16_t: icmp length
 */
static uint16_t NetworkIcmpLength(uint8_t *pHeader, uint16_t ipMsgSize) {
    uint16_t icmpMsgSize = (SWAP16(ipMsgSize) - (uint16_t)IPV4_HEADER_SIZE);

    // Add data if size is odd
    if ((icmpMsgSize % 2) > 0) {
         *(pHeader + icmpMsgSize) = 0;
         icmpMsgSize++;
    }
    return (icmpMsgSize / sizeof(uint16_t));
}

/**
 * \fn static bool NetworkSendIcmpEchoRequest(uint8_t ctrlId, const uint8_t *pIpAdress)
 * \brief Send an icmp echo request
 *
 * \param ctrlId network controller id
 * \param pIpAdress destination ip address
 * \return bool: true if the packet was sent
 */
static bool NetworkSendIcmpEchoRequest(uint8_t ctrlId, const uint8_t *pIpAdress) {
    uint8_t bufferRequest[ETH_HEADER_SIZE + IPV4_HEADER_SIZE + ICMP_HEADER_SIZE + NETWORK_ICMP_DATA_SIZE]; // 56 bytes
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    icmp_header_t *pIcmpHeader = (icmp_header_t *)(bufferRequest + ETH_HEADER_SIZE + IPV4_HEADER_SIZE);
    uint8_t *pIcmpData = bufferRequest + ETH_HEADER_SIZE + IPV4_HEADER_SIZE + ICMP_HEADER_SIZE;
    uint16_t msgLength;
    network_msg_info_t msgInfo;

    // Msg params init
    NetworkInitMsgInfo( &msgInfo, pIpAdress, 0, 0, NETWORK_ICMP_DATA_SIZE );
    // Reset received flag
    pNetworkCtrl->IcmpReplyReceived = false;
    // Fill icmp header
    pIcmpHeader->type = ICMP_ECHO_REQUEST;
    pIcmpHeader->code = 0;
    pIcmpHeader->cksum = 0;
    pIcmpHeader->id = 1;
    pIcmpHeader->seq = 1;
    memset(pIcmpData, 0x5, NETWORK_ICMP_DATA_SIZE);
    // Icmp checksum calculation
    msgLength = UtilsRotrUint16((sizeof(bufferRequest) - ETH_HEADER_SIZE), 8);
    pIcmpHeader->cksum = SWAP16(NetworkIcmpChecksum((uint16_t *)pIcmpHeader, NetworkIcmpLength((uint8_t *)pIcmpHeader, msgLength)));
    msgInfo.HeaderSize += (uint16_t)ICMP_HEADER_SIZE; // We take into account the icmp header
    // Get send time for delay calculation
    pNetworkCtrl->IcmpReplyDelay = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime();
    return NetworkSendIpPacket(ctrlId, IP_PROT_ICMP, bufferRequest, msgInfo);
}

/**
 * \fn static bool NetworkProcessIcmpEchoRequest(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize)
 * \brief Process icmp echo request packets
 *
 * \param ctrlId network controller id
 * \param pBuffer pointer to the buffer to process
 * \param buffSize buffer size
 * \return bool: true if processed successfully
 */
static bool NetworkProcessIcmpEchoRequest(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);
    ethernet_header_t *p_eth = (ethernet_header_t*) pBuffer;
    ipv4_header_t *pIpHeader = (ipv4_header_t *)(pBuffer + ETH_HEADER_SIZE);
    icmp_header_t *pIcmpHeader = (icmp_header_t *)(pBuffer + ETH_HEADER_SIZE + IPV4_HEADER_SIZE);

    // Fill icmp header
    pIcmpHeader->type = ICMP_ECHO_REPLY;
    pIcmpHeader->code = 0;
    pIcmpHeader->cksum = 0;
    // Icmp checksum calculation
    pIcmpHeader->cksum = SWAP16(NetworkIcmpChecksum((uint16_t *)pIcmpHeader, NetworkIcmpLength((uint8_t *)pIcmpHeader, pIpHeader->length)));
    // Swap destination and source ip address
    for (uint8_t idx = 0; idx < 4; idx++) {
        pIpHeader->dstIp[idx] = pIpHeader->srcIp[idx];
        pIpHeader->srcIp[idx] = pNetworkCtrl->IpAddr[idx];
    }
    // Swap destination and source mac address
    for (uint8_t idx = 0; idx < 6; idx++) {
        p_eth->dstMac[idx] = p_eth->srcMac[idx];
        p_eth->srcMac[idx] = pNetworkCtrl->MacAddr[idx];
    }
    // Send the echo_reply
    return pNetworkCtrl->pDesc->ComInterface.MacCtrlSendMsg(pNetworkCtrl->pDesc->MacCtrlId, pBuffer, buffSize);
}

/**
 * \fn static bool NetworkProcessIcmpEchoReply(uint8_t ctrlId)
 * \brief Process icmp reply packets
 *
 * \param ctrlId network controller id
 * \return bool: true if processed successfully
 */
static bool NetworkProcessIcmpEchoReply(uint8_t ctrlId) {
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

    // Note that we received our reply (reply duplicates filtering)
    if (!pNetworkCtrl->IcmpReplyReceived) {
        pNetworkCtrl->IcmpReplyDelay = UtilsDiffUint32(pNetworkCtrl->IcmpReplyDelay, NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime());
        pNetworkCtrl->IcmpReplyReceived = true;
    }
    return true;
}

/**
 * \fn static bool NetworkProcessIcmpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize)
 * \brief Process incoming icmp packets
 *
 * \param ctrlId network controller id
 * \param pBuffer pointer to the buffer to process
 * \param bufferSize buffer size
 * \return bool: true if processed successfully
 */
static bool NetworkProcessIcmpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize) {
    icmp_header_t *pIcmpHeader = (icmp_header_t *)(pBuffer + ETH_HEADER_SIZE + IPV4_HEADER_SIZE);

    switch(pIcmpHeader->type) {
        case ICMP_ECHO_REQUEST:
            return NetworkProcessIcmpEchoRequest(ctrlId, pBuffer, buffSize);
        break;

        case ICMP_ECHO_REPLY:
            return NetworkProcessIcmpEchoReply(ctrlId);
        break;

        default:
            // No process if unknown type
            return true;
        break;
    }
}

/**
 * \fn static uint8_t *NetworkDecodeUdpPacket(uint8_t *pBuffer, uint16_t *pDataSize, uint16_t *pDestPort)
 * \brief Decode an udp packet
 *
 * \param pBuffer: pointer to the buffer to decode
 * \param pDataSize: pointer to contain packet data size
 * \param pDestPort: pointer to contain destination port
 * \return uint8_t *: pointer to the packet data
 */
static uint8_t *NetworkDecodeUdpPacket(uint8_t *pBuffer, uint16_t *pDataSize, uint16_t *pDestPort) {
    udp_header_t *pUpdHeader = (udp_header_t *)(pBuffer + ETH_HEADER_SIZE + IPV4_HEADER_SIZE);

    // Udp header decoding
    pUpdHeader->srcPort = UtilsRotrUint16(pUpdHeader->srcPort, 8);
    pUpdHeader->dstPort = UtilsRotrUint16(pUpdHeader->dstPort, 8);
    *pDestPort = pUpdHeader->dstPort;
    *pDataSize = (UtilsRotrUint16(pUpdHeader->length, 8) - (uint16_t)UDP_HEADER_SIZE);
    // Send back data pointer
    return pBuffer + (ETH_HEADER_SIZE + IPV4_HEADER_SIZE + UDP_HEADER_SIZE);
}

/**
 * \fn static bool NetworkStoreSendData(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest)
 * \brief Store data into the send fifo
 *
 * \param portId network port id
 * \param pBuffer pointer to the send buffer
 * \param buffSize send buffer size
 * \param pIpDest recipient ip address (optional)
 * \return bool: true if stored successfully
 */
static bool NetworkStoreSendData(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest) {
    // Check if we can store the message descriptor ahead of time
    if ((NetworkInfo.pPortInfoList[portId].IsVirtualComTx) || (FifoFreeSpace(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc) > 0)) {
        // Try to store the buffer in the main fifo
        bool sendStatus = FifoWrite(NetworkInfo.pPortInfoList[portId].pFifoTxMsg, pBuffer, buffSize);
        if (sendStatus && !NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
            // Try to store the descriptor
            network_msg_desc_t msgDesc = {.MsgSize = buffSize, .IpAddr = {0,0,0,0}};
            // Check if dest ip defined
            if (pIpDest != NULL) {
                memcpy(msgDesc.IpAddr, pIpDest, IP_ADDR_LENGTH);
            }
            sendStatus &= FifoWrite(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc, &msgDesc, 1);
        }
        return sendStatus;
    }
    return false;
}

/**
 * \fn static bool NetworkStoreIncMsg(const uint8_t *pBuffer, uint16_t buffSize, uint16_t destPort, uint8_t protocol, uint8_t *pIpSrc)
 * \brief Store an incoming message
 *
 * \param pBuffer pointer to the message data
 * \param buffSize buffer size
 * \param destPort destination port
 * \param protocol message ip protocol
 * \param pIpSrc pointer to the sender ip address
 * \return bool: true if stored successfully
 */
static bool NetworkStoreIncMsg(const uint8_t *pBuffer, uint16_t buffSize, uint16_t destPort, uint8_t protocol, uint8_t *pIpSrc) {
    bool storeStatus = true;

    // Parse all instantiated network ports
    for (uint8_t portId = 0; portId < NetworkInfo.pInitDesc->PortNb; portId++) {
        // Check port number and protocol
        if ((destPort == NetworkInfo.pPortInfoList[portId].InPortNb) && (protocol == NetworkInfo.pPortInfoList[portId].pDesc->Protocol)) {
            // Check if we can store the message descriptor ahead of time
            if ((NetworkInfo.pPortInfoList[portId].IsVirtualComRx) || (FifoFreeSpace(NetworkInfo.pPortInfoList[portId].pFifoRxMsgDesc) > 0)) {
                // Try to store the buffer in the main fifo
                storeStatus = FifoWrite(NetworkInfo.pPortInfoList[portId].pFifoRxMsg, pBuffer, buffSize);
                if (storeStatus && !NetworkInfo.pPortInfoList[portId].IsVirtualComRx) {
                    // Try to store the descriptor
                    network_msg_desc_t msgDesc = {.MsgSize = buffSize, .IpAddr = {0,0,0,0}};
                    memcpy(msgDesc.IpAddr, pIpSrc, IP_ADDR_LENGTH);
                    storeStatus &= FifoWrite(NetworkInfo.pPortInfoList[portId].pFifoRxMsgDesc, &msgDesc, 1);
                }
            } else {
                storeStatus = false;
            }
        }
    }
    return storeStatus;
}

/**
 * \fn static bool NetworkProcessSendMsg(uint8_t portId, uint8_t *pBuffer)
 * \brief Process and send stored messages or request arp if needed
 *
 * \param portId: network port id
 * \param pMessage: pointer to the transmit buffer
 * \return bool: true if request processed successfully
 */
static bool NetworkProcessSendMsg(uint8_t portId, uint8_t *pBuffer) {
    uint8_t destIp[IP_ADDR_LENGTH] = {0,0,0,0};

    // Get message info
    uint16_t msgSize = 0;
    if (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
        // Attempt to read message descriptor
        network_msg_desc_t msgDesc;
        if (FifoRead(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc, &msgDesc, 1, false)) {
            msgSize = msgDesc.MsgSize;
            // Send to descriptor dest ip address if valid
            if (memcmp(destIp, msgDesc.IpAddr, IP_ADDR_LENGTH) != 0) {
                memcpy(destIp, msgDesc.IpAddr, IP_ADDR_LENGTH);
            } else { // Send to default dest ip address otherwise
                memcpy(destIp, NetworkInfo.pPortInfoList[portId].DstIpAddr, IP_ADDR_LENGTH);
            }
        } else {
            return false;
        }
    } else {
        // Virtual com port: Get as much data as possible and send to default dest ip address
        msgSize = (uint16_t)FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoTxMsg);
        memcpy(destIp, NetworkInfo.pPortInfoList[portId].DstIpAddr, IP_ADDR_LENGTH);
    }
    // Message size limitation
    msgSize = (msgSize < ETHERNET_MAX_DATA_SIZE) ? msgSize : ETHERNET_MAX_DATA_SIZE;

    // Send message or arp
    uint8_t ctrlId = NetworkInfo.pPortInfoList[portId].pDesc->NetworkCtrlId;
    uint32_t *pTimerARP = &(NetworkInfo.pPortInfoList[portId].TimerRequestARP);
    network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

    // Send only if dest ip valid for this subnet
    if (NetworkIsIpValid(destIp, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
        // Formatting message info
        network_msg_info_t msgInfo;
        NetworkInitMsgInfo(&msgInfo, destIp, NetworkInfo.pPortInfoList[portId].InPortNb, NetworkInfo.pPortInfoList[portId].OutPortNb, msgSize);
        // Check arp status for dest ip
        arp_entry_t *pArpEntry = NetworkGetArpEntry(ctrlId, (uint8_t *)msgInfo.DstIP);
        // Message is broadcast or arp valid, we can send the message
        if (NetworkIsIpBroadcast(msgInfo.DstIP, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask) || ((pArpEntry != NULL) && pArpEntry->Status.IsValid)) {
            // Attempt to read message data
            if (FifoRead(NetworkInfo.pPortInfoList[portId].pFifoTxMsg, pBuffer + NETWORK_HEADER_SIZE, msgSize, false)) {
                // Attempt to send message
                if (NetworkSendUdpPacket(ctrlId, pBuffer, msgInfo)) {
                    FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsg, msgSize);
                    if (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
                        FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc, 1);
                    }
                } else {
                    return false;
                }
            } else {
                // Critical error, mismatched or corrupted fifo
                return false;
            }
        // Arp doesn't exist or invalid, send a request every so often (to avoid arp saturation)
        } else if ((pArpEntry == NULL) || ((!pArpEntry->Status.IsValid) && (NetworkInfo.pInitDesc->GenInterface.pFnTimerIsPassed(*pTimerARP)))) {
            // Send a group of ARP requests
            if (NetworkInfo.pPortInfoList[portId].CounterARP < NETWORK_ARP_REQ_GROUP_NB) {
                NetworkInfo.pPortInfoList[portId].CounterARP++;
                *pTimerARP = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime() + NETWORK_ARP_REQUEST_COOLDOWN;
            } else {
                NetworkInfo.pPortInfoList[portId].CounterARP = 0;
                // No ARP answer, we drop the packet
                FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsg, msgSize);
                if (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
                    FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc, 1);
                }
            }
            // Request arp
            NetworkRequestArp(ctrlId, (uint8_t *)msgInfo.DstIP);
        }
    } else { // if ip invalid, trash the message
        FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsg, msgSize);
        if (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
            FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc, 1);
        }
        return false;
    }
    return true;
}

/**
 * \fn static bool NetworkProcessIpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize)
 * \brief Process incoming ip packets
 *
 * \param ctrlId: network controller id
 * \param pBuffer: pointer to the buffer to process
 * \param buffSize: buffer size
 * \return bool: true if processed successfully
 */
static bool NetworkProcessIpPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize) {
    ipv4_header_t *pIpHeader = (ipv4_header_t *)(pBuffer + ETH_HEADER_SIZE);
    ethernet_header_t *pEthHeader = (ethernet_header_t *)(pBuffer);

    // Process only if packet is accepted
    if (NetworkAcceptIncIpPacket(ctrlId, pIpHeader)) {
        // Update arp table with new data
        NetworkUpdateArpTable(ctrlId, pIpHeader->srcIp, pEthHeader->srcMac, false);
        // Process packet
        switch (pIpHeader->protocol) {
            case IP_PROT_ICMP:
                // Process icmp packet
                return NetworkProcessIcmpPacket(ctrlId, pBuffer, buffSize);
            break;

            case IP_PROT_UDP: {
                // Decode udp packet
                uint16_t msgSize = 0;
                uint16_t destPort = 0;
                uint8_t *pMsgData = NetworkDecodeUdpPacket(pBuffer, &msgSize, &destPort);
                // Store message
                return NetworkStoreIncMsg(pMsgData, msgSize, destPort, IP_PROT_UDP, pIpHeader->srcIp);
            }
            break;

            default:
                // Unknown protocol
                return true;
            break;
        }
    } else {
        return true;
    }
}

/**
 * \fn static bool NetworkProcessEthPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize)
 * \brief Process incoming ethernet packets
 *
 * \param ctrlId network controller id
 * \param pBuffer pointer to the buffer to process
 * \param buffSize buffer size
 * \return bool: true if processed successfully
 */
static bool NetworkProcessEthPacket(uint8_t ctrlId, uint8_t *pBuffer, uint16_t buffSize) {
    ethernet_header_t *pEthHeader = (ethernet_header_t*)(pBuffer);
    uint16_t protocol = SWAP16(pEthHeader->lengthOrType);

    switch (protocol) {
        case ETH_PROT_ARP:
            // Process arp packet
            return NetworkProcessArpPacket(ctrlId, pBuffer, buffSize);
        break;

        case ETH_PROT_IPV4:
            // Process ip packet
            return NetworkProcessIpPacket(ctrlId, pBuffer, buffSize);
        break;

        default:
            // Unknown protocol
            return true;
        break;
    }
}

/**
 * \fn static bool NetworkCtrlValid(uint8_t ctrlId)
 * \brief Check a network controller id validity
 *
 * \param ctrlId network controller id
 * \return bool: true if valid
 */
static bool NetworkCtrlValid(uint8_t ctrlId) {
    return ((ctrlId < NetworkInfo.pInitDesc->CtrlNb) && (NetworkInfo.pCtrlInfoList[ctrlId].pDesc != NULL));
}

/**
 * \fn static bool NetworkPortValid(uint8_t portId)
 * \brief Check a network port id validity
 *
 * \param portId network port id
 * \return bool: true if valid
 */
static bool NetworkPortValid(uint8_t portId) {
    return ((portId < NetworkInfo.pInitDesc->PortNb) && (NetworkInfo.pPortInfoList[portId].pDesc != NULL));
}

/**
 * \fn static bool NetworkCheckGenItfc(const network_gen_itfc_t *pGenItfc)
 * \brief Check the validity of the module descriptor generic interface
 *
 * \param pGenItfc pointer to the generic interface
 * \return bool: true if valid
 */
static bool NetworkCheckGenItfc(const network_gen_itfc_t *pGenItfc) {
    return ((pGenItfc->pFnTimerGetTime != NULL) && (pGenItfc->pFnTimerIsPassed != NULL));
}

/**
 * \fn static bool NetworkCheckComItfc(const network_com_itfc_t *pComItfc)
 * \brief Check the validity of a network controller communication interface
 *
 * \param pGenItfc pointer to the generic interface
 * \return bool: true if valid
 */
static bool NetworkCheckComItfc(const network_com_itfc_t *pComItfc) {
    return ((pComItfc->MacCtrlGetMsg != NULL) && (pComItfc->MacCtrlHasMsg != NULL) && (pComItfc->MacCtrlSendMsg != NULL) && (pComItfc->MacCtrlSetMacAddr != NULL));
}



// *** Public Functions ***

bool NetworkInit(const network_init_desc_t *pInitDesc) {
    if ((pInitDesc != NULL) && NetworkCheckGenItfc(&pInitDesc->GenInterface)) {
        // Copy desc address
        NetworkInfo.pInitDesc = pInitDesc;
        // Info structures memory allocation
        NetworkInfo.pCtrlInfoList = MemAllocCalloc((uint32_t)sizeof(network_ctrl_info_t) * pInitDesc->CtrlNb);
        NetworkInfo.pPortInfoList = MemAllocCalloc((uint32_t)sizeof(network_port_info_t) * pInitDesc->PortNb);
        // Buffer memory allocation
        NetworkInfo.pBuffer = MemAllocCalloc(ETHERNET_FRAME_LENTGH_MAX);
        return true;
    } else {
        return false;
    }
}

bool NetworkCtrlAdd(uint8_t ctrlId, const network_ctrl_desc_t *pCtrlDesc) {
    if ((ctrlId < NetworkInfo.pInitDesc->CtrlNb) && (pCtrlDesc != NULL) && NetworkCheckComItfc(&pCtrlDesc->ComInterface)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Copy desc address
        pNetworkCtrl->pDesc = pCtrlDesc;
        // Init internal variables
        pNetworkCtrl->TimerDecayARP  = 0;
        pNetworkCtrl->IcmpReplyDelay = 0;
        pNetworkCtrl->IcmpReplyReceived = false;
        // Init arp table
        pNetworkCtrl->pArpArray = MemAllocCalloc((uint32_t)sizeof(arp_entry_t) * pCtrlDesc->ArpEntryNb);
        // Init controller subnet mask
        memcpy(pNetworkCtrl->SubnetMask, pCtrlDesc->DefaultSubnetMask, IP_ADDR_LENGTH);
        // Init controller ip address
        memcpy(pNetworkCtrl->IpAddr, pCtrlDesc->DefaultIpAddr, IP_ADDR_LENGTH);
        // Init controller mac address
        memcpy(pNetworkCtrl->MacAddr, pCtrlDesc->DefaultMacAddr, MAC_ADDR_LENGTH);
        // Send mac address to mac controller
        pNetworkCtrl->pDesc->ComInterface.MacCtrlSetMacAddr(pCtrlDesc->MacCtrlId, pNetworkCtrl->MacAddr);
        return true;
    } else {
         return false;
    }
}

bool NetworkPortAdd(uint8_t portId, const network_port_desc_t *pPortDesc) {
    if ((portId < NetworkInfo.pInitDesc->PortNb) && (pPortDesc != NULL)) {
        network_port_info_t *pNetworkPort = &(NetworkInfo.pPortInfoList[portId]);
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[pPortDesc->NetworkCtrlId]);

        // Add only if default dest ip address valid for the subnet
        if (NetworkIsIpValid(pPortDesc->DefaultDstIpAddr, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
            // Copy desc address
            pNetworkPort->pDesc = pPortDesc;
            // Init internal variables
            pNetworkPort->TimerRequestARP = 0;
            pNetworkPort->IsVirtualComTx = true;
            pNetworkPort->IsVirtualComRx = true;
            // Init default dest ip address
            memcpy(pNetworkPort->DstIpAddr, pPortDesc->DefaultDstIpAddr, IP_ADDR_LENGTH);
            // Init default network ports nb
            pNetworkPort->InPortNb = pPortDesc->DefaultInPortNb;
            pNetworkPort->OutPortNb = pPortDesc->DefaultOutPortNb;
            // Data fifo memory allocation
            pNetworkPort->pFifoRxMsg = FifoCreate(pPortDesc->RxFifoSize, sizeof(uint8_t));
            pNetworkPort->pFifoTxMsg = FifoCreate(pPortDesc->TxFifoSize, sizeof(uint8_t));
            // Descriptor fifo memory allocation
            if (pPortDesc->RxDescFifoSize != 0) {
                pNetworkPort->pFifoRxMsgDesc = FifoCreate(pPortDesc->RxDescFifoSize, sizeof(network_msg_desc_t));
                pNetworkPort->IsVirtualComRx = false;
            }
            if (pPortDesc->TxDescFifoSize != 0) {
                pNetworkPort->pFifoTxMsgDesc = FifoCreate(pPortDesc->TxDescFifoSize, sizeof(network_msg_desc_t));
                pNetworkPort->IsVirtualComTx = false;
            }
            return true;
        }
    }
    return false;
}

void NetworkCtrlArpDecayProcess(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Check if it's time to refresh
        if (NetworkInfo.pInitDesc->GenInterface.pFnTimerIsPassed(pNetworkCtrl->TimerDecayARP)) {
            // Set time to next refresh
            pNetworkCtrl->TimerDecayARP = NetworkInfo.pInitDesc->GenInterface.pFnTimerGetTime() + NETWORK_ARP_DECAY_COOLDOWN;
            // Parse arp table for decayed entries
            for (uint8_t idx = 0; idx < pNetworkCtrl->pDesc->ArpEntryNb; idx++) {
                arp_entry_t *pArpEntry = &(pNetworkCtrl->pArpArray[idx]);
                // Check valid entries with decay on
                if ((pArpEntry->Status.HasDecay) && (pArpEntry->Status.IsValid)) {
                    // Check if this entry was inactive for too long
                    uint32_t ARPDecayTimer = pArpEntry->DecayTimer + NETWORK_ARP_DECAY_TIME;
                    if (NetworkInfo.pInitDesc->GenInterface.pFnTimerIsPassed(ARPDecayTimer)) {
                        // Clear the entry
                        memset(pArpEntry, 0, sizeof(arp_entry_t));
                    }
                }
            }
        }
    }
}

void NetworkCtrlRxProcess(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Check if there is data to process
        if (pNetworkCtrl->pDesc->ComInterface.MacCtrlHasMsg(pNetworkCtrl->pDesc->MacCtrlId)) {
            // Get data
            uint16_t dataSize;
            pNetworkCtrl->pDesc->ComInterface.MacCtrlGetMsg(pNetworkCtrl->pDesc->MacCtrlId, NetworkInfo.pBuffer, &dataSize);
            // Process data
            if (!NetworkProcessEthPacket(ctrlId, NetworkInfo.pBuffer, dataSize)) {
                // Something bad happened, we notify it
                if (NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify != NULL)
                    NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify(NetworkInfo.pInitDesc->ErrorCode);
            }
        }
    }
}

void NetworkCtrlTxProcess(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        // Parse the network ports
        for (uint8_t portIdx = 0; portIdx < NetworkInfo.pInitDesc->PortNb; portIdx++) {
            // Skip non-valid network ports
            if (!NetworkPortValid(portIdx)) {
                continue;
            }
            // Check if there is data to send
            if (!NetworkPortIsTxEmpty(portIdx)) {
                // Attempt to send the message
                if (!NetworkProcessSendMsg(portIdx, NetworkInfo.pBuffer)) {
                    // Something bad happened, we notify it
                    if (NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify != NULL)
                        NetworkInfo.pInitDesc->GenInterface.pFnErrorNotify(NetworkInfo.pInitDesc->ErrorCode);
                }
            }
        }
    }
}

void NetworkCtrlMainProcess(uint8_t ctrlId) {
    NetworkCtrlRxProcess(ctrlId);
    NetworkCtrlTxProcess(ctrlId);
    NetworkCtrlArpDecayProcess(ctrlId);
}

bool NetworkCtrlAddArpEntry(uint8_t ctrlId, const uint8_t *pIpAddr, const uint8_t *pMacAddr, bool hasDecay) {
    if (NetworkCtrlValid(ctrlId) && (pIpAddr != NULL) && (pMacAddr != NULL)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Add only if ip address valid for the subnet
        if (NetworkIsIpValid(pIpAddr, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
            return NetworkUpdateArpTable(ctrlId, pIpAddr, pMacAddr, hasDecay);
        }
    }
    return false;
}

bool NetworkCtrlForceRequestARP(uint8_t ctrlId, const uint8_t *pIpAddr) {
    if (NetworkCtrlValid(ctrlId) && (pIpAddr != NULL)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Send request only if ip valid for the subnet
        if (NetworkIsIpValid(pIpAddr, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
            return NetworkRequestArp(ctrlId, pIpAddr);
        }
    }
    return false;
}

bool NetworkCtrlIsArpValid(uint8_t ctrlId, const uint8_t *pIpAddr) {
    if (NetworkCtrlValid(ctrlId) && (pIpAddr != NULL)) {
        arp_entry_t *arpEntry = NetworkGetArpEntry(ctrlId, pIpAddr);

        return ((arpEntry != NULL) && (arpEntry->Status.IsValid));
    } else {
        return false;
    }
}

bool NetworkCtrlSendPingIcmp(uint8_t ctrlId, const uint8_t *pIpAddr) {
    if (NetworkCtrlValid(ctrlId) && (pIpAddr != NULL)) {
        return NetworkSendIcmpEchoRequest(ctrlId, pIpAddr);
    } else {
        return false;
    }
}

bool NetworkCtrlCheckPingReply(uint8_t ctrlId, uint32_t *pDelayValue) {
    if (NetworkCtrlValid(ctrlId)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        if ((pDelayValue != NULL) && (pNetworkCtrl->IcmpReplyReceived)) {
            *pDelayValue = pNetworkCtrl->IcmpReplyDelay;
        }
        return pNetworkCtrl->IcmpReplyReceived;
    } else {
        return false;
    }
}

uint32_t NetworkPortTxFreeSpace(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        // Check if we have place in the descriptor fifo
        if ((!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) && (FifoFreeSpace(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc) == 0))
            return 0;

        return FifoFreeSpace(NetworkInfo.pPortInfoList[portId].pFifoTxMsg);
    } else {
        return 0;
    }
}

bool NetworkPortIsTxEmpty(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        return ((FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoTxMsg) == 0) || (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx && (FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoTxMsgDesc) == 0)));
    } else {
        return false;
    }
}

bool NetworkPortSendByte(uint8_t portId, uint8_t data, const uint8_t *pIpDest) {
    if (NetworkPortValid(portId)) {
        return NetworkStoreSendData(portId, &data, sizeof(data), pIpDest);
    } else {
        return false;
    }
}

bool NetworkPortSendString(uint8_t portId, const char *str, const uint8_t *pIpDest) {
    if (NetworkPortValid(portId) && (str != NULL)) {
        uint16_t strLgth = (uint16_t)strlen(str);

        if (NetworkInfo.pPortInfoList[portId].IsVirtualComTx || (strLgth <= ETHERNET_MAX_DATA_SIZE)) {
            return NetworkStoreSendData(portId, (uint8_t*)str, strLgth, pIpDest);
        }
    }
    return false;
}

bool NetworkPortSendBuff(uint8_t portId, const uint8_t *pBuffer, uint16_t buffSize, const uint8_t *pIpDest) {
    if (NetworkPortValid(portId) && (pBuffer != NULL)) {
        if (NetworkInfo.pPortInfoList[portId].IsVirtualComTx || (buffSize <= ETHERNET_MAX_DATA_SIZE)) {
            return NetworkStoreSendData(portId, pBuffer, buffSize, pIpDest);
        }
    }
    return false;
}

bool NetworkPortIsRxEmpty(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        return ((FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoRxMsg) == 0) || (!NetworkInfo.pPortInfoList[portId].IsVirtualComRx && (FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoRxMsgDesc) == 0)));
    } else {
        return false;
    }
}

bool NetworkPortReadByte(uint8_t portId, uint8_t *pBuffer) {
    if (NetworkPortValid(portId) && (pBuffer != NULL) && NetworkInfo.pPortInfoList[portId].IsVirtualComRx) {
        return FifoRead(NetworkInfo.pPortInfoList[portId].pFifoRxMsg, pBuffer, 1, true);
    } else {
        return false;
    }
}

bool NetworkPortReadBuff(uint8_t portId, uint8_t *pBuffer, uint16_t *pDataSize, uint16_t buffSize, uint8_t *pSrcIp) {
    if (NetworkPortValid(portId) && (pBuffer != NULL) && (pDataSize != NULL)) {
        uint16_t msgSize = 0;

        // Get message info
        if (!NetworkInfo.pPortInfoList[portId].IsVirtualComRx) {
            // Attempt to read message descriptor
            network_msg_desc_t msgDesc;
            if (FifoRead(NetworkInfo.pPortInfoList[portId].pFifoRxMsgDesc, &msgDesc, 1, false)) {
                msgSize = msgDesc.MsgSize;
                // Register source ip address is possible
                if (pSrcIp != NULL) {
                    memcpy(pSrcIp, msgDesc.IpAddr, IP_ADDR_LENGTH);
                }
            } else {
                return false;
            }
        } else {
            // Virtual com port: Get as much data as possible
            msgSize = (uint16_t)FifoItemCount(NetworkInfo.pPortInfoList[portId].pFifoRxMsg);
        }
        // Message size limitation
        if (msgSize > buffSize) {
            *pDataSize = buffSize;
            // Can't read in a non virtual com port if buffer is too small
            if (!NetworkInfo.pPortInfoList[portId].IsVirtualComRx) {
                return false;
            }
        } else {
            *pDataSize = msgSize;
        }
        // Attempt to read and consume data
        if (FifoRead(NetworkInfo.pPortInfoList[portId].pFifoRxMsg, pBuffer, *pDataSize, true)) {
            // Consume descriptor if data is consumed
            if (!NetworkInfo.pPortInfoList[portId].IsVirtualComTx) {
                FifoConsume(NetworkInfo.pPortInfoList[portId].pFifoRxMsgDesc, 1);
            }
            return true;
        }
    }
    return false;
}

uint8_t *NetworkCtrlGetMacAddr(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        return NetworkInfo.pCtrlInfoList[ctrlId].MacAddr;
    } else {
        return NULL;
    }
}

bool NetworkCtrlSetMacAddr(uint8_t ctrlId, const uint8_t *pNewMacAddr) {
    if (NetworkCtrlValid(ctrlId) && (pNewMacAddr != NULL)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[ctrlId]);

        // Set new mac address and send it to the mac controller
        memcpy(pNetworkCtrl->MacAddr, pNewMacAddr, MAC_ADDR_LENGTH);
        pNetworkCtrl->pDesc->ComInterface.MacCtrlSetMacAddr(pNetworkCtrl->pDesc->MacCtrlId, pNewMacAddr);
        return true;
    } else {
        return false;
    }
}

uint8_t *NetworkCtrlGetIpAddress(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        return NetworkInfo.pCtrlInfoList[ctrlId].IpAddr;
    } else {
        return NULL;
    }
}

bool NetworkCtrlSetIpAddress(uint8_t ctrlId, const uint8_t *pNewIpAddr) {
    if (NetworkCtrlValid(ctrlId) && (pNewIpAddr != NULL)) {
        memcpy(NetworkInfo.pCtrlInfoList[ctrlId].IpAddr, pNewIpAddr, IP_ADDR_LENGTH);
        return true;
    } else {
        return false;
    }
}

uint8_t *NetworkCtrlGetSubnetMask(uint8_t ctrlId) {
    if (NetworkCtrlValid(ctrlId)) {
        return NetworkInfo.pCtrlInfoList[ctrlId].SubnetMask;
    } else {
        return NULL;
    }
}

bool NetworkCtrlSetSubnetMask(uint8_t ctrlId, const uint8_t *pNewSubnetMask) {
    if (NetworkCtrlValid(ctrlId) && (pNewSubnetMask != NULL)) {
        memcpy(NetworkInfo.pCtrlInfoList[ctrlId].SubnetMask, pNewSubnetMask, IP_ADDR_LENGTH);
        return true;
    } else {
        return false;
    }
}

uint8_t *NetworkPortGetDstIpAddress(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        return NetworkInfo.pPortInfoList[portId].DstIpAddr;
    } else {
        return NULL;
    }
}

bool NetworkPortSetDstIpAddress(uint8_t portId, const uint8_t *pNewIpAddr) {
    if (NetworkPortValid(portId) && (pNewIpAddr != NULL)) {
        network_ctrl_info_t *pNetworkCtrl = &(NetworkInfo.pCtrlInfoList[NetworkInfo.pPortInfoList[portId].pDesc->NetworkCtrlId]);

        // Change ip address only if valid for the subnet
        if (NetworkIsIpValid(pNewIpAddr, pNetworkCtrl->IpAddr, pNetworkCtrl->SubnetMask)) {
            memcpy(NetworkInfo.pPortInfoList[portId].DstIpAddr, pNewIpAddr, IP_ADDR_LENGTH);
            return true;
        }
    }
    return false;
}

uint16_t NetworkPortGetInPortNb(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        return NetworkInfo.pPortInfoList[portId].InPortNb;
    } else {
        return 0;
    }
}

bool NetworkPortSetInPortNb(uint8_t portId, uint16_t newInPortNb) {
    if (NetworkPortValid(portId)) {
        NetworkInfo.pPortInfoList[portId].InPortNb = newInPortNb;
        return true;
    } else {
        return false;
    }
}

uint16_t NetworkPortGetOutPortNb(uint8_t portId) {
    if (NetworkPortValid(portId)) {
        return NetworkInfo.pPortInfoList[portId].OutPortNb;
    } else {
        return 0;
    }
}

bool NetworkPortSetOutPortNb(uint8_t portId, uint16_t newOutPortNb) {
    if (NetworkPortValid(portId)) {
        NetworkInfo.pPortInfoList[portId].OutPortNb = newOutPortNb;
        return true;
    } else {
        return false;
    }
}