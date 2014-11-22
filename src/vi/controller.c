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
#include "main/main.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"

#if 0
static const char* vi_regs_name[VI_REGS_COUNT] =
{
    "VI_STATUS_REG",
    "VI_ORIGIN_REG",
    "VI_WIDTH_REG",
    "VI_V_INTR_REG",
    "VI_CURRENT_REG",
    "VI_BURST_REG",
    "VI_V_SYNC_REG",
    "VI_H_SYNC_REG",
    "VI_LEAP_REG",
    "VI_H_START_REG",
    "VI_V_START_REG",
    "VI_V_BURST_REG",
    "VI_X_SCALE_REG",
    "VI_Y_SCALE_REG"
};
#endif


int init_vi(struct vi_controller* vi,
            struct mi_controller* mi)
{
    memset(vi, 0, sizeof(*vi));

    /* XXX: first VI is triggered after 5000 ticks ? */
    vi->next_vi = vi->duration = 5000;

    vi->mi = mi;

    return 0;
}

/**
 * VI registers access functions
 **/
static inline uint32_t vi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_vi_regs(struct vi_controller* vi,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = vi_reg(address);

    if (reg == VI_CURRENT_REG)
    {
        /* VI_CURRENT_REG should reflect current timing and field */
        update_count();
        vi->regs[VI_CURRENT_REG] = (vi->duration - (vi->next_vi - g_cp0_regs[CP0_COUNT_REG]))/1500;
        vi->regs[VI_CURRENT_REG] = (vi->regs[VI_CURRENT_REG] & (~1)) | vi->field;
    }

    *value = vi->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", vi_regs_name[reg], *value);

    return 0;
}

int write_vi_regs(struct vi_controller* vi,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    int notify_plugin;
    uint32_t reg = vi_reg(address);

    switch(reg)
    {
    case VI_STATUS_REG:
        notify_plugin = ((vi->regs[VI_STATUS_REG] & mask) != (value & mask));
        masked_write(&vi->regs[VI_STATUS_REG], value, mask);

        if (notify_plugin)
            gfx_vi_status_changed();
        break;

    case VI_WIDTH_REG:
        notify_plugin = ((vi->regs[VI_WIDTH_REG] & mask) != (value & mask));
        masked_write(&vi->regs[VI_WIDTH_REG], value, mask);

        if (notify_plugin)
            gfx_vi_width_changed();
        break;

    case VI_CURRENT_REG:
        vi->mi->regs[MI_INTR_REG] &= ~MI_INTR_VI;
        check_interupt();
        break;

    case VI_ORIGIN_REG:
    case VI_V_INTR_REG:
    case VI_BURST_REG:
    case VI_V_SYNC_REG:
    case VI_H_SYNC_REG:
    case VI_LEAP_REG:
    case VI_H_START_REG:
    case VI_V_START_REG:
    case VI_V_BURST_REG:
    case VI_X_SCALE_REG:
    case VI_Y_SCALE_REG:
        masked_write(&vi->regs[reg], value, mask);
        break;
    default:
        /* other regs are not writable */
        break;
    }
//    DebugMessage(M64MSG_WARNING, "%s <- %08x", vi_regs_name[reg], value);

    return 0;
}


void vi_event_vertical_interrupt(struct vi_controller* vi)
{
    gfx_vi_update_screen();

    /* let the main module do its things (cheats, print some stats, pause, sleep) */
    main_vi_event_callback();

    /* update VI field (0 if non interlaced, toggle if interlaced) */
    vi->field = ((vi->regs[VI_STATUS_REG] & 0x40) >> 6) & ~vi->field;

    /* update next VI interrupt timings */
    vi->duration = (vi->regs[VI_V_SYNC_REG] == 0)
                  ? 500000
                  : (vi->regs[VI_V_SYNC_REG] + 1)*1500;
    vi->next_vi += vi->duration;

    add_interupt_event_count(VI_INT, vi->next_vi);

    raise_rcp_interrupt(vi->mi, MI_INTR_VI);
}


void gfx_vi_update_screen(void)
{
    gfx.updateScreen();
}

void gfx_vi_status_changed(void)
{
    gfx.viStatusChanged();
}

void gfx_vi_width_changed(void)
{
    gfx.viWidthChanged();
}
