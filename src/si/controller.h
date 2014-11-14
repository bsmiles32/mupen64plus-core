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

#ifndef M64P_SI_CONTROLLER_H
#define M64P_SI_CONTROLLER_H

#include <stdint.h>

#include "eeprom.h"
#include "mempack.h"

struct rdram_controller;
struct mi_controller;

/**
 * Serial Interface registers 
 **/
enum si_registers
{
    SI_DRAM_ADDR_REG,
    SI_PIF_ADDR_RD64B_REG,
    SI_RESERVED_1_REG,
    SI_RESERVED_2_REG,
    SI_PIF_ADDR_WR64B_REG,
    SI_RESERVED_3_REG,
    SI_STATUS_REG,
    SI_REGS_COUNT
};

enum { PIF_RAM_SIZE = 0x40 };

enum cic_type
{
    CIC_6101 = 1,
    CIC_6102 = 2,
    CIC_6103 = 3,
    CIC_6105 = 5,
    CIC_6106 = 6
};

/**
 * Controller
 **/
struct si_controller
{
    uint32_t regs[SI_REGS_COUNT];
    uint8_t pif_ram[PIF_RAM_SIZE];
    struct eeprom_controller eeprom;
    struct mempack_controller mempack;
    struct rdram_controller* rdram;
    struct mi_controller* mi;
    enum cic_type cic;
};


int init_si(struct si_controller* si,
            struct rdram_controller* rdram,
            struct mi_controller* mi,
            enum cic_type cic);


int read_si_regs(struct si_controller* si,
                 uint32_t address, uint32_t* value);
int write_si_regs(struct si_controller* si,
                  uint32_t address, uint32_t value, uint32_t mask);

int read_pif_ram(struct si_controller* si,
                 uint32_t address, uint32_t* value);
int write_pif_ram(struct si_controller* si,
                  uint32_t address, uint32_t value, uint32_t mask);


enum cic_type detect_cic_type(const void* ipl3);

#endif

