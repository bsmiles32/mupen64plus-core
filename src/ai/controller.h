/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - controller.h                                            *
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

#ifndef M64P_AI_CONTROLLER_H
#define M64P_AI_CONTROLLER_H

#include <stdint.h>

struct mi_controller;
struct vi_controller;

/**
 * Audio Interface registers 
 **/
enum ai_registers
{
    AI_DRAM_ADDR_REG,
    AI_LEN_REG,
    AI_CONTROL_REG,
    AI_STATUS_REG,
    AI_DACRATE_REG,
    AI_BITRATE_REG,
    AI_REGS_COUNT
};

/**
 * Audio DMA FIFO entry
 *
 * This structure keeps track of DMA length and estimated duration.
 * This allows to schedule Audio Interrupts and
 * adjust AI_LEN_REG reads according to estimated DMA progress
 **/
struct ai_dma
{
    uint32_t length;
    unsigned int duration;
};

/**
 * Controller
 **/
struct ai_controller
{
    uint32_t regs[AI_REGS_COUNT];
    struct ai_dma fifo[2]; /* 0: current, 1:pending */
    struct mi_controller* mi;
    struct vi_controller* vi;
};



int init_ai(struct ai_controller* ai,
            struct mi_controller* mi,
            struct vi_controller* vi);


int read_ai_regs(struct ai_controller* ai,
                 uint32_t address, uint32_t* value);
int write_ai_regs(struct ai_controller* ai,
                  uint32_t address, uint32_t value, uint32_t mask);

void ai_event_end_of_dma(struct ai_controller* ai);

void audio_ai_dacrate_changed(void);

#endif

