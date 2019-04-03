/**
 * \file custom_main.c
 * \brief Application exemple for my network library.
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
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
// Custom lib
#include <Common.h>
#include <LibIp.h>
#include <Timer.h>
#include <MemAlloc.h>
#include <MacCtrl.h>
#include <Network.h>

// *** Module definitions ***

// --- Mac Controller ---
enum mac_controller_list {
    MAIN_MAC_CTRL = 0,
    MAC_CTRL_COUNT,
};

static const mac_init_desc_t MacCtrlInitDesc = {
    MAC_CTRL_COUNT,
};

static const mac_ctrl_init_desc_t MainMacCtrlDesc = {
    100,
    5 * ETHERNET_FRAME_LENTGH_MAX,
};

// --- Network ---
enum network_controller_list {
    MAIN_NETWORK_CTRL = 0,
    NETWORK_CTRL_COUNT,
};

enum network_port_list {
    MAIN_NETWORK_PORT = 0,
    NETWORK_PORT_COUNT,
};

static const network_init_desc_t NetworkInitDesc = {
    {
        (error_notify_ft *)NULL,
        (timer_get_time_ft *)TimerRefGetTime,
        (timer_is_passed_ft *)TimerRefIsPassed,
    },
    0,
    NETWORK_CTRL_COUNT,
    NETWORK_PORT_COUNT,
};

static const network_ctrl_desc_t NetworkMainCtrlDesc = {
    {
        (network_mac_ctrl_set_mac_addr_ft *)MacCtrlSetMacAddress,
        (network_mac_ctrl_has_msg_ft*)MacCtrlHasData,
        (network_mac_ctrl_get_msg_ft*)MacCtrlGetData,
        (network_mac_ctrl_send_msg_ft*)MacCtrlSendData,
    },
    {0x54, 0x10, 0xec, 0x01, 0x23, 0x45}, // Controller mac address
    {192, 168, 2, 101}, // Controller ip address
    {255, 255, 255, 0}, // Controller subnet mask
    MAIN_MAC_CTRL, // Mac controller id
    20, // Arp table size
};

static const network_port_desc_t NetworkMainPortDesc = {
    MAIN_NETWORK_CTRL, // Network controller id
    IP_PROT_UDP, // Network protocol
    {192, 168, 2, 100}, // Default recipient ip address
    10101, // Local network port nb
    10201, // Distant network port nb
    4 * ETHERNET_FRAME_LENTGH_MAX, // Rx fifo size (bytes)
    80, // Rx descriptor fifo size (message nb)
    1 * ETHERNET_FRAME_LENTGH_MAX, // Tx fifo size (bytes)
    20, // Tx descriptor fifo size (message nb)
};

// *** End of module definitions ***

// Variables
static uint8_t _HEAP[0x2000];

// Functions

/**
 * \fn static void app_init(void)
 * \brief Initialize module
 *
 * \return void
 */
static void app_init(void) {
    // Memory allocation
    MemAllocInit(_HEAP, sizeof(_HEAP));
    // Mac controller
    MacCtrlInit(&MacCtrlInitDesc);
    MacCtrlAdd(MAIN_MAC_CTRL, &MainMacCtrlDesc);
    // Network 
    NetworkInit(&NetworkInitDesc);
    NetworkCtrlAdd(MAIN_NETWORK_CTRL, &NetworkMainCtrlDesc);
    NetworkPortAdd(MAIN_NETWORK_PORT, &NetworkMainPortDesc);
}

/**
 * \fn static void app_process(void)
 * \brief Execute module process
 *
 * \return void
 */
static void app_process(void) {
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
}

/**
 * \fn void main(void)
 * \brief Program entry
 *
* \return void
 */
void main(void) {
    // Modules initialization
    app_init();

    // Main loop
    while (true) {
        app_process();
    }
}
