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

#include "flashram.h"
#include "sram.h"

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/m64p_config.h"
#include "api/callbacks.h"
#include "memory/memory.h"
#include "r4300/cached_interp.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"
#include "r4300/r4300.h"
#include "r4300/new_dynarec/new_dynarec.h"
#include "si/controller.h"

#include "main/main.h"
#include "main/rom.h"

#if 0
static const char* pi_regs_name[PI_REGS_COUNT] =
{
    "PI_DRAM_ADDR_REG",
    "PI_CART_ADDR_REG",
    "PI_RD_LEN_REG",
    "PI_WR_LEN_REG",
    "PI_STATUS_REG",
    "PI_BSD_DOM1_LAT_REG",
    "PI_BSD_DOM1_PWD_REG",
    "PI_BSD_DOM1_PGS_REG",
    "PI_BSD_DOM1_RLS_REG",
    "PI_BSD_DOM2_LAT_REG",
    "PI_BSD_DOM2_PWD_REG",
    "PI_BSD_DOM2_PGS_REG",
    "PI_BSD_DOM2_RLS_REG"
};
#endif

static void dma_pi_read(void)
{
    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
            && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
    {
        if (flashram_info.use_flashram != 1)
        {
            flashram_info.use_flashram = -1;

            dma_write_sram(&g_pi, (uint8_t*)g_rdram.ram);
        }
        else
        {
            dma_write_flashram();
        }
    }
    else
    {
        DebugMessage(M64MSG_WARNING, "Unknown dma read in dma_pi_read()");
    }

    g_pi.regs[PI_STATUS_REG] |= 1;
    update_count();
    add_interupt_event(PI_INT, 0x1000/*g_pi.regs[PI_RD_LEN_REG]*/);
}

static void dma_pi_write(void)
{
    unsigned int longueur;
    int i;

    if (g_pi.regs[PI_CART_ADDR_REG] < 0x10000000)
    {
        if (g_pi.regs[PI_CART_ADDR_REG] >= 0x08000000
                && g_pi.regs[PI_CART_ADDR_REG] < 0x08010000)
        {
            if (flashram_info.use_flashram != 1)
            {
                dma_read_sram(&g_pi, (uint8_t*)g_rdram.ram);

                flashram_info.use_flashram = -1;
            }
            else
            {
                dma_read_flashram();
            }
        }
        else if (g_pi.regs[PI_CART_ADDR_REG] >= 0x06000000
                 && g_pi.regs[PI_CART_ADDR_REG] < 0x08000000)
        {
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "Unknown dma write 0x%x in dma_pi_write()", (int)g_pi.regs[PI_CART_ADDR_REG]);
        }

        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, /*g_pi.regs[PI_WR_LEN_REG]*/0x1000);

        return;
    }

    if (g_pi.regs[PI_CART_ADDR_REG] >= 0x1fc00000) // for paper mario
    {
        g_pi.regs[PI_STATUS_REG] |= 1;
        update_count();
        add_interupt_event(PI_INT, 0x1000);

        return;
    }

    longueur = (g_pi.regs[PI_WR_LEN_REG] & 0xFFFFFF)+1;
    i = (g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF;
    longueur = (i + (int) longueur) > rom_size ?
               (rom_size - i) : longueur;
    longueur = (g_pi.regs[PI_DRAM_ADDR_REG] + longueur) > 0x7FFFFF ?
               (0x7FFFFF - g_pi.regs[PI_DRAM_ADDR_REG]) : longueur;

    if (i>rom_size || g_pi.regs[PI_DRAM_ADDR_REG] > 0x7FFFFF)
    {
        g_pi.regs[PI_STATUS_REG] |= 3;
        update_count();
        add_interupt_event(PI_INT, longueur/8);

        return;
    }

    if (r4300emu != CORE_PURE_INTERPRETER)
    {
        for (i=0; i<(int)longueur; i++)
        {
            unsigned long rdram_address1 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0x80000000;
            unsigned long rdram_address2 = g_pi.regs[PI_DRAM_ADDR_REG]+i+0xa0000000;
            ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];

            if (!invalid_code[rdram_address1>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address1>>12]->block[(rdram_address1&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address1>>12] = 1;
                }
#ifdef NEW_DYNAREC
                invalidate_block(rdram_address1>>12);
#endif
            }
            if (!invalid_code[rdram_address2>>12])
            {
                if (!blocks[rdram_address1>>12] ||
                    blocks[rdram_address2>>12]->block[(rdram_address2&0xFFF)/4].ops !=
                    current_instruction_table.NOTCOMPILED)
                {
                    invalid_code[rdram_address2>>12] = 1;
                }
            }
        }
    }
    else
    {
        for (i=0; i<(int)longueur; i++)
        {
            ((unsigned char*)g_rdram.ram)[(g_pi.regs[PI_DRAM_ADDR_REG]+i)^S8]=
                rom[(((g_pi.regs[PI_CART_ADDR_REG]-0x10000000)&0x3FFFFFF)+i)^S8];
        }
    }

    // Set the RDRAM memory size when copying main ROM code
    // (This is just a convenient way to run this code once at the beginning)
    if (g_pi.regs[PI_CART_ADDR_REG] == 0x10001000)
    {
        switch (g_si.cic)
        {
        case CIC_6101:
        case CIC_6102:
        case CIC_6103:
        case CIC_6106:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                write_rdram_ram(&g_rdram, 0x318, 0x400000, ~0U);
            }
            else
            {
                write_rdram_ram(&g_rdram, 0x318, 0x800000, ~0U);
            }
            break;
        }
        case CIC_6105:
        {
            if (ConfigGetParamInt(g_CoreConfig, "DisableExtraMem"))
            {
                write_rdram_ram(&g_rdram, 0x3f0, 0x400000, ~0U);
            }
            else
            {
                write_rdram_ram(&g_rdram, 0x3f0, 0x800000, ~0U);
            }
            break;
        }
        }
    }

    g_pi.regs[PI_STATUS_REG] |= 3;
    update_count();
    add_interupt_event(PI_INT, longueur/8);

    return;
}


int init_pi(struct pi_controller* pi, uint8_t* cart_rom, size_t cart_rom_size)
{
    memset(pi, 0, sizeof(*pi));

    pi->cart_rom = cart_rom;
    pi->cart_rom_size = cart_rom_size;

    return 0;
}

/**
 * PI registers access functions
 **/
static inline uint32_t pi_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_pi_regs(struct pi_controller* pi,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = pi_reg(address);

    *value = pi->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", pi_regs_name[reg], *value);

    return 0;
}

int write_pi_regs(struct pi_controller* pi,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = pi_reg(address);

    switch(reg)
    {
    case PI_DRAM_ADDR_REG:
    case PI_CART_ADDR_REG:
        masked_write(&pi->regs[reg], value, mask);
        break;

    case PI_RD_LEN_REG:
        masked_write(&pi->regs[reg], value, mask);
        dma_pi_read();
        break;

    case PI_WR_LEN_REG:
        masked_write(&pi->regs[reg], value, mask);
        dma_pi_write();
        break;


    case PI_STATUS_REG:
        value &= mask;
        if (value & 2)
        {
            g_mi.regs[MI_INTR_REG] &= ~MI_INTR_PI;
            check_interupt();
        }
        break;

    case PI_BSD_DOM1_LAT_REG:
    case PI_BSD_DOM1_PWD_REG:
    case PI_BSD_DOM1_PGS_REG:
    case PI_BSD_DOM1_RLS_REG:
    case PI_BSD_DOM2_LAT_REG:
    case PI_BSD_DOM2_PWD_REG:
    case PI_BSD_DOM2_PGS_REG:
    case PI_BSD_DOM2_RLS_REG:
        masked_write(&pi->regs[reg], value & 0xff, mask & 0xff);
        break;

    default:
        /* other regs are not writable */
        break;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", pi_regs_name[reg], value);

    return 0;
}


/**
 * Cart ROM access functions
 **/
static inline uint32_t rom_address(uint32_t address)
{
    return (address & 0x03ffffff);
}

int read_cart_rom(struct pi_controller* pi,
                  uint32_t address, uint32_t* value)
{
    uint32_t addr = rom_address(address);

    if (pi->cart_last_write)
    {
        *value = pi->cart_last_write;
        pi->cart_last_write = 0;
    }
    else
        *value = *(uint32_t*)(pi->cart_rom + addr);

    return 0;
}

int write_cart_rom(struct pi_controller* pi,
                   uint32_t address, uint32_t value, uint32_t mask)
{
    pi->cart_last_write = value;

    DebugMessage(M64MSG_VERBOSE, "Writing to ROM @%08x <- %08x", address, value);
    return 0;
}
