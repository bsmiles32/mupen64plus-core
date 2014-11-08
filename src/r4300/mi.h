/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mi.h                                                    *
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

#ifndef M64P_R4300_MI_H
#define M64P_R4300_MI_H

#include <stdint.h>

/**
 * MIPS Interface registers
 **/
enum mi_registers
{
    MI_INIT_MODE_REG,
    MI_VERSION_REG,
    MI_INTR_REG,
    MI_INTR_MASK_REG,
    MI_REGS_COUNT
};

/**
 * MI interrupt pattern
 **/
enum mi_intr_pattern
{
    MI_INTR_SP = 0x01,
    MI_INTR_SI = 0x02,
    MI_INTR_AI = 0x04,
    MI_INTR_VI = 0x08,
    MI_INTR_PI = 0x10,
    MI_INTR_DP = 0x20
};

/**
 * MIPS Interface controller
 **/
struct mi_controller
{
    uint32_t regs[MI_REGS_COUNT];
};


/**
 * MIPS Interface functions prototypes
 **/
int init_mi(struct mi_controller* mi);


int read_mi_regs(struct mi_controller* mi,
                 uint32_t address, uint32_t* value);
int write_mi_regs(struct mi_controller* mi,
                  uint32_t address, uint32_t value, uint32_t mask);
#endif
