/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - controller.c                                            *
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

#include "controller.h"

#include <string.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"

static const char* rdram_regs_name[RDRAM_REGS_COUNT] =
{
    "RDRAM_CONFIG_REG",
    "RDRAM_DEVICE_ID_REG",
    "RDRAM_DELAY_REG",
    "RDRAM_MODE_REG",
    "RDRAM_REF_INTERVAL_REG",
    "RDRAM_REF_ROW_REG",
    "RDRAM_RAS_INTERVAL_REG",
    "RDRAM_MIN_INTERVAL_REG",
    "RDRAM_ADDR_SELECT_REG",
    "RDRAM_DEVICE_MANUF_REG",

};

static const char* ri_regs_name[RI_REGS_COUNT] =
{
    "RI_MODE_REG",
    "RI_CONFIG_REG",
    "RI_CURRENT_LOAD_REG",
    "RI_SELECT_REG",
    "RI_REFRESH_REG",
    "RI_LATENCY_REG",
    "RI_ERROR_REG",
    "RI_WERROR_REG"
};



int init_rdram(struct rdram_controller* rdram)
{
    memset(rdram->ram, 0, RDRAM_MAX_SIZE);
    memset(rdram->rdram_regs, 0, RDRAM_REGS_COUNT*sizeof(rdram->rdram_regs[0]));
    memset(rdram->ri_regs, 0, RI_REGS_COUNT*sizeof(rdram->ri_regs[0]));

    return 0;
}



static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (mask & value) | (~mask & *dst);
}


/**
 * RDRAM RAM access functions
 **/
static inline uint32_t ram_address(uint32_t address)
{
    return (address & 0xffffff) >> 2;
}

int read_rdram_ram(struct rdram_controller* rdram,
                   uint32_t address, uint32_t* value)
{
    uint32_t addr = ram_address(address);

    *value = rdram->ram[addr];

    return 0;
}

int write_rdram_ram(struct rdram_controller* rdram,
                    uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = ram_address(address);

    masked_write(&rdram->ram[addr], value, mask);

    return 0;
}


/**
 * RDRAM registers access functions
 **/
static inline uint32_t rdram_reg(uint32_t address)
{
    return (address & 0x3ff) >> 2;
}

int read_rdram_regs(struct rdram_controller* rdram,
                    uint32_t address, uint32_t* value)
{
    uint32_t reg = rdram_reg(address);

    *value = rdram->rdram_regs[reg];

    DebugMessage(M64MSG_WARNING, "%s -> %08x", rdram_regs_name[reg], *value);

    return 0;
}

int write_rdram_regs(struct rdram_controller* rdram,
                     uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = rdram_reg(address);

    masked_write(&rdram->rdram_regs[reg], value, mask);
    
    DebugMessage(M64MSG_WARNING, "%s <- %08x",  rdram_regs_name[reg], value);

    return 0;
}


/**
 * RI registers access functions
 **/
static inline uint32_t ri_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_ri_regs(struct rdram_controller* rdram,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = ri_reg(address);

    *value = rdram->ri_regs[reg];

    DebugMessage(M64MSG_WARNING, "%s -> %08x", ri_regs_name[reg], *value);

    return 0;
}

int write_ri_regs(struct rdram_controller* rdram,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = ri_reg(address);

    masked_write(&rdram->ri_regs[reg], value, mask);

    DebugMessage(M64MSG_WARNING, "%s <- %08x", ri_regs_name[reg], value);

    return 0;
}
