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

#ifndef M64P_RI_CONTROLLER_H
#define M64P_RI_CONTROLLER_H

#include <stdint.h>

enum rdram_registers
{
    RDRAM_CONFIG_REG,
    RDRAM_DEVICE_ID_REG,
    RDRAM_DELAY_REG,
    RDRAM_MODE_REG,
    RDRAM_REF_INTERVAL_REG,
    RDRAM_REF_ROW_REG,
    RDRAM_RAS_INTERVAL_REG,
    RDRAM_MIN_INTERVAL_REG,
    RDRAM_ADDR_SELECT_REG,
    RDRAM_DEVICE_MANUF_REG,
    RDRAM_REGS_COUNT
};

enum ri_registers
{
    RI_MODE_REG,
    RI_CONFIG_REG,
    RI_CURRENT_LOAD_REG,
    RI_SELECT_REG,
    RI_REFRESH_REG,
    RI_LATENCY_REG,
    RI_ERROR_REG,
    RI_WERROR_REG,
    RI_REGS_COUNT
};

enum { RDRAM_MAX_SIZE = 0x800000 };

struct ri_controller
{
    uint32_t ram[RDRAM_MAX_SIZE/4];
    uint32_t rdram_regs[RDRAM_REGS_COUNT];
    uint32_t ri_regs[RI_REGS_COUNT];
};

int init_ri(struct ri_controller* ri);


int read_rdram_ram(struct ri_controller* ri,
                   uint32_t address, uint32_t* value);
int write_rdram_ram(struct ri_controller* ri,
                    uint32_t address, uint32_t value, uint32_t mask);

int read_ri_regs(struct ri_controller* ri,
                 uint32_t address, uint32_t* value);
int write_ri_regs(struct ri_controller* ri,
                  uint32_t address, uint32_t value, uint32_t mask);

int read_rdram_regs(struct ri_controller* ri,
                    uint32_t address, uint32_t* value);
int write_rdram_regs(struct ri_controller* ri,
                     uint32_t address, uint32_t value, uint32_t mask);

#endif

