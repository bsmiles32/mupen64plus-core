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
static const char* si_regs_name[SI_REGS_COUNT] =
{
    "SI_DRAM_ADDR_REG",
    "SI_PIF_ADDR_RD64B_REG",
    "SI_RESERVED_1_REG",
    "SI_RESERVED_2_REG",
    "SI_PIF_ADDR_WR64B_REG",
    "SI_RESERVED_3_REG",
    "SI_STATUS_REG"
};
#endif



int init_si(struct si_controller* si)
{
    memset(si, 0, sizeof(*si));

    return 0;
}

/**
 * SI registers access functions
 **/
static inline uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_si_regs(struct si_controller* si,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = si_reg(address);

    *value = si->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", si_regs_name[reg], *value);

    return 0;
}

int write_si_regs(struct si_controller* si,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = si_reg(address);

    switch(reg)
    {
    case SI_DRAM_ADDR_REG:
        masked_write(&si->regs[SI_DRAM_ADDR_REG], value, mask);
        break;
    case SI_PIF_ADDR_RD64B_REG:
        masked_write(&si->regs[SI_PIF_ADDR_RD64B_REG], value, mask);
        dma_si_read();
        break;
    case SI_PIF_ADDR_WR64B_REG:
        masked_write(&si->regs[SI_PIF_ADDR_WR64B_REG], value, mask);
        dma_si_write();
        break;
    case SI_STATUS_REG:
        si->regs[SI_STATUS_REG] &= ~0x1000;
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        check_interupt();
        break;
    default:
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x v=%08x m=%08x",
                     address, value, mask);
        return -1;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", si_regs_name[reg], value);

    return 0;
}
