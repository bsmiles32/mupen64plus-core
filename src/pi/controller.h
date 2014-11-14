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

#ifndef M64P_PI_CONTROLLER_H
#define M64P_PI_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "sram.h"
#include "flashram.h"

struct ri_controller;
struct mi_controller;
struct si_controller; /* FIXME: For RDRAM detection hack */

/**
 * Parallel Interface registers 
 **/
enum pi_registers
{
    PI_DRAM_ADDR_REG,
    PI_CART_ADDR_REG,
    PI_RD_LEN_REG,
    PI_WR_LEN_REG,
    PI_STATUS_REG,
    PI_BSD_DOM1_LAT_REG,
    PI_BSD_DOM1_PWD_REG,
    PI_BSD_DOM1_PGS_REG,
    PI_BSD_DOM1_RLS_REG,
    PI_BSD_DOM2_LAT_REG,
    PI_BSD_DOM2_PWD_REG,
    PI_BSD_DOM2_PGS_REG,
    PI_BSD_DOM2_RLS_REG,
    PI_REGS_COUNT
};

/**
 * Controller
 **/
struct pi_controller
{
    uint32_t regs[PI_REGS_COUNT];

    struct ri_controller* ri;
    struct mi_controller* mi;
    struct si_controller* si; /* FIXME: For RDRAM detection hack */

    uint8_t* cart_rom;
    size_t cart_rom_size;
    uint32_t cart_last_write;

    uint8_t sram[SRAM_SIZE];

    struct flashram_controller flashram;
};



int init_pi(struct pi_controller* pi,
            struct ri_controller* ri,
            struct mi_controller* mi,
            struct si_controller* si,
            uint8_t* cart_rom, size_t cart_rom_size);


int read_pi_regs(struct pi_controller* pi,
                 uint32_t address, uint32_t* value);
int write_pi_regs(struct pi_controller* pi,
                  uint32_t address, uint32_t value, uint32_t mask);

int read_cart_rom(struct pi_controller* pi,
                  uint32_t address, uint32_t* value);

int write_cart_rom(struct pi_controller* pi,
                   uint32_t address, uint32_t value, uint32_t mask);
#endif

