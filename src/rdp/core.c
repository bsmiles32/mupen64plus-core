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
#include "plugin/plugin.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"

void do_SP_Task(void);

#if 0
static const char* dpc_regs_name[DPC_REGS_COUNT] =
{
    "DPC_START_REG",
    "DPC_END_REG",
    "DPC_CURRENT_REG",
    "DPC_STATUS_REG",
    "DPC_CLOCK_REG",
    "DPC_BUFBUSY_REG",
    "DPC_PIPEBUSY_REG",
    "DPC_TMEM_REG"
};

static const char* dps_regs_name[DPS_REGS_COUNT] =
{
    "DPS_TBIST_REG",
    "DPS_TEST_MODE_REG",
    "DPS_BUFTEST_ADDR_REG",
    "DPS_BUFTEST_DATA_REG"
};
#endif


int init_rdp(struct rdp_core* dp)
{
    memset(dp, 0, sizeof(*dp));

    return 0;
}

static inline uint32_t dpc_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_dpc_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value)
{
    uint32_t reg = dpc_reg(address);

    *value = dp->dpc_regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", dpc_regs_name[reg], *value);

    return 0;
}

int write_dpc_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t v;
    uint32_t reg = dpc_reg(address);

    switch(reg)
    {
    case DPC_START_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        dp->dpc_regs[DPC_CURRENT_REG] = dp->dpc_regs[DPC_START_REG];
        break;
    case DPC_END_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;

    case DPC_STATUS_REG:
        v = value & mask;

        /* clear / set xbus_dmem_dma */
        if (v & 0x1) dp->dpc_regs[DPC_STATUS_REG] &= ~0x1;
        if (v & 0x2) dp->dpc_regs[DPC_STATUS_REG] |= 0x1;

        /* clear freeze */
        if (v & 0x4)
        {
            dp->dpc_regs[DPC_STATUS_REG] &= ~0x2;

            /* see do_SP_task for more info */
            if ((g_sp.regs[SP_STATUS_REG] & 0x3) == 0) // !halt && !broke
                do_SP_Task();
        }
        /* set freeze */
        if (v & 0x8) dp->dpc_regs[DPC_STATUS_REG] |= 0x2;
        /* clear / set flush */
        if (v & 0x10) dp->dpc_regs[DPC_STATUS_REG] &= ~0x4;
        if (v & 0x20) dp->dpc_regs[DPC_STATUS_REG] |= 0x4;
        break;

    case DPC_CURRENT_REG:
    case DPC_CLOCK_REG:
    case DPC_BUFBUSY_REG:
    case DPC_PIPEBUSY_REG:
        /* read only */
        break;

    case DPC_TMEM_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x v=%08x m=%08x",
                     address, value, mask);
        return -1;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", dpc_regs_name[reg], value);
    return 0;
}



static inline uint32_t dps_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_dps_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value)
{
    uint32_t reg = dps_reg(address);

    *value = dp->dps_regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", dps_regs_name[reg], *value);

    return 0;
}

int write_dps_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = dps_reg(address);

    masked_write(&dp->dps_regs[reg], value, mask);

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", dps_regs_name[reg], value);

    return 0;
}

