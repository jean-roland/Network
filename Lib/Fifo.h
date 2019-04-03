/**
 * \file Fifo.h
 * \brief Fifo management module
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
 
#ifndef _Fifo_h
#define _Fifo_h

// *** Libraries include ***
// Standard lib
#include <stdbool.h>
#include <stdint.h>
// Custom lib

// *** Definitions ***
// --- Public Types ---
typedef struct _fifo_desc {
    uint8_t *Buffer; // pointer to the fifo memory
    uint32_t ItemNb; // number of items
    uint32_t ItemSize; // item size
    uint32_t ReadCount; // counter of read items
    uint32_t WriteCount; // counter of written items
    uint32_t ReadIdx; // idx to the first data item to read
    uint32_t WriteIdx; // idx to the first free item space
} fifo_desc_t;

// --- Public Constants ---
// --- Public Variables ---
// --- Public Function Prototypes ---

/**
 * \fn void *FifoCreate(uint32_t itemNb, uint32_t itemSize)
 * \brief Creates a fifo
 *
 * \param itemNb number to rotate
 * \param itemSize number of bits of the rotation
 * \return void *: pointer to the created fifo
 */
void *FifoCreate(uint32_t itemNb, uint32_t itemSize);

/**
 * \fn uint32_t FifoItemCount(const fifo_desc_t *pFifoDesc)
 * \brief Return the number of items in a fifo
 *
 * \param pFifoDesc fifo descriptor
 * \return uint32_t: fifo current number of items
 */
uint32_t FifoItemCount(const fifo_desc_t *pFifoDesc);

/**
 * \fn uint32_t FifoFreeSpace(const fifo_desc_t *pFifoDesc)
 * \brief Return the number of free space in a fifo
 *
 * \param pFifoDesc fifo descriptor
 * \return uint32_t: fifo current number of free items
 */
uint32_t FifoFreeSpace(const fifo_desc_t *pFifoDesc);

/**
 * \fn void FifoFlush(fifo_desc_t *pFifoDesc)
 * \brief Flush the content of a fifo
 *
 * \param pFifoDesc fifo descriptor
 * \return void
 */
void FifoFlush(fifo_desc_t *pFifoDesc);

/**
 * \fn bool FifoWrite(fifo_desc_t *pFifoDesc, const void *src, uint32_t itemNb)
 * \brief Write items in a fifo
 *
 * \param pFifoDesc fifo descriptor
 * \param src pointer to the data to write
 * \param itemNb number of items to write
 * \return bool: true if success, false if we can't write one of the items (no write)
 */
bool FifoWrite(fifo_desc_t *pFifoDesc, const void *src, uint32_t itemNb);

/**
 * \fn bool FifoRead(fifo_desc_t *pFifoDesc, void *dest, uint32_t itemNb, bool consume)
 * \brief Read items in a Fifo
 *
 * \param pFifoDesc fifo descriptor
 * \param dest pointer to the data storage
 * \param itemNb number of items to read
 * \param consume true to consume the read data, false to leave the data intact
 * \return bool: true if the asked amount of items has been read, false otherwise (no read)
 */
bool FifoRead(fifo_desc_t *pFifoDesc, void *dest, uint32_t itemNb, bool consume);

/**
 * \fn bool FifoConsume(fifo_desc_t *pFifoDesc, uint32_t itemNb)
 * \brief Consume data from a fifo
 *
 * \param pFifoDesc fifo descriptor
 * \param itemNb number of items to consume
 * \return bool: true if the asked amount has been consumed, false otherwise (none consumed)
 */
bool FifoConsume(fifo_desc_t *pFifoDesc, uint32_t itemNb);

/*********************** END OF DEFINITIONS ***********************/
#endif // _Fifo_h