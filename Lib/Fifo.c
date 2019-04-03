/**
 * \file Fifo.c
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

// *** Libraries include ***
// Standard lib
#include <string.h>
// Custom lib
#include <MemAlloc.h>
#include <Fifo.h>

// *** Definitions ***
// --- Private Types ---
// --- Private Constants ---
// --- Private Function Prototypes ---
static uint32_t FifoGetItemCount(uint32_t readCount, uint32_t writeCount);
static uint32_t FifoGetFreeSpace(uint32_t totalCount, uint32_t readCount, uint32_t writeCount);

// --- Private Variables ---
// *** End Definitions ***

// *** Private Functions ***

/**
 * \fn inline static uint32_t FifoGetItemCount(uint32_t readCount, uint32_t writeCount)
 * \brief Return the number of items in a fifo
 *
 * \param readCount number of read items
 * \param writeCount number of written items
 * \return uint32_t: current number of items
 */
inline static uint32_t FifoGetItemCount(uint32_t readCount, uint32_t writeCount) {
    return((readCount <= writeCount) ? writeCount - readCount : (0xFFFFFFFF - readCount) + writeCount + 1);
}

/**
 * \fn inline static uint32_t FifoGetFreeSpace(uint32_t totalCount, uint32_t readCount, uint32_t writeCount)
 * \brief Return the number of free items in a fifo
 *
 * \param totalCount total number of items
 * \param readCount number of read items
 * \param writeCount number of written items
 * \return uint32_t: current number of free items
 */
inline static uint32_t FifoGetFreeSpace(uint32_t totalCount, uint32_t readCount, uint32_t writeCount) {
    return(totalCount - FifoGetItemCount(readCount, writeCount));
}

/**
 * \fn static bool FifoConsumeItems(fifo_desc_t *pFifoDesc, uint32_t itemNb)
 * \brief Consume items from a fifo
 *
 * \param pFifoDesc: fifo descriptor
 * \param itemNb: number of items to consume
 * \return true if the asked amount has been consumed, false otherwise (none consumed)
 */
static bool FifoConsumeItems(fifo_desc_t *pFifoDesc, uint32_t itemNb) {
    // Check if there is enough data in the fifo
    if(FifoGetItemCount(pFifoDesc->ReadCount, pFifoDesc->WriteCount) >= itemNb) {
        // Mark data as read, consuming it
        pFifoDesc->ReadCount += itemNb;
        // Check for roll-over
        if((pFifoDesc->ReadIdx + itemNb) >= pFifoDesc->ItemNb) {
            // Take roll-over into account
            itemNb -= pFifoDesc->ItemNb - pFifoDesc->ReadIdx;
            pFifoDesc->ReadIdx = 0;
        }
        // Finish read idx increment
        pFifoDesc->ReadIdx += itemNb;
        return true;
    }
    return false;
}

// *** Public Functions ***

void *FifoCreate(uint32_t itemNb, uint32_t itemSize) {
    // Fifo descriptor allocation
    fifo_desc_t *pFifoDesc = (fifo_desc_t *)MemAllocCalloc(sizeof(fifo_desc_t));
    // Fifo descriptor assignment 
    pFifoDesc->ItemNb = itemNb;
    pFifoDesc->ItemSize = itemSize;
    pFifoDesc->Buffer = MemAllocMalloc(itemNb * itemSize);
    return pFifoDesc;
}

uint32_t FifoItemCount(const fifo_desc_t *pFifoDesc) {
    // Check if pFifoDesc valid
    if(pFifoDesc != NULL)
        return FifoGetItemCount(pFifoDesc->ReadCount, pFifoDesc->WriteCount);
    else
        return 0;
}

uint32_t FifoFreeSpace(const fifo_desc_t *pFifoDesc) {
    //Check if pFifoDesc valid
    if(pFifoDesc != NULL)
        return FifoGetFreeSpace(pFifoDesc->ItemNb, pFifoDesc->ReadCount, pFifoDesc->WriteCount);
    else
        return 0;
}

void FifoFlush(fifo_desc_t *pFifoDesc) {
    // Reset the descriptor
    pFifoDesc->WriteIdx = 0;
    pFifoDesc->ReadIdx = 0;
    pFifoDesc->ReadCount = 0;
    pFifoDesc->WriteCount = 0;
}

bool FifoWrite(fifo_desc_t *pFifoDesc, const void *src, uint32_t itemNb) {
    // Check if pFifoDesc, src valid and if enough space to write
    if((pFifoDesc != NULL) && (src != NULL) && (FifoFreeSpace(pFifoDesc) >= itemNb)) {
        uint32_t srcOffset = 0;
        // Check for roll-over
        if ((pFifoDesc->WriteIdx + itemNb) >= pFifoDesc->ItemNb) {
            // Pre roll-over write data
            uint32_t ro_itemNb = pFifoDesc->ItemNb - pFifoDesc->WriteIdx;
            srcOffset = ro_itemNb * pFifoDesc->ItemSize;
            memcpy(&pFifoDesc->Buffer[pFifoDesc->WriteIdx * pFifoDesc->ItemSize], &((uint8_t *)src)[0], srcOffset);
            // Take roll-over into account
            pFifoDesc->WriteIdx = 0;
            itemNb -= ro_itemNb;
            pFifoDesc->WriteCount += ro_itemNb;
        }
        // Regular write data
        memcpy(&pFifoDesc->Buffer[pFifoDesc->WriteIdx * pFifoDesc->ItemSize], &((uint8_t *)src)[srcOffset], itemNb * pFifoDesc->ItemSize);
        pFifoDesc->WriteIdx += itemNb;
        // Mark data as written
        pFifoDesc->WriteCount += itemNb;
        return true;
    }
    return false;
}

bool FifoRead(fifo_desc_t *pFifoDesc, void *dest, uint32_t itemNb, bool consume) {
    // Check if pFifoDesc, dest valid and if enough data to read
    if ((pFifoDesc != NULL) && (dest != NULL) && (FifoGetItemCount(pFifoDesc->ReadCount, pFifoDesc->WriteCount) >= itemNb)) {
        uint32_t buffOffset = 0;
        uint32_t tmpReadIdx = pFifoDesc->ReadIdx;
        uint32_t tmpItemNb = itemNb;
        // Check for roll-over
        if ((tmpReadIdx + tmpItemNb) >= pFifoDesc->ItemNb) {
            // Pre roll-over read data
            uint32_t ro_itemNb = pFifoDesc->ItemNb - tmpReadIdx;
            buffOffset = ro_itemNb * pFifoDesc->ItemSize;
            memcpy(&((uint8_t *)dest)[0], &pFifoDesc->Buffer[tmpReadIdx * pFifoDesc->ItemSize], buffOffset);
            // Take roll-over into account
            tmpReadIdx = 0;
            tmpItemNb -= ro_itemNb;
        }
        // Regular data read
        memcpy(&((uint8_t *)dest)[buffOffset], &pFifoDesc->Buffer[tmpReadIdx * pFifoDesc->ItemSize], tmpItemNb * pFifoDesc->ItemSize);
        // Check if data is consumed
        if (consume) {
            FifoConsumeItems(pFifoDesc, itemNb);
        }
        return true;
    }
    return false;
}

bool FifoConsume(fifo_desc_t *pFifoDesc, uint32_t itemNb) {
    //Check if pFifoDesc valid
    if(pFifoDesc != NULL)
        return FifoConsumeItems(pFifoDesc, itemNb);
    else
        return false;
}