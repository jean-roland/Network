#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "unity.h"
#include "Fifo.h"
#include "mock_MemAlloc.h"

#define FIFO_SIZE 100

static fifo_desc_t *pTestFifo;
static bool init_srand;

void setUp(void) {
	// Init rand
	if (!init_srand) {
		srand(0x42424242);
		init_srand = true;
	}
	// Emulate memory allocation
	MemAllocCalloc_ExpectAndReturn(sizeof(fifo_desc_t), calloc(sizeof(fifo_desc_t), sizeof(uint8_t)));
	MemAllocMalloc_ExpectAndReturn(FIFO_SIZE, malloc(FIFO_SIZE));
	// Create fifo
	pTestFifo = FifoCreate(FIFO_SIZE, sizeof(uint8_t));
    TEST_ASSERT_TRUE_MESSAGE(pTestFifo != NULL,"Couldn't create fifo");
	
}

void tearDown(void) {
	// Free memory allocations
	free(pTestFifo->pBuffer);
	free(pTestFifo);
}

void test_fifo_size_management(void) {
	// Test fifo initial free size/item count
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE, FifoFreeSpace(pTestFifo));
	TEST_ASSERT_EQUAL_INT(0, FifoItemCount(pTestFifo));
	// Variables definition
	uint8_t dummy_val[FIFO_SIZE];
	int write_size = (rand() % FIFO_SIZE) + 1;
	int read_size = (rand() % write_size) + 1;
	printf("Test1: write_size: %d, read_size: %d\n", write_size, read_size);
	// Dummy Write & test free size/item count
	TEST_ASSERT_TRUE(FifoWrite(pTestFifo, dummy_val, write_size));
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE - write_size, FifoFreeSpace(pTestFifo));
	TEST_ASSERT_EQUAL_INT(write_size, FifoItemCount(pTestFifo));
	// Consume & test free size/item count
	TEST_ASSERT_TRUE(FifoConsume(pTestFifo, read_size));
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE - write_size + read_size, FifoFreeSpace(pTestFifo));
	TEST_ASSERT_EQUAL_INT(write_size - read_size, FifoItemCount(pTestFifo));	
	// Check free size/item count equivalence
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE - FifoItemCount(pTestFifo), FifoFreeSpace(pTestFifo));
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE - FifoFreeSpace(pTestFifo), FifoItemCount(pTestFifo));
	// Flush & test free size/item count
	TEST_ASSERT_TRUE(FifoFlush(pTestFifo));
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE, FifoFreeSpace(pTestFifo));
	TEST_ASSERT_EQUAL_INT(0, FifoItemCount(pTestFifo));
}

void test_fifo_read_write(void) {
	// Same value single write/read
	const uint8_t write_val = 0x55;
	for (uint8_t idx = 0; idx < FIFO_SIZE; idx++) {
		TEST_ASSERT_TRUE(FifoWrite(pTestFifo, &write_val, sizeof(uint8_t)));
	}
	for (uint8_t idx = 0; idx < FIFO_SIZE; idx++) {
		uint8_t read_val;
		TEST_ASSERT_TRUE(FifoRead(pTestFifo, &read_val, sizeof(uint8_t), true));
		TEST_ASSERT_EQUAL_INT(write_val, read_val);
	}
	// Incremental value single write/read
	for (uint8_t idx = 0; idx < FIFO_SIZE; idx++) {
		TEST_ASSERT_TRUE(FifoWrite(pTestFifo, &idx, sizeof(uint8_t)));
	}
	for (uint8_t idx = 0; idx < FIFO_SIZE; idx++) {
		uint8_t read_val;
		TEST_ASSERT_TRUE(FifoRead(pTestFifo, &read_val, sizeof(uint8_t), true));
		TEST_ASSERT_EQUAL_INT(idx, read_val);
	}
	// Random batch write/read without consuming
	uint8_t write_array[FIFO_SIZE];
	uint8_t read_array[FIFO_SIZE];
	memset(read_array, 0, sizeof(read_array));
	for (uint8_t idx = 0; idx < FIFO_SIZE; idx++) {
		write_array[idx] = (uint8_t)rand();
	}
	TEST_ASSERT_TRUE(FifoWrite(pTestFifo, &write_array, sizeof(write_array)));
	TEST_ASSERT_TRUE(FifoRead(pTestFifo, &read_array, sizeof(read_array), false));
	TEST_ASSERT_EQUAL_INT(memcmp(write_array, read_array, sizeof(write_array)), 0);
	// Test read consumption mechanism
	TEST_ASSERT_EQUAL_INT(FIFO_SIZE, FifoItemCount(pTestFifo));
	TEST_ASSERT_TRUE(FifoRead(pTestFifo, &read_array, sizeof(read_array), true));
	TEST_ASSERT_EQUAL_INT(memcmp(write_array, read_array, sizeof(write_array)), 0);
	TEST_ASSERT_EQUAL_INT(0, FifoItemCount(pTestFifo));
}