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

#include "api/m64p_types.h"

#include "memory.h"

#include "main/main.h"

#include "r4300/r4300.h"
#include "r4300/cached_interp.h"
#include "r4300/tlb.h"

#include "api/callbacks.h"
#include "main/rom.h"
#include "r4300/new_dynarec/new_dynarec.h"

#if NEW_DYNAREC != NEW_DYNAREC_ARM
// address : address of the read/write operation being done
unsigned int address = 0;
#endif

// values that are being written are stored in these variables
#if NEW_DYNAREC != NEW_DYNAREC_ARM
unsigned int word;
unsigned char cpu_byte;
unsigned short hword;
unsigned long long int dword;
#endif

// addresse where the read value will be stored
unsigned long long int* rdword;

// hash tables of read functions
void (*readmem[0x10000])(void);
void (*readmemb[0x10000])(void);
void (*readmemh[0x10000])(void);
void (*readmemd[0x10000])(void);

// hash tables of write functions
void (*writemem[0x10000])(void);
void (*writememb[0x10000])(void);
void (*writememd[0x10000])(void);
void (*writememh[0x10000])(void);



static inline unsigned int bshift(uint32_t address)
{
    return ((address & 3) ^ 3) << 3;
}

static inline unsigned int hshift(uint32_t address)
{
    return ((address & 2) ^ 2) << 3;
}


void map_region_r(uint16_t region,
        void (*readb)(void),
        void (*readh)(void),
        void (*readw)(void),
        void (*readd)(void))
{
    readmemb[region] = readb;
    readmemh[region] = readh;
    readmem [region] = readw;
    readmemd[region] = readd;
}

void map_region_w(uint16_t region,
        void (*writeb)(void),
        void (*writeh)(void),
        void (*writew)(void),
        void (*writed)(void))
{
    writememb[region] = writeb;
    writememh[region] = writeh;
    writemem [region] = writew;
    writememd[region] = writed;
}

void map_region(uint16_t region,
        void (*readb)(void),
        void (*readh)(void),
        void (*readw)(void),
        void (*readd)(void),
        void (*writeb)(void),
        void (*writeh)(void),
        void (*writew)(void),
        void (*writed)(void))
{
    map_region_r(region, readb, readh, readw, readd);
    map_region_w(region, writeb, writeh, writew, writed);
}


#define R(x) read_ ## x ## b, read_ ## x ## h, read_## x, read_## x ## d
#define W(x) write_ ## x ## b, write_ ## x ## h, write_ ## x, write_ ## x ## d
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
        map_region(0xb000+i, R(rom), write_nothingb, write_nothingh, write_rom, write_nothingd);
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

void read_nothing(void)
{
    if (address == 0xa5000508) *rdword = 0xFFFFFFFF;
    else *rdword = 0;
}

void read_nothingb(void)
{
    *rdword = 0;
}

void read_nothingh(void)
{
    *rdword = 0;
}

void read_nothingd(void)
{
    *rdword = 0;
}

void write_nothing(void)
{
}

void write_nothingb(void)
{
}

void write_nothingh(void)
{
}

void write_nothingd(void)
{
}

void read_nomem(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_word_in_memory();
}

void read_nomemb(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_byte_in_memory();
}

void read_nomemh(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_hword_in_memory();
}

void read_nomemd(void)
{
    address = virtual_to_physical_address(address,0);
    if (address == 0x00000000) return;
    read_dword_in_memory();
}

void write_nomem(void)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops !=
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_word_in_memory();
}

void write_nomemb(void)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops != 
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_byte_in_memory();
}

void write_nomemh(void)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops != 
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_hword_in_memory();
}

void write_nomemd(void)
{
    if (r4300emu != CORE_PURE_INTERPRETER && !invalid_code[address>>12])
        if (blocks[address>>12]->block[(address&0xFFF)/4].ops != 
            current_instruction_table.NOTCOMPILED)
            invalid_code[address>>12] = 1;
    address = virtual_to_physical_address(address,1);
    if (address == 0x00000000) return;
    write_dword_in_memory();
}

void read_rdram(void)
{
    uint32_t value;

    read_rdram_ram(&g_ri, address, &value);
    *rdword = value;
}

void read_rdramb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rdram_ram(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rdramh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rdram_ram(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rdramd(void)
{
    uint32_t w[2];

    read_rdram_ram(&g_ri, address    , &w[0]);
    read_rdram_ram(&g_ri, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void read_rdramFB(void)
{
    pre_framebuffer_read(&g_dp, address);
    read_rdram();
}

void read_rdramFBb(void)
{
    pre_framebuffer_read(&g_dp, address);
    read_rdramb();
}

void read_rdramFBh(void)
{
    pre_framebuffer_read(&g_dp, address);
    read_rdramh();
}

void read_rdramFBd(void)
{
    pre_framebuffer_read(&g_dp, address);
    read_rdramd();
}

void write_rdram(void)
{
    write_rdram_ram(&g_ri, address, word, ~0U);
}

void write_rdramb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rdram_ram(&g_ri, address, value, mask);
}

void write_rdramh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rdram_ram(&g_ri, address, value, mask);
}

void write_rdramd(void)
{
    write_rdram_ram(&g_ri, address    , dword >> 32, ~0U);
    write_rdram_ram(&g_ri, address + 4, dword      , ~0U);
}

void write_rdramFB(void)
{
    pre_framebuffer_write(&g_dp, address, 4);
    write_rdram();
}

void write_rdramFBb(void)
{
    pre_framebuffer_write(&g_dp, address^S8, 1);
    write_rdramb();
}

void write_rdramFBh(void)
{
    pre_framebuffer_write(&g_dp, address^S16, 2);
    write_rdramh();
}

void write_rdramFBd(void)
{
    pre_framebuffer_write(&g_dp, address, 8);
    write_rdramd();
}

void read_rdramreg(void)
{
    uint32_t value;

    read_rdram_regs(&g_ri, address, &value);
    *rdword = value;
}

void read_rdramregb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rdram_regs(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rdramregh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rdram_regs(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rdramregd(void)
{
    uint32_t w[2];

    read_rdram_regs(&g_ri, address    , &w[0]);
    read_rdram_regs(&g_ri, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rdramreg(void)
{
    write_rdram_regs(&g_ri, address, word, ~0U);
}

void write_rdramregb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rdram_regs(&g_ri, address, value, mask);
}

void write_rdramregh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rdram_regs(&g_ri, address, value, mask);
}

void write_rdramregd(void)
{
    write_rdram_regs(&g_ri, address    , dword >> 32, ~0U);
    write_rdram_regs(&g_ri, address + 4, dword      , ~0U);
}

void read_rsp_memory(void)
{
    uint32_t value;

    read_rsp_mem(&g_sp, address, &value);
    *rdword = value;
}

void read_rsp_memoryb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rsp_mem(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rsp_memoryh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rsp_mem(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rsp_memoryd(void)
{
    uint32_t w[2];

    read_rsp_mem(&g_sp, address    , &w[0]);
    read_rsp_mem(&g_sp, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rsp_memory(void)
{
    write_rsp_mem(&g_sp, address, word, ~0U);
}

void write_rsp_memoryb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rsp_mem(&g_sp, address, value, mask);
}

void write_rsp_memoryh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rsp_mem(&g_sp, address, value, mask);
}

void write_rsp_memoryd(void)
{
    write_rsp_mem(&g_sp, address    , dword >> 32, ~0U);
    write_rsp_mem(&g_sp, address + 4, dword      , ~0U);
}

void read_rsp_reg(void)
{
    uint32_t value;

    read_rsp_regs(&g_sp, address, &value);
    *rdword = value;
}

void read_rsp_regb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rsp_regs(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rsp_regh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rsp_regs(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rsp_regd(void)
{
    uint32_t w[2];

    read_rsp_regs(&g_sp, address    , &w[0]);
    read_rsp_regs(&g_sp, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rsp_reg(void)
{
    write_rsp_regs(&g_sp, address, word, ~0U);
}

void write_rsp_regb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rsp_regs(&g_sp, address, value, mask);
}

void write_rsp_regh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rsp_regs(&g_sp, address, value, mask);
}

void write_rsp_regd(void)
{
    write_rsp_regs(&g_sp, address    , dword >> 32, ~0U);
    write_rsp_regs(&g_sp, address + 4, dword      , ~0U);
}

void read_rsp(void)
{
    uint32_t value;

    read_rsp_regs2(&g_sp, address, &value);
    *rdword = value;
}

void read_rspb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rsp_regs2(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rsph(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rsp_regs2(&g_sp, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rspd(void)
{
    uint32_t w[2];

    read_rsp_regs2(&g_sp, address    , &w[0]);
    read_rsp_regs2(&g_sp, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rsp(void)
{
    write_rsp_regs2(&g_sp, address, word, ~0U);
}

void write_rspb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rsp_regs2(&g_sp, address, value, mask);
}

void write_rsph(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rsp_regs2(&g_sp, address, value, mask);
}

void write_rspd(void)
{
    write_rsp_regs2(&g_sp, address    , dword >> 32, ~0U);
    write_rsp_regs2(&g_sp, address + 4, dword      , ~0U);
}

void read_dp(void)
{
    uint32_t value;

    read_dpc_regs(&g_dp, address, &value);
    *rdword = value;
}

void read_dpb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_dpc_regs(&g_dp, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_dph(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_dpc_regs(&g_dp, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_dpd(void)
{
    uint32_t w[2];

    read_dpc_regs(&g_dp, address    , &w[0]);
    read_dpc_regs(&g_dp, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_dp(void)
{
    write_dpc_regs(&g_dp, address, word, ~0U);
}

void write_dpb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_dpc_regs(&g_dp, address, value, mask);
}

void write_dph(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_dpc_regs(&g_dp, address, value, mask);
}

void write_dpd(void)
{
    write_dpc_regs(&g_dp, address    , dword >> 32, ~0U);
    write_dpc_regs(&g_dp, address + 4, dword      , ~0U);
}

void read_dps(void)
{
    uint32_t value;

    read_dps_regs(&g_dp, address, &value);
    *rdword = value;
}

void read_dpsb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_dps_regs(&g_dp, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_dpsh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_dps_regs(&g_dp, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_dpsd(void)
{
    uint32_t w[2];

    read_dps_regs(&g_dp, address    , &w[0]);
    read_dps_regs(&g_dp, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_dps(void)
{
    write_dps_regs(&g_dp, address, word, ~0U);
}

void write_dpsb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_dps_regs(&g_dp, address, value, mask);
}

void write_dpsh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_dps_regs(&g_dp, address, value, mask);
}

void write_dpsd(void)
{
    write_dps_regs(&g_dp, address    , dword >> 32, ~0U);
    write_dps_regs(&g_dp, address + 4, dword      , ~0U);
}

void read_mi(void)
{
    uint32_t value;

    read_mi_regs(&g_mi, address, &value);
    *rdword = value;
}

void read_mib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_mi_regs(&g_mi, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_mih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_mi_regs(&g_mi, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_mid(void)
{
    uint32_t w[2];

    read_mi_regs(&g_mi, address    , &w[0]);
    read_mi_regs(&g_mi, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_mi(void)
{
    write_mi_regs(&g_mi, address, word, ~0U);
}

void write_mib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_mi_regs(&g_mi, address, value, mask);
}

void write_mih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_mi_regs(&g_mi, address, value, mask);
}

void write_mid(void)
{
    write_mi_regs(&g_mi, address    , dword >> 32, ~0U);
    write_mi_regs(&g_mi, address + 4, dword      , ~0U);
}

void read_vi(void)
{
    uint32_t value;

    read_vi_regs(&g_vi, address, &value);
    *rdword = value;
}

void read_vib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_vi_regs(&g_vi, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_vih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_vi_regs(&g_vi, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_vid(void)
{
    uint32_t w[2];

    read_vi_regs(&g_vi, address    , &w[0]);
    read_vi_regs(&g_vi, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_vi(void)
{
    write_vi_regs(&g_vi, address, word, ~0U);
}

void write_vib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_vi_regs(&g_vi, address, value, mask);
}

void write_vih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_vi_regs(&g_vi, address, value, mask);
}

void write_vid(void)
{
    write_vi_regs(&g_vi, address    , dword >> 32, ~0U);
    write_vi_regs(&g_vi, address + 4, dword      , ~0U);
}

void read_ai(void)
{
    uint32_t value;

    read_ai_regs(&g_ai, address, &value);
    *rdword = value;
}

void read_aib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_ai_regs(&g_ai, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_aih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_ai_regs(&g_ai, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_aid(void)
{
    uint32_t w[2];

    read_ai_regs(&g_ai, address    , &w[0]);
    read_ai_regs(&g_ai, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_ai(void)
{
    write_ai_regs(&g_ai, address, word, ~0U);
}

void write_aib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_ai_regs(&g_ai, address, value, mask);
}

void write_aih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_ai_regs(&g_ai, address, value, mask);
}

void write_aid(void)
{
    write_ai_regs(&g_ai, address    , dword >> 32, ~0U);
    write_ai_regs(&g_ai, address + 4, dword      , ~0U);
}

void read_pi(void)
{
    uint32_t value;

    read_pi_regs(&g_pi, address, &value);
    *rdword = value;
}

void read_pib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_pi_regs(&g_pi, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_pih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_pi_regs(&g_pi, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_pid(void)
{
    uint32_t w[2];

    read_pi_regs(&g_pi, address    , &w[0]);
    read_pi_regs(&g_pi, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_pi(void)
{
    write_pi_regs(&g_pi, address, word, ~0U);
}

void write_pib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_pi_regs(&g_pi, address, value, mask);
}

void write_pih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_pi_regs(&g_pi, address, value, mask);
}

void write_pid(void)
{
    write_pi_regs(&g_pi, address    , dword >> 32, ~0U);
    write_pi_regs(&g_pi, address + 4, dword      , ~0U);
}

void read_ri(void)
{
    uint32_t value;

    read_ri_regs(&g_ri, address, &value);
    *rdword = value;
}

void read_rib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_ri_regs(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_ri_regs(&g_ri, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rid(void)
{
    uint32_t w[2];

    read_ri_regs(&g_ri, address    , &w[0]);
    read_ri_regs(&g_ri, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_ri(void)
{
    write_ri_regs(&g_ri, address, word, ~0U);
}

void write_rib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_ri_regs(&g_ri, address, value, mask);
}

void write_rih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_ri_regs(&g_ri, address, value, mask);
}

void write_rid(void)
{
    write_ri_regs(&g_ri, address    , dword >> 32, ~0U);
    write_ri_regs(&g_ri, address + 4, dword      , ~0U);
}

void read_si(void)
{
    uint32_t value;

    read_si_regs(&g_si, address, &value);
    *rdword = value;
}

void read_sib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_si_regs(&g_si, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_sih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_si_regs(&g_si, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_sid(void)
{
    uint32_t w[2];

    read_si_regs(&g_si, address    , &w[0]);
    read_si_regs(&g_si, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_si(void)
{
    write_si_regs(&g_si, address, word, ~0U);
}

void write_sib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_si_regs(&g_si, address, value, mask);
}

void write_sih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_si_regs(&g_si, address, value, mask);
}

void write_sid(void)
{
    write_si_regs(&g_si, address    , dword >> 32, ~0U);
    write_si_regs(&g_si, address + 4, dword      , ~0U);
}

void read_flashram_status(void)
{
    uint32_t value;

    if (pi_read_flashram_status(&g_pi, address, &value))
        *rdword = value;
}

void read_flashram_statusb(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusb() not implemented");
}

void read_flashram_statush(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statush() not implemented");
}

void read_flashram_statusd(void)
{
    DebugMessage(M64MSG_ERROR, "read_flashram_statusd() not implemented");
}

void write_flashram_dummy(void)
{
}

void write_flashram_dummyb(void)
{
}

void write_flashram_dummyh(void)
{
}

void write_flashram_dummyd(void)
{
}

void write_flashram_command(void)
{
    pi_write_flashram_command(&g_pi, address, word, ~0U);
}

void write_flashram_commandb(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandb() not implemented");
}

void write_flashram_commandh(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandh() not implemented");
}

void write_flashram_commandd(void)
{
    DebugMessage(M64MSG_ERROR, "write_flashram_commandd() not implemented");
}

void read_rom(void)
{
    uint32_t value;

    read_cart_rom(&g_pi, address, &value);
    *rdword = value;
}

void read_romb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_cart_rom(&g_pi, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_romh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_cart_rom(&g_pi, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_romd(void)
{
    uint32_t w[2];

    read_cart_rom(&g_pi, address    , &w[0]);
    read_cart_rom(&g_pi, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rom(void)
{
    write_cart_rom(&g_pi, address, word, ~0U);
}

void read_pif(void)
{
    uint32_t value;

    read_pif_ram(&g_si, address, &value);
    *rdword = value;
}

void read_pifb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_pif_ram(&g_si, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_pifh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_pif_ram(&g_si, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_pifd(void)
{
    uint32_t w[2];

    read_pif_ram(&g_si, address    , &w[0]);
    read_pif_ram(&g_si, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_pif(void)
{
    write_pif_ram(&g_si, address, word, ~0U);
}

void write_pifb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_pif_ram(&g_si, address, value, mask);
}

void write_pifh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_pif_ram(&g_si, address, value, mask);
}

void write_pifd(void)
{
    write_pif_ram(&g_si, address    , dword >> 32, ~0U);
    write_pif_ram(&g_si, address + 4, dword      , ~0U);
}

unsigned int *fast_mem_access(unsigned int address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */
    if (address < 0x80000000 || address >= 0xc0000000)
        address = virtual_to_physical_address(address, 2);

    if ((address & 0x1FFFFFFF) >= 0x10000000)
        return (unsigned int *)g_pi.cart_rom + ((address & 0x1FFFFFFF) - 0x10000000)/4;
    else if ((address & 0x1FFFFFFF) < RDRAM_MAX_SIZE)
        return (unsigned int *)g_ri.ram + (address & 0x1FFFFFFF)/4;
    else if (address >= 0xa4000000 && address <= 0xa4002000)
        return &g_sp.mem[(address & 0x1fff) / 4];
    else
        return NULL;
}
