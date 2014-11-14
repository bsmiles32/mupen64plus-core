/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - sram.c                                                  *
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

#include "sram.h"
#include "controller.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

#include "memory/memory.h"

#include <stdlib.h>
#include <string.h>

static char *get_sram_path(void)
{
    return formatstr("%s%s.sra", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void sram_format(struct pi_controller* pi)
{
    memset(pi->sram, 0, SRAM_SIZE);
}

static void sram_read_file(struct pi_controller* pi)
{
    char *filename = get_sram_path();

    sram_format(pi);
    switch (read_from_file(filename, pi->sram, SRAM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open sram file '%s' for reading", filename);
            sram_format(pi);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed on 32kb read from sram file '%s'", filename);
            sram_format(pi);
            break;
        default: break;
    }

    free(filename);
}

static void sram_write_file(struct pi_controller* pi)
{
    char *filename = get_sram_path();

    switch (write_to_file(filename, pi->sram, SRAM_SIZE))
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

static inline uint32_t sram_address(uint32_t address)
{
    return (address & 0xffff);
}


void dma_read_sram(struct pi_controller* pi)
{
    size_t i;
    size_t i_max = (pi->regs[PI_WR_LEN_REG] & 0xffffff) + 1;
    uint32_t sram_addr = sram_address(pi->regs[PI_CART_ADDR_REG]);
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG];
    uint8_t* ram = (uint8_t*)pi->ri->ram;

    sram_read_file(pi);

    for(i = 0; i < i_max; ++i)
    {
        ram[(dram_addr+i)^S8]= pi->sram[(sram_addr+i)^S8];
    }
}

void dma_write_sram(struct pi_controller* pi)
{
    size_t i;
    size_t i_max = (pi->regs[PI_RD_LEN_REG] & 0xffffff) + 1;
    uint32_t sram_addr = sram_address(pi->regs[PI_CART_ADDR_REG]);
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG];
    const uint8_t* ram = (uint8_t*)pi->ri->ram;

    sram_read_file(pi);

    for(i = 0; i < i_max; ++i)
    {
        pi->sram[(sram_addr+i)^S8] = ram[(dram_addr+i)^S8];
    }

    sram_write_file(pi);
}
