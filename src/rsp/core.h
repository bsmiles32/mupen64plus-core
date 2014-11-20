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

#ifndef M64P_RSP_CORE_H
#define M64P_RSP_CORE_H

#include <stdint.h>

struct rdp_core;
struct mi_controller;
struct ri_controller;

/**
 * Registers definition
 **/
enum sp_registers
{
    SP_MEM_ADDR_REG,
    SP_DRAM_ADDR_REG,
    SP_RD_LEN_REG,
    SP_WR_LEN_REG,
    SP_STATUS_REG,
    SP_DMA_FULL_REG,
    SP_DMA_BUSY_REG,
    SP_SEMAPHORE_REG,
    SP_REGS_COUNT
};

enum sp_registers2
{
    SP_PC_REG,
    SP_IBIST_REG,
    SP_REGS2_COUNT
};


enum { SP_MEM_SIZE = 0x2000 };

struct rsp_core
{
    uint32_t regs[SP_REGS_COUNT];
    uint32_t regs2[SP_REGS2_COUNT];
    uint32_t mem[SP_MEM_SIZE/4];

    struct rdp_core* dp;
    struct mi_controller* mi;
    struct ri_controller* ri;
};



int init_rsp(struct rsp_core* sp,
             struct rdp_core* dp,
             struct mi_controller* mi,
             struct ri_controller* ri);

int read_rsp_mem(struct rsp_core* sp,
                 uint32_t address, uint32_t* value);
int write_rsp_mem(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask);

int read_rsp_regs(struct rsp_core* sp,
                 uint32_t address, uint32_t* value);
int write_rsp_regs(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask);

int read_rsp_regs2(struct rsp_core* sp,
                 uint32_t address, uint32_t* value);
int write_rsp_regs2(struct rsp_core* sp,
                  uint32_t address, uint32_t value, uint32_t mask);

void do_SP_Task(struct rsp_core* sp);

#endif

