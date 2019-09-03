#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"

#include "Libip.h"
#include "Utils.h"
#include "Fifo.h"
#include "Network.h"

#include "mock_MemAlloc.h"
#include "mock_Timer.h"
#include "mock_MacCtrl.h"

// *** Descriptors ***
enum mac_controller_list {
    MAIN_MAC_CTRL = 0,
    MAC_CTRL_COUNT,
};

enum network_controller_list {
    MAIN_NETWORK_CTRL = 0,
    NETWORK_CTRL_COUNT,
};

enum network_port_list {
    MAIN_NETWORK_PORT = 0,
    SEC_NETWORK_PORT,
    NETWORK_PORT_COUNT,
};

static const network_init_desc_t NetworkInitDesc = {
    {
        (error_notify_ft *)NULL,
        (timer_get_time_ft *)TimerRefGetTime,
        (timer_is_passed_ft *)TimerRefIsPassed,
    },
    0, // Error code
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
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xab}, // Controller mac address
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
    1 * ETHERNET_FRAME_LENTGH_MAX, // Rx fifo size (bytes)
    20, // Rx descriptor fifo size (message nb)
    1 * ETHERNET_FRAME_LENTGH_MAX, // Tx fifo size (bytes)
    20, // Tx descriptor fifo size (message nb)
};

static const network_port_desc_t NetworkSecPortDesc = {
    MAIN_NETWORK_CTRL, // Network controller id
    IP_PROT_UDP, // Network protocol
    {192, 168, 2, 99}, // Default recipient ip address
    25565, // Local network port nb
    25565, // Distant network port nb
    1 * ETHERNET_FRAME_LENTGH_MAX, // Rx fifo size (bytes)
    0, // Mode virtual port com
    1 * ETHERNET_FRAME_LENTGH_MAX, // Tx fifo size (bytes)
    0, // Mode virtual port com
};



// *** Private global vars ***
static bool init_srand;
static void *memPtr[64];
static int memIdx;
static uint8_t in_buffer[ETHERNET_FRAME_LENTGH_MAX];
static uint8_t in_buff_size;
static uint8_t out_buffer[ETHERNET_FRAME_LENTGH_MAX];
static uint8_t out_buff_size;
static bool hasData;
static uint32_t timeVal;



// *** Model frames ***
// SonyMobi_67:89:ab → Broadcast ARP Who has 192.168.2.0? Tell 192.168.2.101
static const uint8_t arp_req_int[] = {
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x08, 0x06, 0x00, 0x01,
0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xc0, 0xa8, 0x02, 0x65,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xa8, 0x02, 0x00
};

// 11:22:44:55:88:aa → SonyMobi_67:89:ab ARP 192.168.2.0 is at 11:22:44:55:88:aa
static const uint8_t arp_reply_ext[] = {
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x06, 0x00, 0x01,
0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0xC0, 0xA8, 0x02, 0x00,
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xC0, 0xA8, 0x02, 0x65
};

// 11:22:44:55:88:aa → Broadcast ARP Who has 192.168.2.101? Tell 192.168.2.0
static const uint8_t arp_req_ext[] = {
0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x06, 0x00, 0x01,
0x08, 0x00, 0x06, 0x04, 0x00, 0x01, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0xC0, 0xA8, 0x02, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x65
};

// SonyMobi_67:89:ab → 11:22:44:55:88:aa ARP 192.168.2.101 is at 01:23:45:67:89:ab
static const uint8_t arp_reply_int[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x08, 0x06, 0x00, 0x01,
0x08, 0x00, 0x06, 0x04, 0x00, 0x02, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xC0, 0xA8, 0x02, 0x65,
0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0xC0, 0xA8, 0x02, 0x00
};

// 192.168.2.101 → 192.168.2.0 ICMP Echo (ping) request
static const uint8_t icmp_req_int[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x08, 0x00, 0x45, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x40, 0x00, 0x80, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x65, 0xC0, 0xA8,
0x02, 0x00, 0x08, 0x00, 0xD2, 0xDC, 0x01, 0x00, 0x01, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
};

// 192.168.2.0 → 192.168.2.101 ICMP Echo (ping) reply
static const uint8_t icmp_reply_ext[] = {
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x00, 0x45, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x40, 0x00, 0x80, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x00, 0xC0, 0xA8,
0x02, 0x65, 0x00, 0x00, 0xDA, 0xDC, 0x01, 0x00, 0x01, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
};

// 192.168.2.0 → 192.168.2.101 ICMP Echo (ping) request
static const uint8_t icmp_req_ext[] = {
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x00, 0x45, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x40, 0x00, 0x80, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x00, 0xC0, 0xA8,
0x02, 0x65, 0x08, 0x00, 0xD2, 0xDC, 0x01, 0x00, 0x01, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
};

// 192.168.2.101 → 192.168.2.0 ICMP Echo (ping) reply
static const uint8_t icmp_reply_int[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x08, 0x00, 0x45, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x40, 0x00, 0x80, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x65, 0xC0, 0xA8,
0x02, 0x00, 0x00, 0x00, 0xDA, 0xDC, 0x01, 0x00, 0x01, 0x00, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05
};

// 192.168.2.101 → 192.168.2.0 UDP 10101 → 10201, data = 0x55
static const uint8_t udp_tx_byte[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xaa, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x08, 0x00, 0x45, 0x00,
0x00, 0x1d, 0x00, 0x00, 0x40, 0x00, 0x80, 0x11, 0x00, 0x00, 0xc0, 0xa8, 0x02, 0x65, 0xc0, 0xa8,
0x02, 0x00, 0x27, 0x75, 0x27, 0xd9, 0x00, 0x09, 0x00, 0x00, 0x55
};

// 192.168.2.101 → 192.168.2.0 UDP 10101 → 10201, data = 0x48, 0x65, 0x6C, 0x6C, 0x6F "Hello"
static const uint8_t udp_tx_str[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xaa, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x08, 0x00, 0x45, 0x00,
0x00, 0x21, 0x00, 0x00, 0x40, 0x00, 0x80, 0x11, 0x00, 0x00, 0xc0, 0xa8, 0x02, 0x65, 0xc0, 0xa8,
0x02, 0x00, 0x27, 0x75, 0x27, 0xd9, 0x00, 0x0d, 0x00, 0x00, 0x48, 0x65, 0x6C, 0x6C, 0x6F
};
    
// 192.168.2.101 → 192.168.2.0 UDP 10101 → 10201, data = 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
static const uint8_t udp_tx_barray[] = {
0x11, 0x22, 0x44, 0x55, 0x88, 0xaa, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0x08, 0x00, 0x45, 0x00,
0x00, 0x26, 0x00, 0x00, 0x40, 0x00, 0x80, 0x11, 0x00, 0x00, 0xc0, 0xa8, 0x02, 0x65, 0xc0, 0xa8,
0x02, 0x00, 0x27, 0x75, 0x27, 0xd9, 0x00, 0x12, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
0x06, 0x07, 0x08, 0x09
};

// 192.168.2.0 → 192.168.2.101 UDP 10201 → 10101, data = 0x53, 0x79, 0x6E, 0x65, 0x72, 0x65, 0x73, 0x69, 0x73 "Syrenesis"
static const uint8_t udp_rx_barray[] = {
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x00, 0x45, 0x00,
0x00, 0x25, 0x00, 0x00, 0x40, 0x00, 0x80, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x00, 0xC0, 0xA8,
0x02, 0x65, 0x27, 0xD9, 0x27, 0x75, 0x00, 0x11, 0x00, 0x00, 0x53, 0x79, 0x6E, 0x65, 0x72, 0x65,
0x73, 0x69, 0x73
};

// 192.168.2.16 → 192.168.2.101 UDP 25565 → 25565, data = 0x48, 0x65, 0x73, 0x73, 0x69, 0x61, 0x6E, 0x20, 0x6D, 0x61, 0x74, 0x72, 0x69, 0x78 "Hessian matrix"
static const uint8_t udp_com_rx_barray[] = {
0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0x11, 0x22, 0x44, 0x55, 0x88, 0xAA, 0x08, 0x00, 0x45, 0x00,
0x00, 0x2A, 0x00, 0x00, 0x40, 0x00, 0x80, 0x11, 0x00, 0x00, 0xC0, 0xA8, 0x02, 0x10, 0xC0, 0xA8,
0x02, 0x65, 0x63, 0xDD, 0x63, 0xDD, 0x00, 0x16, 0x00, 0x00, 0x48, 0x65, 0x73, 0x73, 0x69, 0x61,
0x6E, 0x20, 0x6D, 0x61, 0x74, 0x72, 0x69, 0x78
};

// *** Private Functions ***
static void *calloc_Callback(uint32_t size, int num_calls) {
    memPtr[memIdx] = calloc(size,1);
    return memPtr[memIdx++];
}

static void *malloc_Callback(uint32_t size, int num_calls) {
    memPtr[memIdx] = malloc(size);
    return memPtr[memIdx++];
}

static bool has_data_Callback(uint8_t macId, int num_calls) {
    return hasData;
}

static bool get_data_Callback(uint8_t macId, uint8_t *pBuffer, uint16_t *pBuffSize, int num_calls) {
    memcpy(pBuffer, in_buffer, in_buff_size);
    *pBuffSize = in_buff_size;
    return true;
}

static bool send_data_Callback(uint8_t macId, const uint8_t *pBuffer, uint16_t buffSize, int num_calls) {
    memcpy(out_buffer, pBuffer, buffSize);
    out_buff_size = buffSize;
    return true;
}

static uint32_t time_get_Callback(int num_calls) {
    return timeVal;
}

static bool time_passed_Callback(uint32_t val, int num_calls) {
    return (timeVal >= val);
}

static void arp_tests(void) {
    uint8_t ipAdr[4] = {192, 168, 2, 0};
    uint8_t macAdr[6] = {0x11, 0x22, 0x44, 0x55, 0x88, 0xaa};
    // Direct arp add test
    TEST_ASSERT_FALSE(NetworkCtrlIsArpValid(MAIN_NETWORK_CTRL, ipAdr));
    TEST_ASSERT_TRUE(NetworkCtrlAddArpEntry(MAIN_NETWORK_CTRL, ipAdr, macAdr, true));
    TEST_ASSERT_TRUE(NetworkCtrlIsArpValid(MAIN_NETWORK_CTRL, ipAdr));
    // Test Decay
    timeVal += 30000;
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_TRUE(NetworkCtrlIsArpValid(MAIN_NETWORK_CTRL, ipAdr));
    timeVal += 30000;
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_FALSE(NetworkCtrlIsArpValid(MAIN_NETWORK_CTRL, ipAdr));
    // Force request arp test
    TEST_ASSERT_TRUE(NetworkCtrlForceRequestARP(MAIN_NETWORK_CTRL, ipAdr));
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    // Retrieve and test arp request
    TEST_ASSERT_EQUAL_HEX8_ARRAY(arp_req_int, out_buffer, out_buff_size);
    out_buff_size = 0;
    // Input arp reply
    hasData = true;
    memcpy(in_buffer, arp_reply_ext, sizeof(arp_reply_ext));
    in_buff_size = sizeof(arp_reply_ext);
    // Process reply
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_TRUE(NetworkCtrlIsArpValid(MAIN_NETWORK_CTRL, ipAdr));
    // Test reply to request
    hasData = true;
    memcpy(in_buffer, arp_req_ext, sizeof(arp_req_ext));
    in_buff_size = sizeof(arp_req_ext);    
    // Process request
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(arp_reply_int, out_buffer, out_buff_size);
    out_buff_size = 0;
    // Reset globals
    hasData = false;
    timeVal = 0;
    in_buff_size = 0;
    out_buff_size = 0;
    memset(out_buffer, 0, sizeof(out_buffer));
    memset(in_buffer, 0, sizeof(in_buffer));
}

static void icmp_tests(void) {
    uint8_t ipAdr[4] = {192, 168, 2, 0};
    // Test request
    timeVal = rand() % 1024;
    TEST_ASSERT_TRUE(NetworkCtrlSendPingIcmp(MAIN_NETWORK_CTRL, ipAdr));
    // Retrieve and test icmp request
    TEST_ASSERT_EQUAL_HEX8_ARRAY(icmp_req_int, out_buffer, out_buff_size);
    out_buff_size = 0;
    // Input icmp reply
    uint32_t delay = 0;
    uint32_t test_delay = timeVal;
    timeVal += rand() % 1024;
    test_delay = timeVal - test_delay;
    hasData = true;
    memcpy(in_buffer, icmp_reply_ext, sizeof(icmp_reply_ext));
    in_buff_size = sizeof(icmp_reply_ext);
    // Process reply
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_TRUE(NetworkCtrlCheckPingReply(MAIN_NETWORK_CTRL, &delay));
    TEST_ASSERT_EQUAL_INT(test_delay, delay);
    // Test reply to request
    hasData = true;
    memcpy(in_buffer, icmp_req_ext, sizeof(icmp_req_ext));
    in_buff_size = sizeof(icmp_req_ext);
    // Process request
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(icmp_reply_int, out_buffer, out_buff_size);
    out_buff_size = 0;
    // Reset globals
    hasData = false;
    timeVal = 0;
    in_buff_size = 0;
    out_buff_size = 0;
    memset(out_buffer, 0, sizeof(out_buffer));
    memset(in_buffer, 0, sizeof(in_buffer));
}

static void udp_tests(void) {
    uint8_t ipAdr[4] = {192, 168, 2, 0};
    // *** UDP TESTS TX ***
    uint8_t send_val = 0x55;
    char send_str[] = "Hello";
    uint8_t send_array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    // Inputting data
    TEST_ASSERT_EQUAL_INT(NetworkMainPortDesc.TxFifoSize, NetworkPortTxFreeSpace(MAIN_NETWORK_PORT));
    TEST_ASSERT_TRUE(NetworkPortIsTxEmpty(MAIN_NETWORK_PORT));
    TEST_ASSERT_TRUE(NetworkPortSendByte(MAIN_NETWORK_PORT, send_val, ipAdr));    
    TEST_ASSERT_TRUE(NetworkPortSendString(MAIN_NETWORK_PORT, send_str, ipAdr));
    TEST_ASSERT_TRUE(NetworkPortSendBuff(MAIN_NETWORK_PORT, send_array, sizeof(send_array), ipAdr));
    int txSize = sizeof(send_val) + strlen(send_str) + sizeof(send_array);
    TEST_ASSERT_EQUAL_INT(NetworkMainPortDesc.TxFifoSize - txSize, NetworkPortTxFreeSpace(MAIN_NETWORK_PORT));
    TEST_ASSERT_FALSE(NetworkPortIsTxEmpty(MAIN_NETWORK_PORT));
    // Process transmission
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(udp_tx_byte, out_buffer, out_buff_size);
    out_buff_size = 0;
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(udp_tx_str, out_buffer, out_buff_size);
    out_buff_size = 0;
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(udp_tx_barray, out_buffer, out_buff_size);
    out_buff_size = 0;    
    TEST_ASSERT_TRUE(NetworkPortIsTxEmpty(MAIN_NETWORK_PORT));
    
    // *** UDP TESTS RX ***
    uint16_t received_size;
    uint8_t received_array[64];
    uint8_t source_ip[4];
    const char modelStr[] = "Syneresis";
    // Recieve data
    TEST_ASSERT_TRUE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));
    hasData = true;
    memcpy(in_buffer, udp_rx_barray, sizeof(udp_rx_barray));
    in_buff_size = sizeof(udp_rx_barray);
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_FALSE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));
    // Read data
    TEST_ASSERT_TRUE(NetworkPortReadBuff(MAIN_NETWORK_PORT, received_array, &received_size, sizeof(received_array), source_ip));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ipAdr, source_ip, sizeof(source_ip));    
    TEST_ASSERT_EQUAL_INT(0, strcmp(modelStr, (char *)received_array));
    TEST_ASSERT_EQUAL_INT(strlen(modelStr), received_size);
    TEST_ASSERT_TRUE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));    
}

// *** Public Functions ***
void setUp(void) {
    // Init rand
    if (!init_srand) {
        srand(0x42424242);
        init_srand = true;
    }
    // Init mocking
    MemAllocCalloc_StubWithCallback(calloc_Callback);
    MemAllocMalloc_StubWithCallback(malloc_Callback);
    MacCtrlSetMacAddress_IgnoreAndReturn(true);
    // Network module init
    TEST_ASSERT_TRUE(NetworkInit(&NetworkInitDesc));
    TEST_ASSERT_TRUE(NetworkCtrlAdd(MAIN_NETWORK_CTRL, &NetworkMainCtrlDesc));
    TEST_ASSERT_TRUE(NetworkPortAdd(MAIN_NETWORK_PORT, &NetworkMainPortDesc));
}

void tearDown(void) {
    // Free allocated memory
    for (int idx = 0; idx < memIdx; idx++) {
        free(memPtr[idx]);
    }
    memIdx = 0;
}

void test_network_parameters(void) {
    // Check initial params
    TEST_ASSERT_EQUAL_INT(10201, NetworkPortGetOutPortNb(MAIN_NETWORK_PORT));
    TEST_ASSERT_EQUAL_INT(10101, NetworkPortGetInPortNb(MAIN_NETWORK_PORT));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(NetworkMainPortDesc.DefaultDstIpAddr, NetworkPortGetDstIpAddress(MAIN_NETWORK_PORT), 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(NetworkMainCtrlDesc.DefaultSubnetMask, NetworkCtrlGetSubnetMask(MAIN_NETWORK_CTRL), 4) ;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(NetworkMainCtrlDesc.DefaultIpAddr, NetworkCtrlGetIpAddress(MAIN_NETWORK_CTRL), 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(NetworkMainCtrlDesc.DefaultMacAddr, NetworkCtrlGetMacAddr(MAIN_NETWORK_CTRL), 6);
    // Set new params
    uint16_t newOutPortnb = rand();
    uint16_t newInPortnb = rand();
    uint8_t newPortDstIpAdr[4] = {192, 168, 2, 0};
    newPortDstIpAdr[3] = rand();
    uint8_t newCtrlSubnetMsk[4] = {255, 255, 254, 0};
    uint8_t newCtrlIpAdr[4] = {192, 168, 2, 0};
    newCtrlIpAdr[3] = rand();
    uint8_t newCtrlMacAdr[6];
    for (int idx = 0; idx < 6; idx++) {
        newCtrlMacAdr[idx] = rand();
    }
    printf("Test1: inport: %d, outport: %d, dstipadr4b: %d, ipadr4b: %d, macadr: %02x.%02x.%02x.%02x.%02x.%02x\n",
        newOutPortnb, newInPortnb, newPortDstIpAdr[3], newCtrlIpAdr[3],
        newCtrlMacAdr[0], newCtrlMacAdr[1], newCtrlMacAdr[2], newCtrlMacAdr[3], newCtrlMacAdr[4], newCtrlMacAdr[5]);
    TEST_ASSERT_TRUE(NetworkPortSetOutPortNb(MAIN_NETWORK_PORT, newOutPortnb));
    TEST_ASSERT_TRUE(NetworkPortSetInPortNb(MAIN_NETWORK_PORT, newInPortnb));
    TEST_ASSERT_TRUE(NetworkPortSetDstIpAddress(MAIN_NETWORK_PORT, newPortDstIpAdr));
    TEST_ASSERT_TRUE(NetworkCtrlSetSubnetMask(MAIN_NETWORK_CTRL, newCtrlSubnetMsk));
    TEST_ASSERT_TRUE(NetworkCtrlSetIpAddress(MAIN_NETWORK_CTRL, newCtrlIpAdr));
    TEST_ASSERT_TRUE(NetworkCtrlSetMacAddr(MAIN_NETWORK_CTRL, newCtrlMacAdr));
    // Check new params
    TEST_ASSERT_EQUAL_INT(newOutPortnb, NetworkPortGetOutPortNb(MAIN_NETWORK_PORT));
    TEST_ASSERT_EQUAL_INT(newInPortnb, NetworkPortGetInPortNb(MAIN_NETWORK_PORT));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newPortDstIpAdr, NetworkPortGetDstIpAddress(MAIN_NETWORK_PORT), 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newCtrlSubnetMsk, NetworkCtrlGetSubnetMask(MAIN_NETWORK_CTRL), 4) ;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newCtrlIpAdr, NetworkCtrlGetIpAddress(MAIN_NETWORK_CTRL), 4);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(newCtrlMacAdr, NetworkCtrlGetMacAddr(MAIN_NETWORK_CTRL), 6);    
}

void test_network_packets(void) {
    // Mac_ctrl spoofing
    MacCtrlHasData_StubWithCallback(has_data_Callback);
    MacCtrlGetData_StubWithCallback(get_data_Callback);
    MacCtrlSendData_StubWithCallback(send_data_Callback);
    // Timer spoofing
    TimerRefGetTime_StubWithCallback(time_get_Callback);
    TimerRefIsPassed_StubWithCallback(time_passed_Callback);
    // Init globals
    hasData = false;
    in_buff_size = 0;
    timeVal = rand() % 1024;
    
    arp_tests();
    icmp_tests();
    udp_tests();
}

void test_network_virtual_port_com(void) {
    const char modelStr[] = "Hessian matrix";
    uint16_t received_size;
    uint8_t received_array[64];
    
    // Mac_ctrl spoofing
    MacCtrlHasData_StubWithCallback(has_data_Callback);
    MacCtrlGetData_StubWithCallback(get_data_Callback);
    MacCtrlSendData_StubWithCallback(send_data_Callback);
    // Timer spoofing
    TimerRefGetTime_StubWithCallback(time_get_Callback);
    TimerRefIsPassed_StubWithCallback(time_passed_Callback);
    // Init globals
    hasData = false;
    in_buff_size = 0;
    timeVal = rand() % 1024;
    
    // Add secondary network port
    TEST_ASSERT_TRUE(NetworkPortAdd(MAIN_NETWORK_PORT, &NetworkSecPortDesc));
    // Recieve data
    TEST_ASSERT_TRUE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));
    hasData = true;
    memcpy(in_buffer, udp_com_rx_barray, sizeof(udp_com_rx_barray));
    in_buff_size = sizeof(udp_com_rx_barray);
    NetworkCtrlMainProcess(MAIN_NETWORK_CTRL);
    TEST_ASSERT_FALSE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));
    // Read one byte
    TEST_ASSERT_TRUE(NetworkPortReadByte(MAIN_NETWORK_PORT, received_array));
    TEST_ASSERT_EQUAL_INT(0, strncmp(modelStr, (char *)received_array, 1));
    // Read other data
    TEST_ASSERT_TRUE(NetworkPortReadBuff(MAIN_NETWORK_PORT, &(received_array[1]), &received_size, sizeof(received_array), NULL));    
    TEST_ASSERT_EQUAL_INT(0, strcmp(modelStr, (char *)received_array));
    TEST_ASSERT_EQUAL_INT(strlen(modelStr), received_size + 1);
    TEST_ASSERT_TRUE(NetworkPortIsRxEmpty(MAIN_NETWORK_PORT));   
}