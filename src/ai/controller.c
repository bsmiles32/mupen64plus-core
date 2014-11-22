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
#include "main/rom.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"
#include "vi/controller.h"


#if 0
static const char* ai_regs_name[AI_REGS_COUNT] =
{
    "AI_DRAM_ADDR_REG",
    "AI_LEN_REG",
    "AI_CONTROL_REG",
    "AI_STATUS_REG",
    "AI_DACRATE_REG",
    "AI_BITRATE_REG"
};
#endif

/**
 * Estimate remaining length of current DMA (if any)
 **/
static uint32_t get_remaining_dma_length(struct ai_controller* ai)
{
    unsigned int ai_event;
    unsigned int ai_delay;

    if (ai->fifo[0].duration == 0)
        return 0;

    update_count();
    ai_event = get_event(AI_INT);

    if (ai_event == 0)
        return 0;

    ai_delay = ai_event - g_cp0_regs[CP0_COUNT_REG];

    if (ai_delay >= 0x80000000)
        return 0;

    return (uint64_t)ai_delay * ai->fifo[0].length / ai->fifo[0].duration;
}

/**
 * Estimate duration needed to consume the given audio buffer
 **/
static unsigned int get_dma_duration(struct ai_controller* ai)
{
    unsigned int samples_per_sec =
        ROM_PARAMS.aidacrate / (1 + ai->regs[AI_DACRATE_REG]);

    return ((uint64_t)ai->regs[AI_LEN_REG] * ai->vi->duration * ROM_PARAMS.vilimit)
           / (4*samples_per_sec);
}


static void schedule_ai_event_end_of_dma(unsigned int delay)
{
    update_count();
    add_interupt_event(AI_INT, delay);
}

static void fifo_push(struct ai_controller* ai)
{
    unsigned int duration = get_dma_duration(ai);

    if (ai->regs[AI_STATUS_REG] & 0x40000000)
    {
        ai->regs[AI_STATUS_REG] |= 0x80000001;
        ai->fifo[1].length = ai->regs[AI_LEN_REG];
        ai->fifo[1].duration = duration;
    }
    else
    {
        ai->regs[AI_STATUS_REG] |= 0x40000000;
        ai->fifo[0].length = ai->regs[AI_LEN_REG];
        ai->fifo[0].duration = duration;

        schedule_ai_event_end_of_dma(duration);
    }
}

static void fifo_pop(struct ai_controller* ai)
{
    if (ai->regs[AI_STATUS_REG] & 0x80000001)
    {
        ai->regs[AI_STATUS_REG] &= ~0x80000001;
        ai->fifo[0] = ai->fifo[1];

        schedule_ai_event_end_of_dma(ai->fifo[0].duration);
    }
    else
    {
        ai->regs[AI_STATUS_REG] &= ~0x40000000;
    }

    raise_rcp_interrupt(ai->mi, MI_INTR_AI);
}

void ai_event_end_of_dma(struct ai_controller* ai)
{
    fifo_pop(ai);
}


int init_ai(struct ai_controller* ai,
            struct mi_controller* mi,
            struct vi_controller* vi)
{
    memset(ai, 0, sizeof(*ai));

    ai->mi = mi;
    ai->vi = vi;

    return 0;
}

/**
 * AI registers access functions
 **/
static inline uint32_t ai_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_ai_regs(struct ai_controller* ai,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = ai_reg(address);

    switch(reg)
    {
    case AI_LEN_REG:
        *value = get_remaining_dma_length(ai);
        break;

    case AI_STATUS_REG:
        *value = ai->regs[AI_STATUS_REG];
        break;

    default:
        /* other regs are not readable */
        break;
    }

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", ai_regs_name[reg], *value);

    return 0;
}

int write_ai_regs(struct ai_controller* ai,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    int dacrate_changed;
    uint32_t reg = ai_reg(address);

    switch(reg)
    {
    case AI_DRAM_ADDR_REG:
        masked_write(&ai->regs[reg], value & 0x00fffff8, mask);
        break;

    case AI_LEN_REG:
        masked_write(&ai->regs[reg], value & 0x0001fff8, mask);
        audio.aiLenChanged();
        fifo_push(ai);
        break;

    case AI_CONTROL_REG:
        masked_write(&ai->regs[reg], value & 0x1, mask);
        break;

    case AI_STATUS_REG:
        ai->mi->regs[MI_INTR_REG] &= ~MI_INTR_AI;
        check_interupt();
        break;

    case AI_DACRATE_REG:
        dacrate_changed = ((ai->regs[reg] & mask) != (value & mask));
        masked_write(&ai->regs[reg], value & 0x3fff, mask);

        if (dacrate_changed)
        {
            audio_ai_dacrate_changed();
        }
        break;

    case AI_BITRATE_REG:
        masked_write(&ai->regs[reg], value & 0xf, mask);
        break;

    default:
        /* other regs are not writable */
        break;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", ai_regs_name[reg], value);

    return 0;
}

void audio_ai_dacrate_changed(void)
{
    audio.aiDacrateChanged(ROM_PARAMS.systemtype);
}

