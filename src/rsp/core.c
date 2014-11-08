/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - core.c                                                  *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2014 Bobby Smiles                                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "core.h"

#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "memory/memory.h"
#include "memory/dma.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"


void do_SP_Task(void);

#if 0
static const char* sp_regs_name[SP_REGS_COUNT] =
{
    "SP_MEM_ADDR_REG",
    "SP_DRAM_ADDR_REG",
    "SP_RD_LEN_REG",
    "SP_WR_LEN_REG",
    "SP_STATUS_REG",
    "SP_DMA_FULL_REG",
    "SP_DMA_BUSY_REG",
    "SP_SEMAPHORE_REG"
};

static const char* sp_regs2_name[SP_REGS2_COUNT] =
{
    "SP_PC_REG",
    "SP_IBIST_REG"
};
#endif


int init_rsp(struct rsp_core* sp)
{
    memset(sp, 0, sizeof(*sp));
    
    sp->regs[SP_STATUS_REG] = 1;

    return 0;
}



static inline uint32_t mem_address(uint32_t address)
{
    return (address & 0x1fff) >> 2;
}

int read_rsp_mem(struct rsp_core* sp,
                 uint32_t address, uint32_t* value)
{
    uint32_t addr = mem_address(address);
    
    *value = sp->mem[addr];

    return 0;
}

int write_rsp_mem(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = mem_address(address);

    masked_write(&sp->mem[addr], value, mask);

    return 0;
}



static inline uint32_t rsp_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_rsp_regs(struct rsp_core* sp,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg(address);

    *value = sp->regs[reg];

    if (reg == SP_SEMAPHORE_REG)
    {
        sp->regs[SP_SEMAPHORE_REG] = 1;
    }

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", sp_regs_name[reg], *value);

    return 0;
}

int write_rsp_regs(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t v;
    uint32_t reg = rsp_reg(address);

    switch(reg)
    {
    case SP_MEM_ADDR_REG:
    case SP_DRAM_ADDR_REG:
        masked_write(&sp->regs[reg], value, mask);
        break;

    case SP_RD_LEN_REG:
        masked_write(&sp->regs[reg], value, mask);
        dma_sp_write();
        break;

    case SP_WR_LEN_REG:
        masked_write(&sp->regs[reg], value, mask);
        dma_sp_read();
        break;

    case SP_STATUS_REG:
        v = value & mask;
        /* clear / set halt */
        if (v & 0x1) sp->regs[SP_STATUS_REG] &= ~0x1;
        if (v & 0x2) sp->regs[SP_STATUS_REG] |= 0x1;
        /* clear broke */
        if (v & 0x4) sp->regs[SP_STATUS_REG] &= ~0x2;
        /* clear SP interrupt */
        if (v & 0x8)
        {
            g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SP;
            check_interupt();
        }
        /* set SP interrupt */
        if (v & 0x10)
        {
            g_mi.regs[MI_INTR_REG] |= MI_INTR_SP;
            check_interupt();
        }
        /* clear / set single step */
        if (v & 0x20) sp->regs[SP_STATUS_REG] &= ~0x20;
        if (v & 0x40) sp->regs[SP_STATUS_REG] |= 0x20;
        /* clear / set interrupt on break */
        if (v & 0x80)  sp->regs[SP_STATUS_REG] &= ~0x40;
        if (v & 0x100) sp->regs[SP_STATUS_REG] |= 0x40;
        /* clear / set signal0 */
        if (v & 0x200) sp->regs[SP_STATUS_REG] &= ~0x80;
        if (v & 0x400) sp->regs[SP_STATUS_REG] |= 0x80;
        /* clear / set signal1 */
        if (v & 0x800)  sp->regs[SP_STATUS_REG] &= ~0x100;
        if (v & 0x1000) sp->regs[SP_STATUS_REG] |= 0x100;
        /* clear / set signal2 */
        if (v & 0x2000) sp->regs[SP_STATUS_REG] &= ~0x200;
        if (v & 0x4000) sp->regs[SP_STATUS_REG] |= 0x200;
        /* clear / set signal3 */
        if (v & 0x8000)  sp->regs[SP_STATUS_REG] &= ~0x400;
        if (v & 0x10000) sp->regs[SP_STATUS_REG] |= 0x400;
        /* clear / set signal4 */
        if (v & 0x20000) sp->regs[SP_STATUS_REG] &= ~0x800;
        if (v & 0x40000) sp->regs[SP_STATUS_REG] |= 0x800;
        /* clear / set signal5 */
        if (v & 0x80000)  sp->regs[SP_STATUS_REG] &= ~0x1000;
        if (v & 0x100000) sp->regs[SP_STATUS_REG] |= 0x1000;
        /* clear / set signal6 */
        if (v & 0x200000) sp->regs[SP_STATUS_REG] &= ~0x2000;
        if (v & 0x400000) sp->regs[SP_STATUS_REG] |= 0x2000;
        /* clear / set signal7 */
        if (v & 0x800000)  sp->regs[SP_STATUS_REG] &= ~0x4000;
        if (v & 0x1000000) sp->regs[SP_STATUS_REG] |= 0x4000;

        /* execute rsp task if needed */
        if (((v & 0x5) != 0) && ((sp->regs[SP_STATUS_REG] & 0x3) == 0))
            do_SP_Task();
        break;

    case SP_DMA_FULL_REG:
    case SP_DMA_BUSY_REG:
        /* non writable */
        break;

    case SP_SEMAPHORE_REG:
        sp->regs[SP_SEMAPHORE_REG] = 0;
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x v=%08x m=%08x",
                     address, value, mask);
        return -1;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", sp_regs_name[reg], value);
    return 0;
}



static inline uint32_t rsp_reg2(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_rsp_regs2(struct rsp_core* sp,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = rsp_reg2(address);

    *value = sp->regs2[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", sp_regs2_name[reg], *value);

    return 0;
}

int write_rsp_regs2(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rsp_reg2(address);

    masked_write(&sp->regs2[reg], value, mask);

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", sp_regs2_name[reg], value);

    return 0;
}

