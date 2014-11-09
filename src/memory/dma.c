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
#include "pif.h"
#include "flashram.h"

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

static unsigned char sram[0x8000];
int delay_si = 0;

static char *get_sram_path(void)
{
    return formatstr("%s%s.sra", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void sram_format(void)
{
    memset(sram, 0, sizeof(sram));
}

static void sram_read_file(void)
{
    char *filename = get_sram_path();

    sram_format();
    switch (read_from_file(filename, sram, sizeof(sram)))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open sram file '%s' for reading", filename);
            sram_format();
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed on 32kb read from sram file '%s'", filename);
            sram_format();
            break;
        default: break;
    }

    free(filename);
}

static void sram_write_file(void)
{
    char *filename = get_sram_path();

    switch (write_to_file(filename, sram, sizeof(sram)))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open sram file '%s' for writing.", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed on 32kb write to sram file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

void dma_pi_read(void)
{
    unsigned int i;

    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
            && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
    {
        if (flashram_info.use_flashram != 1)
        {
            sram_read_file();

            for (i=0; i < (g_pi.regs[PI_RD_LEN_REG] & 0xFFFFFF)+1; i++)
            {
                sram[((g_pi.regs[PI_CART_ADDR_REG]-0x08000000)+i)^S8] =
                    ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8];
            }

            sram_write_file();

            flashram_info.use_flashram = -1;
        }
        else
        {
            dma_write_flashram();
        }
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Unknown dma read in dma_pi_read()");
    }

    g_pi.regs[PI_STATUS_REG] |= 1;
    update_count();
    add_interupt_event(PI_INT, 0x1000/*g_pi.regs[PI_RD_LEN_REG]*/);
}

void dma_pi_write(void)
{
    unsigned int longueur;
    int i;

    if (g_pi.regs[PI_CART_ADDR_REG] < 0x10000000)
    {
        if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
                && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
        {
            if (flashram_info.use_flashram != 1)
            {
                int i;

                sram_read_file();

                for (i=0; i<(int)(g_pi.regs[PI_WR_LEN_REG] & 0xFFFFFF)+1; i++)
                {
                    ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                        sram[(((g_pi.regs[PI_CART_ADDR_REG]-0x08000000)&0xFFFF)+i)^S8];
                }

                flashram_info.use_flashram = -1;
            }
            else
            {
                dma_read_flashram();
            }
        }
        else if (g_pi.regs[PI_CART_ADDR_REG] >= 0x06000000
                 && g_pi.regs[PI_CART_ADDR_REG] < 0x08000000)
        {
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "Unknown dma write 0x%x in dma_pi_write()", (int)g_pi.regs[PI_CART_ADDR_REG]);
        }

        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, /*g_pi.regs[PI_WR_LEN_REG]*/0x1000);

        return;
    }

    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x1fc00000) // for paper mario
    {
        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, 0x1000);

        return;
    }

    longueur = (g_pi.regs[PI_WR_LEN_REG] & 0xFFFFFF)+1;
    i = (g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF;
    longueur = (i + (int) longueur) > rom_size ?
               (rom_size - i) : longueur;
    longueur = (g_pi.regs[PI_DRAM_ADDR_REG] + longueur) > 0x7FFFFF ?
               (0x7FFFFF - g_pi.regs[PI_DRAM_ADDR_REG]) : longueur;

    if (i>rom_size || g_pi.regs[PI_DRAM_ADDR_REG] > 0x7FFFFF)
    {
        g_pi.regs[PI_STATUS_REG] |= 3;
        update_count();
        add_interupt_event(PI_INT, longueur/8);

        return;
    }

    if (r4300emu != CORE_PURE_INTERPRETER)
    {
        for (i=0; i<(int)longueur; i++)
        {
            unsigned long rdram_address1 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0x80000000;
            unsigned long rdram_address2 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0xa0000000;
            ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];

            if (!invalid_code[rdram_address1>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address1>>12]->block[(rdram_address1&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address1>>12] = 1;
                }
#ifdef NEW_DYNAREC
                invalidate_block(rdram_address1>>12);
#endif
            }
            if (!invalid_code[rdram_address2>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address2>>12]->block[(rdram_address2&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address2>>12] = 1;
                }
            }
        }
    }
    else
    {
        for (i=0; i<(int)longueur; i++)
        {
            ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];
        }
    }

    // Set the RDRAM memory size when copying main ROM code
    // (This is just a convenient way to run this code once at the beginning)
    if (g_pi.regs[PI_CART_ADDR_REG] == 0x10001000)
    {
        switch (CIC_Chip)
        {
        case 1:
        case 2:
        case 3:
        case 6:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                write_rdram_ram(&g_rdram, 0x318, 0x400000, ~0U);
            }
            else
            {
                write_rdram_ram(&g_rdram, 0x318, 0x800000, ~0U);
            }
            break;
        }
        case 5:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                write_rdram_ram(&g_rdram, 0x3f0, 0x400000, ~0U);
            }
            else
            {
                write_rdram_ram(&g_rdram, 0x3f0, 0x800000, ~0U);
            }
            break;
        }
        }
    }

    g_pi.regs[PI_STATUS_REG] |= 3;
    update_count();
    add_interupt_event(PI_INT, longueur/8);

    return;
}

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

    for (i=0; i<(64/4); i++)
    {
        PIF_RAM[i] = sl(g_rdram.ram[g_si.regs[SI_DRAM_ADDR_REG]/4+i]);
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

    for (i=0; i<(64/4); i++)
    {
        g_rdram.ram[g_si.regs[SI_DRAM_ADDR_REG]/4+i] = sl(PIF_RAM[i]);
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

