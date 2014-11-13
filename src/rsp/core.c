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
#include "main/main.h"
#include "main/profile.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"
#include "rdp/core.h"

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


static void dma_sp_write(struct rsp_core* sp)
{
    unsigned int i,j;

    unsigned int l = sp->regs[SP_RD_LEN_REG];

    unsigned int length = ((l & 0xfff) | 7) + 1;
    unsigned int count = ((l >> 12) & 0xff) + 1;
    unsigned int skip = ((l >> 20) & 0xfff);
 
    unsigned int memaddr = sp->regs[SP_MEM_ADDR_REG] & 0xfff;
    unsigned int dramaddr = sp->regs[SP_DRAM_ADDR_REG] & 0xffffff;

    unsigned char *spmem = ((unsigned char*)sp->mem)
                         + (sp->regs[SP_MEM_ADDR_REG] & 0x1000);
    unsigned char *dram = (unsigned char*)sp->rdram->ram;

    for(j=0; j<count; j++) {
        for(i=0; i<length; i++) {
            spmem[memaddr^S8] = dram[dramaddr^S8];
            memaddr++;
            dramaddr++;
        }
        dramaddr+=skip;
    }
}

static void dma_sp_read(struct rsp_core* sp)
{
    unsigned int i,j;

    unsigned int l = sp->regs[SP_WR_LEN_REG];

    unsigned int length = ((l & 0xfff) | 7) + 1;
    unsigned int count = ((l >> 12) & 0xff) + 1;
    unsigned int skip = ((l >> 20) & 0xfff);

    unsigned int memaddr = sp->regs[SP_MEM_ADDR_REG] & 0xfff;
    unsigned int dramaddr = sp->regs[SP_DRAM_ADDR_REG] & 0xffffff;

    unsigned char *spmem = ((unsigned char*)sp->mem)
                         + (sp->regs[SP_MEM_ADDR_REG] & 0x1000);
    unsigned char *dram = (unsigned char*)sp->rdram->ram;

    for(j=0; j<count; j++) {
        for(i=0; i<length; i++) {
            dram[dramaddr^S8] = spmem[memaddr^S8];
            memaddr++;
            dramaddr++;
        }
        dramaddr+=skip;
    }
}


void do_SP_Task(struct rsp_core* sp)
{
    int save_pc = sp->regs2[SP_PC_REG] & ~0xfff;
    if (sp->mem[0xfc0/4] == 1)
    {
        if (sp->dp->dpc_regs[DPC_STATUS_REG] & 0x2) // DP frozen (DK64, BC)
        {
            // don't do the task now
            // the task will be done when DP is unfreezed (see write_dpc_regs)
            return;
        }

        // unprotecting old frame buffers
        unprotect_framebuffers(sp->dp);

        //gfx.processDList();
        sp->regs2[SP_PC_REG] &= 0xfff;
        timed_section_start(TIMED_SECTION_GFX);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_GFX);
        sp->regs2[SP_PC_REG] |= save_pc;
        new_frame();

        update_count();
        if (sp->mi->regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 1000);
        if (sp->mi->regs[MI_INTR_REG] & MI_INTR_DP)
            add_interupt_event(DP_INT, 1000);
        sp->mi->regs[MI_INTR_REG] &= ~(MI_INTR_SP | MI_INTR_DP);
        sp->regs[SP_STATUS_REG] &= ~0x303;

        // protecting new frame buffers
        protect_framebuffers(sp->dp);
    }
    else if (sp->mem[0xfc0/4] == 2)
    {
        //audio.processAList();
        sp->regs2[SP_PC_REG] &= 0xfff;
        timed_section_start(TIMED_SECTION_AUDIO);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_AUDIO);
        sp->regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (sp->mi->regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 4000/*500*/);
        sp->mi->regs[MI_INTR_REG] &= ~MI_INTR_SP;
        sp->regs[SP_STATUS_REG] &= ~0x303;
    }
    else
    {
        sp->regs2[SP_PC_REG] &= 0xfff;
        rsp.doRspCycles(0xFFFFFFFF);
        sp->regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (sp->mi->regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 0/*100*/);
        sp->mi->regs[MI_INTR_REG] &= ~MI_INTR_SP;
        sp->regs[SP_STATUS_REG] &= ~0x203;
    }
}






int init_rsp(struct rsp_core* sp,
             struct rdp_core* dp,
             struct mi_controller* mi,
             struct rdram_controller* rdram)
{
    memset(sp, 0, sizeof(*sp));
    
    sp->regs[SP_STATUS_REG] = 1;

    sp->dp = dp;
    sp->mi = mi;
    sp->rdram = rdram;

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
        dma_sp_write(sp);
        break;

    case SP_WR_LEN_REG:
        masked_write(&sp->regs[reg], value, mask);
        dma_sp_read(sp);
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
            sp->mi->regs[MI_INTR_REG] &= ~MI_INTR_SP;
            check_interupt();
        }
        /* set SP interrupt */
        if (v & 0x10)
        {
            sp->mi->regs[MI_INTR_REG] |= MI_INTR_SP;
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
            do_SP_Task(sp);
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

