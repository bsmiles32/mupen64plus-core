/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
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

#ifndef MEMORY_H
#define MEMORY_H

#include "osal/preproc.h"
#include "ai/controller.h"
#include "pi/controller.h"
#include "r4300/mi.h"
#include "rdp/core.h"
#include "rsp/core.h"
#include "rdram/controller.h"
#include "si/controller.h"
#include "vi/controller.h"

int init_memory(int DoByteSwap);
void free_memory(void);
#define read_word_in_memory() readmem[address>>16]()
#define read_byte_in_memory() readmemb[address>>16]()
#define read_hword_in_memory() readmemh[address>>16]()
#define read_dword_in_memory() readmemd[address>>16]()
#define write_word_in_memory() writemem[address>>16]()
#define write_byte_in_memory() writememb[address >>16]()
#define write_hword_in_memory() writememh[address >>16]()
#define write_dword_in_memory() writememd[address >>16]()

extern ALIGN(16, struct rdram_controller g_rdram);
extern struct ai_controller g_ai;
extern struct mi_controller g_mi;
extern struct pi_controller g_pi;
extern struct si_controller g_si;
extern struct vi_controller g_vi;
extern struct rdp_core g_dp;
extern struct rsp_core g_sp;

extern unsigned int address, word;
extern unsigned char cpu_byte;
extern unsigned short hword;
extern unsigned long long dword, *rdword;

extern void (*readmem[0x10000])(void);
extern void (*readmemb[0x10000])(void);
extern void (*readmemh[0x10000])(void);
extern void (*readmemd[0x10000])(void);
extern void (*writemem[0x10000])(void);
extern void (*writememb[0x10000])(void);
extern void (*writememh[0x10000])(void);
extern void (*writememd[0x10000])(void);

extern unsigned int CIC_Chip;

#ifndef M64P_BIG_ENDIAN
#if defined(__GNUC__) && (__GNUC__ > 4  || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#define sl(x) __builtin_bswap32(x)
#else
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)
#endif
#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

void read_nothing(void);
void read_nothingh(void);
void read_nothingb(void);
void read_nothingd(void);
void read_nomem(void);
void read_nomemb(void);
void read_nomemh(void);
void read_nomemd(void);
void read_rdram(void);
void read_rdramb(void);
void read_rdramh(void);
void read_rdramd(void);
void read_rdramFB(void);
void read_rdramFBb(void);
void read_rdramFBh(void);
void read_rdramFBd(void);
void read_rdramreg(void);
void read_rdramregb(void);
void read_rdramregh(void);
void read_rdramregd(void);
void read_rsp_memory(void);
void read_rsp_memoryb(void);
void read_rsp_memoryh(void);
void read_rsp_memoryd(void);
void read_rsp_reg(void);
void read_rsp_regb(void);
void read_rsp_regh(void);
void read_rsp_regd(void);
void read_rsp(void);
void read_rspb(void);
void read_rsph(void);
void read_rspd(void);
void read_dp(void);
void read_dpb(void);
void read_dph(void);
void read_dpd(void);
void read_dps(void);
void read_dpsb(void);
void read_dpsh(void);
void read_dpsd(void);
void read_mi(void);
void read_mib(void);
void read_mih(void);
void read_mid(void);
void read_vi(void);
void read_vib(void);
void read_vih(void);
void read_vid(void);
void read_ai(void);
void read_aib(void);
void read_aih(void);
void read_aid(void);
void read_pi(void);
void read_pib(void);
void read_pih(void);
void read_pid(void);
void read_ri(void);
void read_rib(void);
void read_rih(void);
void read_rid(void);
void read_si(void);
void read_sib(void);
void read_sih(void);
void read_sid(void);
void read_flashram_status(void);
void read_flashram_statusb(void);
void read_flashram_statush(void);
void read_flashram_statusd(void);
void read_rom(void);
void read_romb(void);
void read_romh(void);
void read_romd(void);
void read_pif(void);
void read_pifb(void);
void read_pifh(void);
void read_pifd(void);

void write_nothing(void);
void write_nothingb(void);
void write_nothingh(void);
void write_nothingd(void);
void write_nomem(void);
void write_nomemb(void);
void write_nomemd(void);
void write_nomemh(void);
void write_rdram(void);
void write_rdramb(void);
void write_rdramh(void);
void write_rdramd(void);
void write_rdramFB(void);
void write_rdramFBb(void);
void write_rdramFBh(void);
void write_rdramFBd(void);
void write_rdramreg(void);
void write_rdramregb(void);
void write_rdramregh(void);
void write_rdramregd(void);
void write_rsp_memory(void);
void write_rsp_memoryb(void);
void write_rsp_memoryh(void);
void write_rsp_memoryd(void);
void write_rsp_reg(void);
void write_rsp_regb(void);
void write_rsp_regh(void);
void write_rsp_regd(void);
void write_rsp(void);
void write_rspb(void);
void write_rsph(void);
void write_rspd(void);
void write_dp(void);
void write_dpb(void);
void write_dph(void);
void write_dpd(void);
void write_dps(void);
void write_dpsb(void);
void write_dpsh(void);
void write_dpsd(void);
void write_mi(void);
void write_mib(void);
void write_mih(void);
void write_mid(void);
void write_vi(void);
void write_vib(void);
void write_vih(void);
void write_vid(void);
void write_ai(void);
void write_aib(void);
void write_aih(void);
void write_aid(void);
void write_pi(void);
void write_pib(void);
void write_pih(void);
void write_pid(void);
void write_ri(void);
void write_rib(void);
void write_rih(void);
void write_rid(void);
void write_si(void);
void write_sib(void);
void write_sih(void);
void write_sid(void);
void write_flashram_dummy(void);
void write_flashram_dummyb(void);
void write_flashram_dummyh(void);
void write_flashram_dummyd(void);
void write_flashram_command(void);
void write_flashram_commandb(void);
void write_flashram_commandh(void);
void write_flashram_commandd(void);
void write_rom(void);
void write_pif(void);
void write_pifb(void);
void write_pifh(void);
void write_pifd(void);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
unsigned int *fast_mem_access(unsigned int address);


static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (mask & value) | (~mask & *dst);
}

#endif

