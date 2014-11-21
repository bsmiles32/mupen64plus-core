/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.c                                                *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
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

#include "memory.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "main/main.h"
#include "main/rom.h"

#include "r4300/cached_interp.h"
#include "r4300/ops.h"
#include "r4300/r4300.h"
#include "r4300/recomp.h"
#include "r4300/tlb.h"



/* hash tables of read functions, write functions */
static int (*readmem[0x10000])(uint32_t, uint32_t*) ;
static int (*writemem[0x10000])(uint32_t, uint32_t, uint32_t);


static int read_nothing(uint32_t address, uint32_t* value)
{
    /* XXX: document */
    if (address == 0xa5000508) *value = 0xFFFFFFFF;
    else *value = 0;

    return 0;
}

static int write_nothing(uint32_t address, uint32_t value, uint32_t mask)
{
    return 0;
}



static int read_nomem(uint32_t address, uint32_t* value)
{
    address = virtual_to_physical_address(address,0);

    if (address == 0x00000000)
        return -1;

    return read_word(address, value);
}

static int write_nomem(uint32_t address, uint32_t value, uint32_t mask)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops !=
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;

    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000)
        return -1;

    return write_word(address, value, mask);
}



int read_rdram(uint32_t address, uint32_t* value)
{
    return read_rdram_ram(&g_ri, address, value);
}

int write_rdram(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rdram_ram(&g_ri, address, value, mask);
}

static int read_rdramreg(uint32_t address, uint32_t* value)
{
    return read_rdram_regs(&g_ri, address, value);
}

static int write_rdramreg(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rdram_regs(&g_ri, address, value, mask);
}



static int read_rsp_memory(uint32_t address, uint32_t* value)
{
    return read_rsp_mem(&g_sp, address, value);
}

static int write_rsp_memory(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rsp_mem(&g_sp, address, value, mask);
}

static int read_rsp_reg(uint32_t address, uint32_t* value)
{
    return read_rsp_regs(&g_sp, address, value);
}

static int write_rsp_reg(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_rsp_regs(&g_sp, address, value, mask);
}

static int read_rsp(uint32_t address, uint32_t* value)
{
    return read_rsp_regs2(&g_sp, address, value);
}

static int write_rsp(uint32_t address, uint32_t value, uint32_t mask)
{
   return  write_rsp_regs2(&g_sp, address, value, mask);
}



static int read_dp(uint32_t address, uint32_t* value)
{
    return read_dpc_regs(&g_dp, address, value);
}

static int write_dp(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_dpc_regs(&g_dp, address, value, mask);
}

static int read_dps(uint32_t address, uint32_t* value)
{
    return read_dps_regs(&g_dp, address, value);
}

static int write_dps(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_dps_regs(&g_dp, address, value, mask);
}

int read_rdramFB(uint32_t address, uint32_t* value)
{
    pre_framebuffer_read(&g_dp, address);
    return read_rdram(address, value);
}

int write_rdramFB(uint32_t address, uint32_t value, uint32_t mask)
{
    pre_framebuffer_write(&g_dp, address, 4);
    return write_rdram(address, value, mask);
}



static int read_mi(uint32_t address, uint32_t* value)
{
    return read_mi_regs(&g_mi, address, value);
}

static int write_mi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_mi_regs(&g_mi, address, value, mask);
}



static int read_vi(uint32_t address, uint32_t* value)
{
    return read_vi_regs(&g_vi, address, value);
}

static int write_vi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_vi_regs(&g_vi, address, value, mask);
}



static int read_ai(uint32_t address, uint32_t* value)
{
    return read_ai_regs(&g_ai, address, value);
}

static int write_ai(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_ai_regs(&g_ai, address, value, mask);
}



static int read_pi(uint32_t address, uint32_t* value)
{
    return read_pi_regs(&g_pi, address, value);
}

static int write_pi(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_pi_regs(&g_pi, address, value, mask);
}



static int read_ri(uint32_t address, uint32_t* value)
{
    return read_ri_regs(&g_ri, address, value);
}

static int write_ri(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_ri_regs(&g_ri, address, value, mask);
}



static int read_si(uint32_t address, uint32_t* value)
{
    return read_si_regs(&g_si, address, value);
}

static int write_si(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_si_regs(&g_si, address, value, mask);
}



static int read_flashram_status(uint32_t address, uint32_t* value)
{
    return pi_read_flashram_status(&g_pi, address, value);
}

static int write_flashram_dummy(uint32_t address, uint32_t value, uint32_t mask)
{
    return 0;
}

static int write_flashram_command(uint32_t address, uint32_t value, uint32_t mask)
{
    return pi_write_flashram_command(&g_pi, address, value, mask);
}




static int read_rom(uint32_t address, uint32_t* value)
{
    return read_cart_rom(&g_pi, address, value);
}

static int write_rom(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_cart_rom(&g_pi, address, value, mask);
}



static int read_pif(uint32_t address, uint32_t* value)
{
    return read_pif_ram(&g_si, address, value);
}

static int write_pif(uint32_t address, uint32_t value, uint32_t mask)
{
    return write_pif_ram(&g_si, address, value, mask);
}



#define R(x) read_ ## x
#define W(x) write_ ## x
#define X(x) R(x), W(x)

int init_memory(void)
{
    int i;

    /* clear mappings */
    for(i = 0; i < 0x10000; ++i)
    {
        map_region(i, X(nomem));
    }


    /* map RDRAM memory */
    for(i = 0; i < /*0x40*/0x80; ++i)
    {
        map_region(0x8000+i, X(rdram));
        map_region(0xa000+i, X(rdram));
    }
    for(i =/*0x40*/0x80; i < 0x3f0; ++i)
    {
        map_region(0x8000+i, X(nothing));
        map_region(0xa000+i, X(nothing));
    }

    /* map RDRAM registers */
    map_region(0x83f0, X(rdramreg));
    map_region(0xa3f0, X(rdramreg));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x83f0+i, X(nothing));
        map_region(0xa3f0+i, X(nothing));
    }

    /* map SP memory */
    map_region(0x8400, X(rsp_memory));
    map_region(0xa400, X(rsp_memory));
    for(i = 1; i < 4; ++i)
    {
        map_region(0x8400+i, X(nothing));
        map_region(0xa400+i, X(nothing));
    }

    /* map SP registers (1) */
    map_region(0x8404, X(rsp_reg));
    map_region(0xa404, X(rsp_reg));
    for(i = 5; i < 8; ++i)
    {
        map_region(0x8400+i, X(nothing));
        map_region(0xa400+i, X(nothing));
    }

    /* map SP registers (2) */
    map_region(0x8408, X(rsp));
    map_region(0xa408, X(rsp));
    for(i = 9; i < 0x10; ++i)
    {
        map_region(0x8400+i, X(nothing));
        map_region(0xa400+i, X(nothing));
    }

    /* map DP registers (DPC) */
    map_region(0x8410, X(dp));
    map_region(0xa410, X(dp));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8410+i, X(nothing));
        map_region(0xa410+i, X(nothing));
    }

    /* map DP registers (DPS) */
    map_region(0x8420, X(dps));
    map_region(0xa420, X(dps));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8420+i, X(nothing));
        map_region(0xa420+i, X(nothing));
    }

    /* map MIPS Interface registers */
    map_region(0x8430, X(mi));
    map_region(0xa430, X(mi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8430+i, X(nothing));
        map_region(0xa430+i, X(nothing));
    }

    /* map VI registers */
    map_region(0x8440, X(vi));
    map_region(0xa440, X(vi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8440+i, X(nothing));
        map_region(0xa440+i, X(nothing));
    }

    /* map AI registers */
    map_region(0x8450, X(ai));
    map_region(0xa450, X(ai));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8450+i, X(nothing));
        map_region(0xa450+i, X(nothing));
    }

    /* map PI registers */
    map_region(0x8460, X(pi));
    map_region(0xa460, X(pi));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8460+i, X(nothing));
        map_region(0xa460+i, X(nothing));
    }

    /* map RI registers */
    map_region(0x8470, X(ri));
    map_region(0xa470, X(ri));
    for(i = 1; i < 0x10; ++i)
    {
        map_region(0x8470+i, X(nothing));
        map_region(0xa470+i, X(nothing));
    }

    /* map SI registers */
    map_region(0x8480, X(si));
    map_region(0xa480, X(si));
    for(i = 0x481; i < 0x800; ++i)
    {
        map_region(0x8000+i, X(nothing));
        map_region(0xa000+i, X(nothing));
    }

    /* map flashram / sram */
    map_region(0x8800, R(flashram_status), W(flashram_dummy));
    map_region(0xa800, R(flashram_status), W(flashram_dummy));
    map_region(0x8801, R(nothing), W(flashram_command));
    map_region(0xa801, R(nothing), W(flashram_command));
    for(i = 0x802; i < 0x1000; ++i)
    {
        map_region(0x8000+i, X(nothing));
        map_region(0xa000+i, X(nothing));
    }

    /* map ROM */
    for(i = 0; i < (rom_size >> 16); ++i)
    {
        map_region(0x9000+i, R(rom), W(nothing));
        map_region(0xb000+i, X(rom));
    }
    for(i = (rom_size >> 16); i < 0xfc0; ++i)
    {
        map_region(0x9000+i, X(nothing));
        map_region(0xb000+i, X(nothing));
    }

    /* map PIF RAM */
    map_region(0x9fc0, X(pif));
    map_region(0xbfc0, X(pif));
    for(i = 0xfc1; i < 0x1000; ++i)
    {
        map_region(0x9000+i, X(nothing));
        map_region(0xb000+i, X(nothing));
    }

    init_ri(&g_ri);
    init_rsp(&g_sp, &g_dp, &g_mi, &g_ri);
    init_rdp(&g_dp, &g_mi, &g_sp);
    init_mi(&g_mi);
    init_vi(&g_vi, &g_mi);
    init_ai(&g_ai, &g_mi, &g_vi);
    init_pi(&g_pi, &g_ri, &g_mi, &g_si, rom, rom_size);
    enum cic_type cic = detect_cic_type(rom + 0x40);
    init_si(&g_si, &g_ri, &g_mi, cic);

    DebugMessage(M64MSG_VERBOSE, "Memory initialized");
    return 0;
}

void free_memory(void)
{
}



void map_region_r(uint16_t region, int (*readfn)(uint32_t, uint32_t*))
{
    readmem[region] = readfn;
}

void map_region_w(uint16_t region, int (writefn)(uint32_t, uint32_t, uint32_t))
{
    writemem[region] = writefn;
}

void map_region(uint16_t region,
        int (*readfn)(uint32_t, uint32_t*),
        int (*writefn)(uint32_t, uint32_t, uint32_t))
{
    map_region_r(region, readfn);
    map_region_w(region, writefn);
}



int read_word(uint32_t address, uint32_t* value)
{
    return readmem[address >> 16](address, value);
}

int write_word(uint32_t address, uint32_t value, uint32_t mask)
{
    return writemem[address >> 16](address, value, mask);
}



unsigned int *fast_mem_access(unsigned int address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */

    if ((address & 0xc0000000) != 0x80000000) /* not in [0x80000000-0xc0000000[ */
        address = virtual_to_physical_address(address, 2);

    address &= 0x1ffffffc;

    if (address < RDRAM_MAX_SIZE)
        return (unsigned int*)((uint8_t*)g_ri.ram + address);
    else if (address >= 0x10000000)
        return (unsigned int*)((uint8_t*)g_pi.cart_rom + address - 0x10000000);
    else if ((address & 0xffffe000) == 0x04000000) /* in [0x04000000-0x04002000[ */
        return (unsigned int*)((uint8_t*)g_sp.mem + (address & 0x1ffc));
    else
        return NULL;
}

