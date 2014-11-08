/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mi.c                                                    *
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

#include "mi.h"

#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "memory/memory.h"
#include "r4300/r4300.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"


#if 0
static const char* mi_regs_name[MI_REGS_COUNT] =
{
    "MI_INIT_MODE_REG",
    "MI_VERSION_REG",
    "MI_INTR_REG",
    "MI_INTR_MASK_REG"
};
#endif

/* initialize MI controller */
int init_mi(struct mi_controller* mi)
{
    memset(mi, 0, sizeof(*mi));
    mi->regs[MI_VERSION_REG] = 0x02020102;

    return 0;
}

/**
 * MI registers access functions
 **/
static inline uint32_t mi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_mi_regs(struct mi_controller* mi,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = mi_reg(address);

    *value = mi->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", mi_regs_name[reg], *value);

    return 0;
}

/* write memory mapped MI register */
int write_mi_regs(struct mi_controller* mi,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = mi_reg(address);

    switch(reg)
    {
    case MI_INIT_MODE_REG:
        /* set init length */
        masked_write(&mi->regs[MI_INIT_MODE_REG], value & 0x7f, mask & 0x7f);
        value &= mask;

        /* clear / set init_mode */
        if (value & 0x80)  mi->regs[MI_INIT_MODE_REG] &= ~0x80;
        if (value & 0x100) mi->regs[MI_INIT_MODE_REG] |= 0x80;
        /* clear / set ebus_test_mode */
        if (value & 0x200) mi->regs[MI_INIT_MODE_REG] &= ~0x100;
        if (value & 0x400) mi->regs[MI_INIT_MODE_REG] |= 0x100;
        /* clear dp interrupt */
        if (value & 0x800)
        {
            mi->regs[MI_INTR_REG] &= ~MI_INTR_DP;
            check_interupt();
        }
        /* clear / set rdram_reg_mode */
        if (value & 0x1000) mi->regs[MI_INIT_MODE_REG] &= ~0x200;
        if (value & 0x2000) mi->regs[MI_INIT_MODE_REG] |= 0x200;
        break;

    case MI_INTR_MASK_REG:
        /* clear / set SP mask */
        if (value & 0x1)   mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_SP;
        if (value & 0x2)   mi->regs[MI_INTR_MASK_REG] |= MI_INTR_SP;
        /* clear / set SI mask */
        if (value & 0x4)   mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_SI;
        if (value & 0x8)   mi->regs[MI_INTR_MASK_REG] |= MI_INTR_SI;
        /* clear / set AI mask */
        if (value & 0x10)  mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_AI;
        if (value & 0x20)  mi->regs[MI_INTR_MASK_REG] |= MI_INTR_AI;
        /* clear / set VI mask */
        if (value & 0x40)  mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_VI;
        if (value & 0x80)  mi->regs[MI_INTR_MASK_REG] |= MI_INTR_VI;
        /* clear / set PI mask */
        if (value & 0x100) mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_PI;
        if (value & 0x200) mi->regs[MI_INTR_MASK_REG] |= MI_INTR_PI;
        /* clear / set DP mask */
        if (value & 0x400) mi->regs[MI_INTR_MASK_REG] &= ~MI_INTR_DP;
        if (value & 0x800) mi->regs[MI_INTR_MASK_REG] |= MI_INTR_DP;

        check_interupt();
        update_count();
        if (next_interupt <= g_cp0_regs[CP0_COUNT_REG]) gen_interupt();
        break;

    default:
        /* other regs are not writable */
        break;
    }
//    DebugMessage(M64MSG_WARNING, "%s <- %08x", mi_regs_name[reg], value);

    return 0;
}
