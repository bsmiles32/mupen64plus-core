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

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "memory/memory.h"
#include "memory/dma.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"

#if 0
static const char* pi_regs_name[PI_REGS_COUNT] =
{
    "PI_DRAM_ADDR_REG",
    "PI_CART_ADDR_REG",
    "PI_RD_LEN_REG",
    "PI_WR_LEN_REG",
    "PI_STATUS_REG",
    "PI_BSD_DOM1_LAT_REG",
    "PI_BSD_DOM1_PWD_REG",
    "PI_BSD_DOM1_PGS_REG",
    "PI_BSD_DOM1_RLS_REG",
    "PI_BSD_DOM2_LAT_REG",
    "PI_BSD_DOM2_PWD_REG",
    "PI_BSD_DOM2_PGS_REG",
    "PI_BSD_DOM2_RLS_REG"
};
#endif



int init_pi(struct pi_controller* pi)
{
    memset(pi, 0, sizeof(*pi));

    return 0;
}

/**
 * PI registers access functions
 **/
static inline uint32_t pi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_pi_regs(struct pi_controller* pi,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = pi_reg(address);

    *value = pi->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", pi_regs_name[reg], *value);

    return 0;
}

int write_pi_regs(struct pi_controller* pi,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = pi_reg(address);

    switch(reg)
    {
    case PI_DRAM_ADDR_REG:
    case PI_CART_ADDR_REG:
        masked_write(&pi->regs[reg], value, mask);
        break;

    case PI_RD_LEN_REG:
        masked_write(&pi->regs[reg], value, mask);
        dma_pi_read();
        break;

    case PI_WR_LEN_REG:
        masked_write(&pi->regs[reg], value, mask);
        dma_pi_write();
        break;


    case PI_STATUS_REG:
        value &= mask;
        if (value & 2)
        {
            g_mi.regs[MI_INTR_REG] &= ~MI_INTR_PI;
            check_interupt();
        }
        break;

    case PI_BSD_DOM1_LAT_REG:
    case PI_BSD_DOM1_PWD_REG:
    case PI_BSD_DOM1_PGS_REG:
    case PI_BSD_DOM1_RLS_REG:
    case PI_BSD_DOM2_LAT_REG:
    case PI_BSD_DOM2_PWD_REG:
    case PI_BSD_DOM2_PGS_REG:
    case PI_BSD_DOM2_RLS_REG:
        masked_write(&pi->regs[reg], value & 0xff, mask & 0xff);
        break;

    default:
        /* other regs are not writable */
        break;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", pi_regs_name[reg], value);

    return 0;
}
