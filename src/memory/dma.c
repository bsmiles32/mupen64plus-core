/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dma.c                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "api/m64p_types.h"

#include "dma.h"
#include "memory.h"
#include "si/pif.h"
#include "pi/flashram.h"

#include "r4300/r4300.h"
#include "r4300/cached_interp.h"
#include "r4300/interupt.h"
#include "r4300/cp0.h"
#include "r4300/mi.h"
#include "r4300/ops.h"
#include "../r4300/new_dynarec/new_dynarec.h"

#include "rsp/core.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_config.h"
#include "api/config.h"
#include "api/callbacks.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

int delay_si = 0;

void dma_sp_write(void)
{
    unsigned int i,j;

    unsigned int l = g_sp.regs[SP_RD_LEN_REG];

    unsigned int length = ((l & 0xfff) | 7) + 1;
    unsigned int count = ((l >> 12) & 0xff) + 1;
    unsigned int skip = ((l >> 20) & 0xfff);
 
    unsigned int memaddr = g_sp.regs[SP_MEM_ADDR_REG] & 0xfff;
    unsigned int dramaddr = g_sp.regs[SP_DRAM_ADDR_REG] & 0xffffff;

    unsigned char *spmem = ((unsigned char*)g_sp.mem)
                         + (g_sp.regs[SP_MEM_ADDR_REG] & 0x1000);
    unsigned char *dram = (unsigned char*)g_rdram.ram;

    for(j=0; j<count; j++) {
        for(i=0; i<length; i++) {
            spmem[memaddr^S8] = dram[dramaddr^S8];
            memaddr++;
            dramaddr++;
        }
        dramaddr+=skip;
    }
}

void dma_sp_read(void)
{
    unsigned int i,j;

    unsigned int l = g_sp.regs[SP_WR_LEN_REG];

    unsigned int length = ((l & 0xfff) | 7) + 1;
    unsigned int count = ((l >> 12) & 0xff) + 1;
    unsigned int skip = ((l >> 20) & 0xfff);

    unsigned int memaddr = g_sp.regs[SP_MEM_ADDR_REG] & 0xfff;
    unsigned int dramaddr = g_sp.regs[SP_DRAM_ADDR_REG] & 0xffffff;

    unsigned char *spmem = ((unsigned char*)g_sp.mem)
                         + (g_sp.regs[SP_MEM_ADDR_REG] & 0x1000);
    unsigned char *dram = (unsigned char*)g_rdram.ram;

    for(j=0; j<count; j++) {
        for(i=0; i<length; i++) {
            dram[dramaddr^S8] = spmem[memaddr^S8];
            memaddr++;
            dramaddr++;
        }
        dramaddr+=skip;
    }
}

void dma_si_write(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_WR64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_write(): unknown SI use");
        stop=1;
    }

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        *((uint32_t*)(g_si.pif_ram + i)) = sl(g_rdram.ram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4]);
    }

    update_pif_write();
    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_mi.regs[MI_INTR_REG] |= MI_INTR_SI;
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

void dma_si_read(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_RD64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_read(): unknown SI use");
        stop=1;
    }

    update_pif_read();

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        g_rdram.ram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4] = sl(*(uint32_t*)(g_si.pif_ram + i));
    }

    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_mi.regs[MI_INTR_REG] |= MI_INTR_SI;
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

