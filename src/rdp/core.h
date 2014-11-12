/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - core.h                                                  *
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

#ifndef M64P_RDP_CORE_H
#define M64P_RDP_CORE_H

#include "stddef.h"
#include "stdint.h"

#include "api/m64p_plugin.h"

/**
 * Registers definition
 **/
enum dpc_registers
{
    DPC_START_REG,
    DPC_END_REG,
    DPC_CURRENT_REG,
    DPC_STATUS_REG,
    DPC_CLOCK_REG,
    DPC_BUFBUSY_REG,
    DPC_PIPEBUSY_REG,
    DPC_TMEM_REG,
    DPC_REGS_COUNT
};

enum dps_registers
{
    DPS_TBIST_REG,
    DPS_TEST_MODE_REG,
    DPS_BUFTEST_ADDR_REG,
    DPS_BUFTEST_DATA_REG,
    DPS_REGS_COUNT
};

enum { FB_INFO_COUNT = 6 };


struct rdp_core
{
    uint32_t dpc_regs[DPC_REGS_COUNT];
    uint32_t dps_regs[DPS_REGS_COUNT];

    FrameBufferInfo fb_infos[FB_INFO_COUNT];
    char fb_read_dirty[0x800000 >> 12];
    int fb_first_protection;
};



int init_rdp(struct rdp_core* dp);


int read_dpc_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value);
int write_dpc_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask);

int read_dps_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value);
int write_dps_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask);

void pre_framebuffer_read(struct rdp_core* dp, uint32_t address);
void pre_framebuffer_write(struct rdp_core* dp, uint32_t address, size_t size);

void protect_framebuffers(struct rdp_core* dp);
void unprotect_framebuffers(struct rdp_core* dp);

#endif

