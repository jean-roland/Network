/**
 * \file LibIp.h
 * \brief Librairy containing types for Ethernet and IP modules
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

#ifndef LibIp_h
#define LibIp_h

// *** Libraries include ***
// Standard lib
#include <stdint.h>
// Custom lib

// *** Definitions ***
// --- Private Constants ---
// Ethernet types
#define ETH_PROT_IPV4 0x0800 // 2048  (0x0800) IPv4
#define ETH_PROT_ARP 0x0806 // 2054  (0x0806) ARP
#define ETH_PROT_APPLETALK 0x8019 // 32923 (0x8019) Appletalk
#define ETH_PROT_IPV6 0x86DD // 34525 (0x86DD) IPv6
// ARP OP codes
#define ARP_REQUEST 0x0001 // ARP Request packet
#define ARP_REPLY 0x0002 // ARP Reply packet
// IP protocols code
#define IP_PROT_ICMP 1
#define IP_PROT_IPV4 4
#define IP_PROT_TCP 6
#define IP_PROT_UDP 17
// ICMP types
#define ICMP_ECHO_REPLY 0x00 // Echo reply (used to ping)
// 1 and 2 Reserved
#define ICMP_DEST_UNREACHABLE 0x03 // Destination Unreachable
#define ICMP_SOURCE_QUENCH 0x04 // Source Quench
#define ICMP_REDIR_MESSAGE 0x05 // Redirect Message
#define ICMP_ALT_HOST_ADD 0x06 // Alternate Host Address
// 0x07 Reserved
#define ICMP_ECHO_REQUEST 0x08 // Echo Request
#define ICMP_ROUTER_ADV 0x09 // Router Advertisement
#define ICMP_ROUTER_SOL 0x0A // Router Solicitation
#define ICMP_TIME_EXC 0x0B // Time Exceeded
#define ICMP_PARAM_PB 0x0C // Parameter Problem: Bad IP header
#define ICMP_TIMESTAMP 0x0D // Timestamp
#define ICMP_TIMESTAMP_REP 0x0E // Timestamp Reply
#define ICMP_INFO_REQ 0x0F // Information Request
#define ICMP_INFO_REPLY 0x10 // Information Reply
#define ICMP_ADD_MASK_REQ 0x11 // Address Mask Request
#define ICMP_ADD_MASK_REP 0x12 // Address Mask Reply
// 0x13 Reserved for security
// 0X14 through 0x1D Reserved for robustness experiment
#define ICMP_TRACEROUTE 0x1E // Traceroute
#define ICMP_DAT_CONV_ERROR 0x1F // Datagram Conversion Error
#define ICMP_MOB_HOST_RED 0x20 // Mobile Host Redirect
#define ICMP_W_A_Y 0x21 // Where-Are-You (originally meant for IPv6)
#define ICMP_H_I_A 0x22 // Here-I-Am (originally meant for IPv6)
#define ICMP_MOB_REG_REQ 0x23 // Mobile Registration Request
#define ICMP_MOB_REG_REP 0x24 // Mobile Registration Reply
#define ICMP_DOM_NAME_REQ 0x25 // Domain Name Request
#define ICMP_DOM_NAME_REP 0x26 // Domain Name Reply
#define ICMP_SKIP_ALGO_PROT 0x27 // SKIP Algorithm Discovery Protocol, Simple Key-Management for Internet Protocol
#define ICMP_PHOTURIS 0x28 // Photuris, Security failures
#define ICMP_EXP_MOBIL 0x29 // ICMP for experimental mobility protocols such as Seamoby [RFC4065]
// 0x2A through 0xFF Reserved

// --- Public Types ---
// Address Sizes
#define IP_ADDR_LENGTH 4
#define MAC_ADDR_LENGTH 6

// Ethernet header structure type
typedef struct _ethernet_header {
    uint8_t dstMac[MAC_ADDR_LENGTH]; // Destination mac address
    uint8_t srcMac[MAC_ADDR_LENGTH]; // Source mac address
    uint16_t lengthOrType; // Size of frame
} ethernet_header_t;

// ARP header structure type
typedef struct _arp_header {
    uint16_t hardwareTpe; // [2 bytes]
    uint16_t protocolType; // [2 bytes]
    uint8_t hardwareLength; // [1 bytes]
    uint8_t protocolLength; // [1 bytes]
    uint16_t operation; // [2 bytes]
    uint8_t senderMac[MAC_ADDR_LENGTH]; // [6 bytes]
    uint8_t senderIp[IP_ADDR_LENGTH]; // [4 bytes]
    uint8_t targetMac[MAC_ADDR_LENGTH]; // [6 bytes]
    uint8_t targetIp[IP_ADDR_LENGTH]; // [4 bytes]
} arp_header_t; // total: 28 bytes, 0 padding

// IPV4 header structure type
typedef struct _ipv4_header {
    uint8_t ihl: 4; // [4 bits] Internet Header Length: 4 uint32_t in the header
    uint8_t version: 4; // [4 bits] version: 4 stands for IPV4
    uint8_t ecn: 2; // [2 bits] Explicit Congestion Notification
    uint8_t dscp: 6; // [6 bits] Differentiated Services Code Point
    uint16_t length; // [2 bytes] Total size in bytes (header+data)
    uint16_t identification; // [2 bytes] identification
    uint16_t fragmentOffsetAndFlags; // [2 bytes] Contains fragment offset and the flags
    uint8_t ttl; // [1 byte] time to live
    uint8_t protocol; // [1 byte]
    uint16_t checksum; // [2 bytes]
    uint8_t srcIp[IP_ADDR_LENGTH]; // [4 bytes]
    uint8_t dstIp[IP_ADDR_LENGTH]; // [4 bytes]
} ipv4_header_t; // total: 20 bytes, 0 padding

// ICMP echo structure type
typedef struct _icmp_echo_header {
    uint8_t type; // [1 byte] Type of message
    uint8_t code; // [1 byte] Type subcode
    uint16_t cksum; // [2 bytes] 1's complement checksum of struct
    uint16_t id; // [2 bytes] Identifier
    uint16_t seq; // [2 bytes] Sequence number
} icmp_header_t; // total: 8 bytes, 0 padding

// UDP header structure type
typedef struct _udp_header {
    uint16_t srcPort; // [2 bytes] source port
    uint16_t dstPort; // [2 bytes] destination port
    uint16_t length; // [2 bytes] frame size in bytes (header + data)
    uint16_t checksum; // [2 bytes]
} udp_header_t; // total: 8 bytes, 0 padding

// Headers size
#define ETH_HEADER_SIZE ( sizeof( ethernet_header_t ) ) // [14 bytes]
#define ARP_HEADER_SIZE ( sizeof( arp_header_t ) ) // [28 bytes]
#define IPV4_HEADER_SIZE ( sizeof( ipv4_header_t ) ) // [20 bytes]
#define ICMP_HEADER_SIZE ( sizeof( icmp_header_t ) ) // [8 bytes]
#define UDP_HEADER_SIZE ( sizeof( udp_header_t ) ) // [8 bytes]
#define NETWORK_HEADER_SIZE (ETH_HEADER_SIZE + IPV4_HEADER_SIZE + UDP_HEADER_SIZE) // [42 bytes] network header size (UDP + IP + ETH)

// Other Sizes
#define ETHERNET_PAYLOAD_SIZE  1500
#define ETHERNET_MAX_DATA_SIZE (ETHERNET_PAYLOAD_SIZE - (IPV4_HEADER_SIZE + UDP_HEADER_SIZE)) // [1472 bytes] frame max data size (if more, frame is fragmented)
#define ETHERNET_FRAME_LENTGH_MAX (ETHERNET_PAYLOAD_SIZE + ETH_HEADER_SIZE)

// --- Public Variables ---
// --- Public Function Prototypes ---

// *** End Definitions ***
#endif // LibIp_h