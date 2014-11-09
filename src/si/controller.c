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
#include "memory/memory.h"
#include "pif.h"
#include "r4300/r4300.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"

#if 0
static const char* si_regs_name[SI_REGS_COUNT] =
{
    "SI_DRAM_ADDR_REG",
    "SI_PIF_ADDR_RD64B_REG",
    "SI_RESERVED_1_REG",
    "SI_RESERVED_2_REG",
    "SI_PIF_ADDR_WR64B_REG",
    "SI_RESERVED_3_REG",
    "SI_STATUS_REG"
};
#endif

int delay_si = 0;

static void dma_si_write(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_WR64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_write(): unknown SI use");
        stop=1;
    }

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        *((uint32_t*)(g_si.pif_ram + i)) = sl(g_rdram.ram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4]);
    }

    update_pif_write();
    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_mi.regs[MI_INTR_REG] |= MI_INTR_SI;
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}

static void dma_si_read(void)
{
    int i;

    if (g_si.regs[SI_PIF_ADDR_RD64B_REG] != 0x1FC007C0)
    {
        DebugMessage(M64MSG_ERROR, "dma_si_read(): unknown SI use");
        stop=1;
    }

    update_pif_read();

    for (i = 0; i < PIF_RAM_SIZE; i += 4)
    {
        g_rdram.ram[(g_si.regs[SI_DRAM_ADDR_REG]+i)/4] = sl(*(uint32_t*)(g_si.pif_ram + i));
    }

    update_count();

    if (delay_si) {
        add_interupt_event(SI_INT, /*0x100*/0x900);
    } else {
        g_mi.regs[MI_INTR_REG] |= MI_INTR_SI;
        g_si.regs[SI_STATUS_REG] |= 0x1000; // INTERRUPT
        check_interupt();
    }
}



int init_si(struct si_controller* si, enum cic_type cic)
{
    memset(si, 0, sizeof(*si));

    si->cic = cic;

    return 0;
}

/**
 * SI registers access functions
 **/
static inline uint32_t si_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_si_regs(struct si_controller* si,
                 uint32_t address, uint32_t* value)
{
    uint32_t reg = si_reg(address);

    *value = si->regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", si_regs_name[reg], *value);

    return 0;
}

int write_si_regs(struct si_controller* si,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = si_reg(address);

    switch(reg)
    {
    case SI_DRAM_ADDR_REG:
        masked_write(&si->regs[SI_DRAM_ADDR_REG], value, mask);
        break;
    case SI_PIF_ADDR_RD64B_REG:
        masked_write(&si->regs[SI_PIF_ADDR_RD64B_REG], value, mask);
        dma_si_read();
        break;
    case SI_PIF_ADDR_WR64B_REG:
        masked_write(&si->regs[SI_PIF_ADDR_WR64B_REG], value, mask);
        dma_si_write();
        break;
    case SI_STATUS_REG:
        si->regs[SI_STATUS_REG] &= ~0x1000;
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        check_interupt();
        break;
    default:
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x v=%08x m=%08x",
                     address, value, mask);
        return -1;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", si_regs_name[reg], value);

    return 0;
}



static inline uint32_t pif_ram_address(uint32_t address)
{
    return ((address & 0xfffc) - 0x7c0);
}

int read_pif_ram(struct si_controller* si,
                 uint32_t address, uint32_t* value)
{
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x", address);
        return -1;
    }

    memcpy(value, si->pif_ram + addr, sizeof(*value));
    *value = sl(*value);

    DebugMessage(M64MSG_WARNING, "pif_ram[0x%02x] -> %08x", addr, *value);

    return 0;
}

int write_pif_ram(struct si_controller* si,
                  uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t addr = pif_ram_address(address);

    if (addr >= PIF_RAM_SIZE)
    {
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x", address);
        return -1;
    }

    masked_write((uint32_t*)&si->pif_ram[addr], sl(value), sl(mask));

    if ((addr == 0x3c) && ((mask & 0x000000ff) != 0))
    {
        if (si->pif_ram[0x3f] == 0x08)
        {
            si->pif_ram[0x3f] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
        {
            update_pif_write();
        }
    }

    DebugMessage(M64MSG_WARNING, "pif_ram[0x%02x] <- %08x & %08x", addr, value, mask);

    return 0;
}

enum cic_type detect_cic_type(const void* ipl3)
{
    unsigned long long crc = 0;
    size_t i = 0;

    for(i = 0; i < 0xfc0/4; ++i)
        crc += ((uint32_t*)ipl3)[i];

    switch(crc)
    {
        default:
            DebugMessage(M64MSG_WARNING, "Unknown CIC type (%08x)! using CIC 6102.", crc);
        case 0x000000D057C85244LL: return CIC_6102;
        case 0x000000D0027FDF31LL:
        case 0x000000CFFB631223LL: return CIC_6101;
        case 0x000000D6497E414BLL: return CIC_6103;
        case 0x0000011A49F60E96LL: return CIC_6105;
        case 0x000000D6D5BE5580LL: return CIC_6106;
    }

    return 0;
}
