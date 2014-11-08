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

#include <stdlib.h>

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#if !defined(WIN32)
#include <sys/mman.h>
#endif

#include "api/m64p_types.h"

#include "memory.h"
#include "dma.h"
#include "pif.h"
#include "flashram.h"

#include "ai/controller.h"
#include "pi/controller.h"

#include "r4300/r4300.h"
#include "r4300/cached_interp.h"
#include "r4300/cp0.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"
#include "r4300/recomph.h"
#include "r4300/ops.h"
#include "r4300/tlb.h"

#include "rdram/controller.h"
#include "rsp/core.h"
#include "vi/controller.h"

#include "api/callbacks.h"
#include "main/main.h"
#include "main/profile.h"
#include "main/rom.h"
#include "osal/preproc.h"
#include "plugin/plugin.h"
#include "r4300/new_dynarec/new_dynarec.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_breakpoints.h"
#endif

/* definitions of the rcp's structures and memory area */
SI_register si_register;
DPC_register dpc_register;
DPS_register dps_register;

unsigned int CIC_Chip;

ALIGN(16, struct rdram_controller g_rdram);
struct ai_controller g_ai;
struct mi_controller g_mi;
struct pi_controller g_pi;
struct vi_controller g_vi;
struct rsp_core g_sp;

unsigned int PIF_RAM[0x40/4];
unsigned char *PIF_RAMb = (unsigned char *)(PIF_RAM);

#if NEW_DYNAREC != NEW_DYNAREC_ARM
// address : address of the read/write operation being done
unsigned int address = 0;
#endif
// *address_low = the lower 16 bit of the address :
#ifdef M64P_BIG_ENDIAN
static unsigned short *address_low = (unsigned short *)(&address)+1;
#else
static unsigned short *address_low = (unsigned short *)(&address);
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

// trash : when we write to unmaped memory it is written here
static unsigned int trash;

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

// memory sections
unsigned int *readsi[0x10000];
unsigned int *readdp[0x10000];
unsigned int *readdps[0x10000];

// the frameBufferInfos
static FrameBufferInfo frameBufferInfos[6];
static char framebufferRead[0x800];
static int firstFrameBufferSetting;

// uncomment to output count of calls to write_rdram():
//#define COUNT_WRITE_RDRAM_CALLS 1

#if defined( COUNT_WRITE_RDRAM_CALLS )
	int writerdram_count = 1;
#endif



static inline unsigned int bshift(uint32_t address)
{
    return ((address & 3) ^ 3) << 3;
}

static inline unsigned int hshift(uint32_t address)
{
    return ((address & 2) ^ 2) << 3;
}


int init_memory(int DoByteSwap)
{
    int i;
    long long CRC = 0;

    if (DoByteSwap != 0)
    {
        //swap rom
        unsigned int *roml = (unsigned int *) rom;
        for (i=0; i<(rom_size/4); i++) roml[i] = sl(roml[i]);
    }

    //init hash tables
    for (i=0; i<(0x10000); i++)
    {
        readmem[i] = read_nomem;
        readmemb[i] = read_nomemb;
        readmemd[i] = read_nomemd;
        readmemh[i] = read_nomemh;
        writemem[i] = write_nomem;
        writememb[i] = write_nomemb;
        writememd[i] = write_nomemd;
        writememh[i] = write_nomemh;
    }

    init_rdram(&g_rdram);

    /* map RDRAM RAM */
    for (i=0; i</*0x40*/0x80; i++)
    {
        readmem[(0x8000+i)] = read_rdram;
        readmem[(0xa000+i)] = read_rdram;
        readmemb[(0x8000+i)] = read_rdramb;
        readmemb[(0xa000+i)] = read_rdramb;
        readmemh[(0x8000+i)] = read_rdramh;
        readmemh[(0xa000+i)] = read_rdramh;
        readmemd[(0x8000+i)] = read_rdramd;
        readmemd[(0xa000+i)] = read_rdramd;
        writemem[(0x8000+i)] = write_rdram;
        writemem[(0xa000+i)] = write_rdram;
        writememb[(0x8000+i)] = write_rdramb;
        writememb[(0xa000+i)] = write_rdramb;
        writememh[(0x8000+i)] = write_rdramh;
        writememh[(0xa000+i)] = write_rdramh;
        writememd[(0x8000+i)] = write_rdramd;
        writememd[(0xa000+i)] = write_rdramd;
    }
    for (i=/*0x40*/0x80; i<0x3F0; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    /* map RDRAM registers */
    readmem[0x83f0] = read_rdramreg;
    readmem[0xa3f0] = read_rdramreg;
    readmemb[0x83f0] = read_rdramregb;
    readmemb[0xa3f0] = read_rdramregb;
    readmemh[0x83f0] = read_rdramregh;
    readmemh[0xa3f0] = read_rdramregh;
    readmemd[0x83f0] = read_rdramregd;
    readmemd[0xa3f0] = read_rdramregd;
    writemem[0x83f0] = write_rdramreg;
    writemem[0xa3f0] = write_rdramreg;
    writememb[0x83f0] = write_rdramregb;
    writememb[0xa3f0] = write_rdramregb;
    writememh[0x83f0] = write_rdramregh;
    writememh[0xa3f0] = write_rdramregh;
    writememd[0x83f0] = write_rdramregd;
    writememd[0xa3f0] = write_rdramregd;
    for (i=1; i<0x10; i++)
    {
        readmem[0x83f0+i] = read_nothing;
        readmem[0xa3f0+i] = read_nothing;
        readmemb[0x83f0+i] = read_nothingb;
        readmemb[0xa3f0+i] = read_nothingb;
        readmemh[0x83f0+i] = read_nothingh;
        readmemh[0xa3f0+i] = read_nothingh;
        readmemd[0x83f0+i] = read_nothingd;
        readmemd[0xa3f0+i] = read_nothingd;
        writemem[0x83f0+i] = write_nothing;
        writemem[0xa3f0+i] = write_nothing;
        writememb[0x83f0+i] = write_nothingb;
        writememb[0xa3f0+i] = write_nothingb;
        writememh[0x83f0+i] = write_nothingh;
        writememh[0xa3f0+i] = write_nothingh;
        writememd[0x83f0+i] = write_nothingd;
        writememd[0xa3f0+i] = write_nothingd;
    }

    init_rsp(&g_sp);

    /* map SP memory */
    readmem[0x8400] = read_rsp_memory;
    readmem[0xa400] = read_rsp_memory;
    readmemb[0x8400] = read_rsp_memoryb;
    readmemb[0xa400] = read_rsp_memoryb;
    readmemh[0x8400] = read_rsp_memoryh;
    readmemh[0xa400] = read_rsp_memoryh;
    readmemd[0x8400] = read_rsp_memoryd;
    readmemd[0xa400] = read_rsp_memoryd;
    writemem[0x8400] = write_rsp_memory;
    writemem[0xa400] = write_rsp_memory;
    writememb[0x8400] = write_rsp_memoryb;
    writememb[0xa400] = write_rsp_memoryb;
    writememh[0x8400] = write_rsp_memoryh;
    writememh[0xa400] = write_rsp_memoryh;
    writememd[0x8400] = write_rsp_memoryd;
    writememd[0xa400] = write_rsp_memoryd;
    for (i=1; i<0x4; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    /* map SP registers */
    readmem[0x8404] = read_rsp_reg;
    readmem[0xa404] = read_rsp_reg;
    readmemb[0x8404] = read_rsp_regb;
    readmemb[0xa404] = read_rsp_regb;
    readmemh[0x8404] = read_rsp_regh;
    readmemh[0xa404] = read_rsp_regh;
    readmemd[0x8404] = read_rsp_regd;
    readmemd[0xa404] = read_rsp_regd;
    writemem[0x8404] = write_rsp_reg;
    writemem[0xa404] = write_rsp_reg;
    writememb[0x8404] = write_rsp_regb;
    writememb[0xa404] = write_rsp_regb;
    writememh[0x8404] = write_rsp_regh;
    writememh[0xa404] = write_rsp_regh;
    writememd[0x8404] = write_rsp_regd;
    writememd[0xa404] = write_rsp_regd;
    for (i=5; i<8; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    readmem[0x8408] = read_rsp;
    readmem[0xa408] = read_rsp;
    readmemb[0x8408] = read_rspb;
    readmemb[0xa408] = read_rspb;
    readmemh[0x8408] = read_rsph;
    readmemh[0xa408] = read_rsph;
    readmemd[0x8408] = read_rspd;
    readmemd[0xa408] = read_rspd;
    writemem[0x8408] = write_rsp;
    writemem[0xa408] = write_rsp;
    writememb[0x8408] = write_rspb;
    writememb[0xa408] = write_rspb;
    writememh[0x8408] = write_rsph;
    writememh[0xa408] = write_rsph;
    writememd[0x8408] = write_rspd;
    writememd[0xa408] = write_rspd;
    for (i=9; i<0x10; i++)
    {
        readmem[0x8400+i] = read_nothing;
        readmem[0xa400+i] = read_nothing;
        readmemb[0x8400+i] = read_nothingb;
        readmemb[0xa400+i] = read_nothingb;
        readmemh[0x8400+i] = read_nothingh;
        readmemh[0xa400+i] = read_nothingh;
        readmemd[0x8400+i] = read_nothingd;
        readmemd[0xa400+i] = read_nothingd;
        writemem[0x8400+i] = write_nothing;
        writemem[0xa400+i] = write_nothing;
        writememb[0x8400+i] = write_nothingb;
        writememb[0xa400+i] = write_nothingb;
        writememh[0x8400+i] = write_nothingh;
        writememh[0xa400+i] = write_nothingh;
        writememd[0x8400+i] = write_nothingd;
        writememd[0xa400+i] = write_nothingd;
    }

    //init rdp command registers
    readmem[0x8410] = read_dp;
    readmem[0xa410] = read_dp;
    readmemb[0x8410] = read_dpb;
    readmemb[0xa410] = read_dpb;
    readmemh[0x8410] = read_dph;
    readmemh[0xa410] = read_dph;
    readmemd[0x8410] = read_dpd;
    readmemd[0xa410] = read_dpd;
    writemem[0x8410] = write_dp;
    writemem[0xa410] = write_dp;
    writememb[0x8410] = write_dpb;
    writememb[0xa410] = write_dpb;
    writememh[0x8410] = write_dph;
    writememh[0xa410] = write_dph;
    writememd[0x8410] = write_dpd;
    writememd[0xa410] = write_dpd;
    dpc_register.dpc_start=0;
    dpc_register.dpc_end=0;
    dpc_register.dpc_current=0;
    dpc_register.w_dpc_status=0;
    dpc_register.dpc_status=0;
    dpc_register.dpc_clock=0;
    dpc_register.dpc_bufbusy=0;
    dpc_register.dpc_pipebusy=0;
    dpc_register.dpc_tmem=0;
    readdp[0x0] = &dpc_register.dpc_start;
    readdp[0x4] = &dpc_register.dpc_end;
    readdp[0x8] = &dpc_register.dpc_current;
    readdp[0xc] = &dpc_register.dpc_status;
    readdp[0x10] = &dpc_register.dpc_clock;
    readdp[0x14] = &dpc_register.dpc_bufbusy;
    readdp[0x18] = &dpc_register.dpc_pipebusy;
    readdp[0x1c] = &dpc_register.dpc_tmem;

    for (i=0x20; i<0x10000; i++) readdp[i] = &trash;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8410+i] = read_nothing;
        readmem[0xa410+i] = read_nothing;
        readmemb[0x8410+i] = read_nothingb;
        readmemb[0xa410+i] = read_nothingb;
        readmemh[0x8410+i] = read_nothingh;
        readmemh[0xa410+i] = read_nothingh;
        readmemd[0x8410+i] = read_nothingd;
        readmemd[0xa410+i] = read_nothingd;
        writemem[0x8410+i] = write_nothing;
        writemem[0xa410+i] = write_nothing;
        writememb[0x8410+i] = write_nothingb;
        writememb[0xa410+i] = write_nothingb;
        writememh[0x8410+i] = write_nothingh;
        writememh[0xa410+i] = write_nothingh;
        writememd[0x8410+i] = write_nothingd;
        writememd[0xa410+i] = write_nothingd;
    }

    //init rsp span registers
    readmem[0x8420] = read_dps;
    readmem[0xa420] = read_dps;
    readmemb[0x8420] = read_dpsb;
    readmemb[0xa420] = read_dpsb;
    readmemh[0x8420] = read_dpsh;
    readmemh[0xa420] = read_dpsh;
    readmemd[0x8420] = read_dpsd;
    readmemd[0xa420] = read_dpsd;
    writemem[0x8420] = write_dps;
    writemem[0xa420] = write_dps;
    writememb[0x8420] = write_dpsb;
    writememb[0xa420] = write_dpsb;
    writememh[0x8420] = write_dpsh;
    writememh[0xa420] = write_dpsh;
    writememd[0x8420] = write_dpsd;
    writememd[0xa420] = write_dpsd;
    dps_register.dps_tbist=0;
    dps_register.dps_test_mode=0;
    dps_register.dps_buftest_addr=0;
    dps_register.dps_buftest_data=0;
    readdps[0x0] = &dps_register.dps_tbist;
    readdps[0x4] = &dps_register.dps_test_mode;
    readdps[0x8] = &dps_register.dps_buftest_addr;
    readdps[0xc] = &dps_register.dps_buftest_data;

    for (i=0x10; i<0x10000; i++) readdps[i] = &trash;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8420+i] = read_nothing;
        readmem[0xa420+i] = read_nothing;
        readmemb[0x8420+i] = read_nothingb;
        readmemb[0xa420+i] = read_nothingb;
        readmemh[0x8420+i] = read_nothingh;
        readmemh[0xa420+i] = read_nothingh;
        readmemd[0x8420+i] = read_nothingd;
        readmemd[0xa420+i] = read_nothingd;
        writemem[0x8420+i] = write_nothing;
        writemem[0xa420+i] = write_nothing;
        writememb[0x8420+i] = write_nothingb;
        writememb[0xa420+i] = write_nothingb;
        writememh[0x8420+i] = write_nothingh;
        writememh[0xa420+i] = write_nothingh;
        writememd[0x8420+i] = write_nothingd;
        writememd[0xa420+i] = write_nothingd;
    }

    init_mi(&g_mi);

    /* map MIPS Interface registers */
    readmem[0xa830] = read_mi;
    readmem[0xa430] = read_mi;
    readmemb[0xa830] = read_mib;
    readmemb[0xa430] = read_mib;
    readmemh[0xa830] = read_mih;
    readmemh[0xa430] = read_mih;
    readmemd[0xa830] = read_mid;
    readmemd[0xa430] = read_mid;
    writemem[0x8430] = write_mi;
    writemem[0xa430] = write_mi;
    writememb[0x8430] = write_mib;
    writememb[0xa430] = write_mib;
    writememh[0x8430] = write_mih;
    writememh[0xa430] = write_mih;
    writememd[0x8430] = write_mid;
    writememd[0xa430] = write_mid;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8430+i] = read_nothing;
        readmem[0xa430+i] = read_nothing;
        readmemb[0x8430+i] = read_nothingb;
        readmemb[0xa430+i] = read_nothingb;
        readmemh[0x8430+i] = read_nothingh;
        readmemh[0xa430+i] = read_nothingh;
        readmemd[0x8430+i] = read_nothingd;
        readmemd[0xa430+i] = read_nothingd;
        writemem[0x8430+i] = write_nothing;
        writemem[0xa430+i] = write_nothing;
        writememb[0x8430+i] = write_nothingb;
        writememb[0xa430+i] = write_nothingb;
        writememh[0x8430+i] = write_nothingh;
        writememh[0xa430+i] = write_nothingh;
        writememd[0x8430+i] = write_nothingd;
        writememd[0xa430+i] = write_nothingd;
    }

    init_vi(&g_vi);

    /* map VI registers */
    readmem[0x8440] = read_vi;
    readmem[0xa440] = read_vi;
    readmemb[0x8440] = read_vib;
    readmemb[0xa440] = read_vib;
    readmemh[0x8440] = read_vih;
    readmemh[0xa440] = read_vih;
    readmemd[0x8440] = read_vid;
    readmemd[0xa440] = read_vid;
    writemem[0x8440] = write_vi;
    writemem[0xa440] = write_vi;
    writememb[0x8440] = write_vib;
    writememb[0xa440] = write_vib;
    writememh[0x8440] = write_vih;
    writememh[0xa440] = write_vih;
    writememd[0x8440] = write_vid;
    writememd[0xa440] = write_vid;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8440+i] = read_nothing;
        readmem[0xa440+i] = read_nothing;
        readmemb[0x8440+i] = read_nothingb;
        readmemb[0xa440+i] = read_nothingb;
        readmemh[0x8440+i] = read_nothingh;
        readmemh[0xa440+i] = read_nothingh;
        readmemd[0x8440+i] = read_nothingd;
        readmemd[0xa440+i] = read_nothingd;
        writemem[0x8440+i] = write_nothing;
        writemem[0xa440+i] = write_nothing;
        writememb[0x8440+i] = write_nothingb;
        writememb[0xa440+i] = write_nothingb;
        writememh[0x8440+i] = write_nothingh;
        writememh[0xa440+i] = write_nothingh;
        writememd[0x8440+i] = write_nothingd;
        writememd[0xa440+i] = write_nothingd;
    }

    init_ai(&g_ai);

    /* map AI registers */
    readmem[0x8450] = read_ai;
    readmem[0xa450] = read_ai;
    readmemb[0x8450] = read_aib;
    readmemb[0xa450] = read_aib;
    readmemh[0x8450] = read_aih;
    readmemh[0xa450] = read_aih;
    readmemd[0x8450] = read_aid;
    readmemd[0xa450] = read_aid;
    writemem[0x8450] = write_ai;
    writemem[0xa450] = write_ai;
    writememb[0x8450] = write_aib;
    writememb[0xa450] = write_aib;
    writememh[0x8450] = write_aih;
    writememh[0xa450] = write_aih;
    writememd[0x8450] = write_aid;
    writememd[0xa450] = write_aid;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8450+i] = read_nothing;
        readmem[0xa450+i] = read_nothing;
        readmemb[0x8450+i] = read_nothingb;
        readmemb[0xa450+i] = read_nothingb;
        readmemh[0x8450+i] = read_nothingh;
        readmemh[0xa450+i] = read_nothingh;
        readmemd[0x8450+i] = read_nothingd;
        readmemd[0xa450+i] = read_nothingd;
        writemem[0x8450+i] = write_nothing;
        writemem[0xa450+i] = write_nothing;
        writememb[0x8450+i] = write_nothingb;
        writememb[0xa450+i] = write_nothingb;
        writememh[0x8450+i] = write_nothingh;
        writememh[0xa450+i] = write_nothingh;
        writememd[0x8450+i] = write_nothingd;
        writememd[0xa450+i] = write_nothingd;
    }

    init_pi(&g_pi);

    /* map PI registers */
    readmem[0x8460] = read_pi;
    readmem[0xa460] = read_pi;
    readmemb[0x8460] = read_pib;
    readmemb[0xa460] = read_pib;
    readmemh[0x8460] = read_pih;
    readmemh[0xa460] = read_pih;
    readmemd[0x8460] = read_pid;
    readmemd[0xa460] = read_pid;
    writemem[0x8460] = write_pi;
    writemem[0xa460] = write_pi;
    writememb[0x8460] = write_pib;
    writememb[0xa460] = write_pib;
    writememh[0x8460] = write_pih;
    writememh[0xa460] = write_pih;
    writememd[0x8460] = write_pid;
    writememd[0xa460] = write_pid;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8460+i] = read_nothing;
        readmem[0xa460+i] = read_nothing;
        readmemb[0x8460+i] = read_nothingb;
        readmemb[0xa460+i] = read_nothingb;
        readmemh[0x8460+i] = read_nothingh;
        readmemh[0xa460+i] = read_nothingh;
        readmemd[0x8460+i] = read_nothingd;
        readmemd[0xa460+i] = read_nothingd;
        writemem[0x8460+i] = write_nothing;
        writemem[0xa460+i] = write_nothing;
        writememb[0x8460+i] = write_nothingb;
        writememb[0xa460+i] = write_nothingb;
        writememh[0x8460+i] = write_nothingh;
        writememh[0xa460+i] = write_nothingh;
        writememd[0x8460+i] = write_nothingd;
        writememd[0xa460+i] = write_nothingd;
    }

    /* map RI registers */
    readmem[0x8470] = read_ri;
    readmem[0xa470] = read_ri;
    readmemb[0x8470] = read_rib;
    readmemb[0xa470] = read_rib;
    readmemh[0x8470] = read_rih;
    readmemh[0xa470] = read_rih;
    readmemd[0x8470] = read_rid;
    readmemd[0xa470] = read_rid;
    writemem[0x8470] = write_ri;
    writemem[0xa470] = write_ri;
    writememb[0x8470] = write_rib;
    writememb[0xa470] = write_rib;
    writememh[0x8470] = write_rih;
    writememh[0xa470] = write_rih;
    writememd[0x8470] = write_rid;
    writememd[0xa470] = write_rid;
    for (i=1; i<0x10; i++)
    {
        readmem[0x8470+i] = read_nothing;
        readmem[0xa470+i] = read_nothing;
        readmemb[0x8470+i] = read_nothingb;
        readmemb[0xa470+i] = read_nothingb;
        readmemh[0x8470+i] = read_nothingh;
        readmemh[0xa470+i] = read_nothingh;
        readmemd[0x8470+i] = read_nothingd;
        readmemd[0xa470+i] = read_nothingd;
        writemem[0x8470+i] = write_nothing;
        writemem[0xa470+i] = write_nothing;
        writememb[0x8470+i] = write_nothingb;
        writememb[0xa470+i] = write_nothingb;
        writememh[0x8470+i] = write_nothingh;
        writememh[0xa470+i] = write_nothingh;
        writememd[0x8470+i] = write_nothingd;
        writememd[0xa470+i] = write_nothingd;
    }

    //init SI registers
    readmem[0x8480] = read_si;
    readmem[0xa480] = read_si;
    readmemb[0x8480] = read_sib;
    readmemb[0xa480] = read_sib;
    readmemh[0x8480] = read_sih;
    readmemh[0xa480] = read_sih;
    readmemd[0x8480] = read_sid;
    readmemd[0xa480] = read_sid;
    writemem[0x8480] = write_si;
    writemem[0xa480] = write_si;
    writememb[0x8480] = write_sib;
    writememb[0xa480] = write_sib;
    writememh[0x8480] = write_sih;
    writememh[0xa480] = write_sih;
    writememd[0x8480] = write_sid;
    writememd[0xa480] = write_sid;
    si_register.si_dram_addr = 0;
    si_register.si_pif_addr_rd64b = 0;
    si_register.si_pif_addr_wr64b = 0;
    si_register.si_stat = 0;
    readsi[0x0] = &si_register.si_dram_addr;
    readsi[0x4] = &si_register.si_pif_addr_rd64b;
    readsi[0x8] = &trash;
    readsi[0x10] = &si_register.si_pif_addr_wr64b;
    readsi[0x14] = &trash;
    readsi[0x18] = &si_register.si_stat;

    for (i=0x1c; i<0x10000; i++) readsi[i] = &trash;
    for (i=0x481; i<0x800; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    //init flashram / sram
    readmem[0x8800] = read_flashram_status;
    readmem[0xa800] = read_flashram_status;
    readmemb[0x8800] = read_flashram_statusb;
    readmemb[0xa800] = read_flashram_statusb;
    readmemh[0x8800] = read_flashram_statush;
    readmemh[0xa800] = read_flashram_statush;
    readmemd[0x8800] = read_flashram_statusd;
    readmemd[0xa800] = read_flashram_statusd;
    writemem[0x8800] = write_flashram_dummy;
    writemem[0xa800] = write_flashram_dummy;
    writememb[0x8800] = write_flashram_dummyb;
    writememb[0xa800] = write_flashram_dummyb;
    writememh[0x8800] = write_flashram_dummyh;
    writememh[0xa800] = write_flashram_dummyh;
    writememd[0x8800] = write_flashram_dummyd;
    writememd[0xa800] = write_flashram_dummyd;
    readmem[0x8801] = read_nothing;
    readmem[0xa801] = read_nothing;
    readmemb[0x8801] = read_nothingb;
    readmemb[0xa801] = read_nothingb;
    readmemh[0x8801] = read_nothingh;
    readmemh[0xa801] = read_nothingh;
    readmemd[0x8801] = read_nothingd;
    readmemd[0xa801] = read_nothingd;
    writemem[0x8801] = write_flashram_command;
    writemem[0xa801] = write_flashram_command;
    writememb[0x8801] = write_flashram_commandb;
    writememb[0xa801] = write_flashram_commandb;
    writememh[0x8801] = write_flashram_commandh;
    writememh[0xa801] = write_flashram_commandh;
    writememd[0x8801] = write_flashram_commandd;
    writememd[0xa801] = write_flashram_commandd;

    for (i=0x802; i<0x1000; i++)
    {
        readmem[0x8000+i] = read_nothing;
        readmem[0xa000+i] = read_nothing;
        readmemb[0x8000+i] = read_nothingb;
        readmemb[0xa000+i] = read_nothingb;
        readmemh[0x8000+i] = read_nothingh;
        readmemh[0xa000+i] = read_nothingh;
        readmemd[0x8000+i] = read_nothingd;
        readmemd[0xa000+i] = read_nothingd;
        writemem[0x8000+i] = write_nothing;
        writemem[0xa000+i] = write_nothing;
        writememb[0x8000+i] = write_nothingb;
        writememb[0xa000+i] = write_nothingb;
        writememh[0x8000+i] = write_nothingh;
        writememh[0xa000+i] = write_nothingh;
        writememd[0x8000+i] = write_nothingd;
        writememd[0xa000+i] = write_nothingd;
    }

    //init rom area
    for (i=0; i<(rom_size >> 16); i++)
    {
        readmem[0x9000+i] = read_rom;
        readmem[0xb000+i] = read_rom;
        readmemb[0x9000+i] = read_romb;
        readmemb[0xb000+i] = read_romb;
        readmemh[0x9000+i] = read_romh;
        readmemh[0xb000+i] = read_romh;
        readmemd[0x9000+i] = read_romd;
        readmemd[0xb000+i] = read_romd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_rom;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }
    for (i=(rom_size >> 16); i<0xfc0; i++)
    {
        readmem[0x9000+i] = read_nothing;
        readmem[0xb000+i] = read_nothing;
        readmemb[0x9000+i] = read_nothingb;
        readmemb[0xb000+i] = read_nothingb;
        readmemh[0x9000+i] = read_nothingh;
        readmemh[0xb000+i] = read_nothingh;
        readmemd[0x9000+i] = read_nothingd;
        readmemd[0xb000+i] = read_nothingd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_nothing;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }

    // init CIC type
    for (i = 0x40/4; i < (0x1000/4); i++)
        CRC += ((uint32_t*)rom)[i];

    switch(CRC)
    {
        default:
            DebugMessage(M64MSG_WARNING, "Unknown CIC type (%08x)! using CIC 6102.", CRC);
        /* CIC 6102 */
        case 0x000000D057C85244LL: CIC_Chip = 2; break;
        /* CIC 6101 */
        case 0x000000D0027FDF31LL:
        case 0x000000CFFB631223LL: CIC_Chip = 1; break;
        /* CIC 6103 */
        case 0x000000D6497E414BLL: CIC_Chip = 3; break;
        /* CIC 6105 */
        case 0x0000011A49F60E96LL: CIC_Chip = 5; break;
        /* CIC 6106 */
        case 0x000000D6D5BE5580LL: CIC_Chip = 6; break;
    }

    //init PIF_RAM
    readmem[0x9fc0] = read_pif;
    readmem[0xbfc0] = read_pif;
    readmemb[0x9fc0] = read_pifb;
    readmemb[0xbfc0] = read_pifb;
    readmemh[0x9fc0] = read_pifh;
    readmemh[0xbfc0] = read_pifh;
    readmemd[0x9fc0] = read_pifd;
    readmemd[0xbfc0] = read_pifd;
    writemem[0x9fc0] = write_pif;
    writemem[0xbfc0] = write_pif;
    writememb[0x9fc0] = write_pifb;
    writememb[0xbfc0] = write_pifb;
    writememh[0x9fc0] = write_pifh;
    writememh[0xbfc0] = write_pifh;
    writememd[0x9fc0] = write_pifd;
    writememd[0xbfc0] = write_pifd;
    for (i=0; i<(0x40/4); i++) PIF_RAM[i]=0;

    for (i=0xfc1; i<0x1000; i++)
    {
        readmem[0x9000+i] = read_nothing;
        readmem[0xb000+i] = read_nothing;
        readmemb[0x9000+i] = read_nothingb;
        readmemb[0xb000+i] = read_nothingb;
        readmemh[0x9000+i] = read_nothingh;
        readmemh[0xb000+i] = read_nothingh;
        readmemd[0x9000+i] = read_nothingd;
        readmemd[0xb000+i] = read_nothingd;
        writemem[0x9000+i] = write_nothing;
        writemem[0xb000+i] = write_nothing;
        writememb[0x9000+i] = write_nothingb;
        writememb[0xb000+i] = write_nothingb;
        writememh[0x9000+i] = write_nothingh;
        writememh[0xb000+i] = write_nothingh;
        writememd[0x9000+i] = write_nothingd;
        writememd[0xb000+i] = write_nothingd;
    }

    flashram_info.use_flashram = 0;
    init_flashram();

    frameBufferInfos[0].addr = 0;
    fast_memory = 1;
    firstFrameBufferSetting = 1;

    DebugMessage(M64MSG_VERBOSE, "Memory initialized");
    return 0;
}

void free_memory(void)
{
}

void do_SP_Task(void)
{
    int save_pc = g_sp.regs2[SP_PC_REG] & ~0xfff;
    if (g_sp.mem[0xfc0/4] == 1)
    {
        if (dpc_register.dpc_status & 0x2) // DP frozen (DK64, BC)
        {
            // don't do the task now
            // the task will be done when DP is unfreezed (see update_DPC)
            return;
        }
        
        // unprotecting old frame buffers
        if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite &&
                frameBufferInfos[0].addr)
        {
            int i;
            for (i=0; i<6; i++)
            {
                if (frameBufferInfos[i].addr)
                {
                    int j;
                    int start = frameBufferInfos[i].addr & 0x7FFFFF;
                    int end = start + frameBufferInfos[i].width*
                              frameBufferInfos[i].height*
                              frameBufferInfos[i].size - 1;
                    start = start >> 16;
                    end = end >> 16;

                    for (j=start; j<=end; j++)
                    {
#ifdef DBG
                        if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                        {
                            readmem[0x8000+j] = read_rdram_break;
                            readmemb[0x8000+j] = read_rdramb_break;
                            readmemh[0x8000+j] = read_rdramh_break;
                            readmemd[0xa000+j] = read_rdramd_break;
                        }
                        else
                        {
#endif
                            readmem[0x8000+j] = read_rdram;
                            readmemb[0x8000+j] = read_rdramb;
                            readmemh[0x8000+j] = read_rdramh;
                            readmemd[0xa000+j] = read_rdramd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                        {
                            readmem[0xa000+j] = read_rdram_break;
                            readmemb[0xa000+j] = read_rdramb_break;
                            readmemh[0xa000+j] = read_rdramh_break;
                            readmemd[0x8000+j] = read_rdramd_break;
                        }
                        else
                        {
#endif
                            readmem[0xa000+j] = read_rdram;
                            readmemb[0xa000+j] = read_rdramb;
                            readmemh[0xa000+j] = read_rdramh;
                            readmemd[0x8000+j] = read_rdramd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                        {
                            writemem[0x8000+j] = write_rdram_break;
                            writememb[0x8000+j] = write_rdramb_break;
                            writememh[0x8000+j] = write_rdramh_break;
                            writememd[0x8000+j] = write_rdramd_break;
                        }
                        else
                        {
#endif
                            writemem[0x8000+j] = write_rdram;
                            writememb[0x8000+j] = write_rdramb;
                            writememh[0x8000+j] = write_rdramh;
                            writememd[0x8000+j] = write_rdramd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                        {
                            writemem[0xa000+j] = write_rdram_break;
                            writememb[0xa000+j] = write_rdramb_break;
                            writememh[0xa000+j] = write_rdramh_break;
                            writememd[0xa000+j] = write_rdramd_break;
                        }
                        else
                        {
#endif
                            writemem[0xa000+j] = write_rdram;
                            writememb[0xa000+j] = write_rdramb;
                            writememh[0xa000+j] = write_rdramh;
                            writememd[0xa000+j] = write_rdramd;
#ifdef DBG
                        }
#endif
                    }
                }
            }
        }

        //gfx.processDList();
        g_sp.regs2[SP_PC_REG] &= 0xfff;
        timed_section_start(TIMED_SECTION_GFX);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_GFX);
        g_sp.regs2[SP_PC_REG] |= save_pc;
        new_frame();

        update_count();
        if (g_mi.regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 1000);
        if (g_mi.regs[MI_INTR_REG] & MI_INTR_DP)
            add_interupt_event(DP_INT, 1000);
        g_mi.regs[MI_INTR_REG] &= ~(MI_INTR_SP | MI_INTR_DP);
        g_sp.regs[SP_STATUS_REG] &= ~0x303;

        // protecting new frame buffers
        if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite)
            gfx.fBGetFrameBufferInfo(frameBufferInfos);
        if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite
                && frameBufferInfos[0].addr)
        {
            int i;
            for (i=0; i<6; i++)
            {
                if (frameBufferInfos[i].addr)
                {
                    int j;
                    int start = frameBufferInfos[i].addr & 0x7FFFFF;
                    int end = start + frameBufferInfos[i].width*
                              frameBufferInfos[i].height*
                              frameBufferInfos[i].size - 1;
                    int start1 = start;
                    int end1 = end;
                    start >>= 16;
                    end >>= 16;
                    for (j=start; j<=end; j++)
                    {
#ifdef DBG
                        if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                        {
                            readmem[0x8000+j] = read_rdramFB_break;
                            readmemb[0x8000+j] = read_rdramFBb_break;
                            readmemh[0x8000+j] = read_rdramFBh_break;
                            readmemd[0xa000+j] = read_rdramFBd_break;
                        }
                        else
                        {
#endif
                            readmem[0x8000+j] = read_rdramFB;
                            readmemb[0x8000+j] = read_rdramFBb;
                            readmemh[0x8000+j] = read_rdramFBh;
                            readmemd[0xa000+j] = read_rdramFBd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                        {
                            readmem[0xa000+j] = read_rdramFB_break;
                            readmemb[0xa000+j] = read_rdramFBb_break;
                            readmemh[0xa000+j] = read_rdramFBh_break;
                            readmemd[0x8000+j] = read_rdramFBd_break;
                        }
                        else
                        {
#endif
                            readmem[0xa000+j] = read_rdramFB;
                            readmemb[0xa000+j] = read_rdramFBb;
                            readmemh[0xa000+j] = read_rdramFBh;
                            readmemd[0x8000+j] = read_rdramFBd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                        {
                            writemem[0x8000+j] = write_rdramFB_break;
                            writememb[0x8000+j] = write_rdramFBb_break;
                            writememh[0x8000+j] = write_rdramFBh_break;
                            writememd[0x8000+j] = write_rdramFBd_break;
                        }
                        else
                        {
#endif
                            writemem[0x8000+j] = write_rdramFB;
                            writememb[0x8000+j] = write_rdramFBb;
                            writememh[0x8000+j] = write_rdramFBh;
                            writememd[0x8000+j] = write_rdramFBd;
#ifdef DBG
                        }
                        if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                              M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                        {
                            writemem[0xa000+j] = write_rdramFB_break;
                            writememb[0xa000+j] = write_rdramFBb_break;
                            writememh[0xa000+j] = write_rdramFBh_break;
                            writememd[0xa000+j] = write_rdramFBd_break;
                        }
                        else
                        {
#endif
                            writemem[0xa000+j] = write_rdramFB;
                            writememb[0xa000+j] = write_rdramFBb;
                            writememh[0xa000+j] = write_rdramFBh;
                            writememd[0xa000+j] = write_rdramFBd;
#ifdef DBG
                        }
#endif
                    }
                    start <<= 4;
                    end <<= 4;
                    for (j=start; j<=end; j++)
                    {
                        if (j>=start1 && j<=end1) framebufferRead[j]=1;
                        else framebufferRead[j] = 0;
                    }

                    if (firstFrameBufferSetting)
                    {
                        firstFrameBufferSetting = 0;
                        fast_memory = 0;
                        for (j=0; j<0x100000; j++)
                            invalid_code[j] = 1;
                    }
                }
            }
        }
    }
    else if (g_sp.mem[0xfc0/4] == 2)
    {
        //audio.processAList();
        g_sp.regs2[SP_PC_REG] &= 0xfff;
        timed_section_start(TIMED_SECTION_AUDIO);
        rsp.doRspCycles(0xFFFFFFFF);
        timed_section_end(TIMED_SECTION_AUDIO);
        g_sp.regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (g_mi.regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 4000/*500*/);
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SP;
        g_sp.regs[SP_STATUS_REG] &= ~0x303;
    }
    else
    {
        g_sp.regs2[SP_PC_REG] &= 0xfff;
        rsp.doRspCycles(0xFFFFFFFF);
        g_sp.regs2[SP_PC_REG] |= save_pc;

        update_count();
        if (g_mi.regs[MI_INTR_REG] & MI_INTR_SP)
            add_interupt_event(SP_INT, 0/*100*/);
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SP;
        g_sp.regs[SP_STATUS_REG] &= ~0x203;
    }
}

void make_w_dpc_status(void)
{
    dpc_register.w_dpc_status = 0;

    if ((dpc_register.dpc_status & 0x0001) == 0)
        dpc_register.w_dpc_status |= 0x0000001;
    else
        dpc_register.w_dpc_status |= 0x0000002;

    if ((dpc_register.dpc_status & 0x0002) == 0)
        dpc_register.w_dpc_status |= 0x0000004;
    else
        dpc_register.w_dpc_status |= 0x0000008;

    if ((dpc_register.dpc_status & 0x0004) == 0)
        dpc_register.w_dpc_status |= 0x0000010;
    else
        dpc_register.w_dpc_status |= 0x0000020;
}

static void update_DPC(void)
{
    if (dpc_register.w_dpc_status & 0x1) // clear xbus_dmem_dma
        dpc_register.dpc_status &= ~0x1;
    if (dpc_register.w_dpc_status & 0x2) // set xbus_dmem_dma
        dpc_register.dpc_status |= 0x1;

    if (dpc_register.w_dpc_status & 0x4) // clear freeze
    {
        dpc_register.dpc_status &= ~0x2;

        // see do_SP_task for more info
        if ((g_sp.regs[SP_STATUS_REG] & 0x3) == 0) // !halt && !broke
            do_SP_Task();
    }
    if (dpc_register.w_dpc_status & 0x8) // set freeze
        dpc_register.dpc_status |= 0x2;

    if (dpc_register.w_dpc_status & 0x10) // clear flush
        dpc_register.dpc_status &= ~0x4;
    if (dpc_register.w_dpc_status & 0x20) // set flush
        dpc_register.dpc_status |= 0x4;
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

    read_rdram_ram(&g_rdram, address, &value);
    *rdword = value;
}

void read_rdramb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rdram_ram(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rdramh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rdram_ram(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rdramd(void)
{
    uint32_t w[2];

    read_rdram_ram(&g_rdram, address    , &w[0]);
    read_rdram_ram(&g_rdram, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void read_rdramFB(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    framebufferRead[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                framebufferRead[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
    read_rdram();
}

void read_rdramFBb(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    framebufferRead[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                framebufferRead[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
    read_rdramb();
}

void read_rdramFBh(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    framebufferRead[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                framebufferRead[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
    read_rdramh();
}

void read_rdramFBd(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    framebufferRead[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                framebufferRead[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
    read_rdramd();
}

void write_rdram(void)
{
#if defined( COUNT_WRITE_RDRAM_CALLS )
	printf( "write_rdram, word=%i, count: %i", word, writerdram_count );
	writerdram_count++;
#endif
    write_rdram_ram(&g_rdram, address, word, ~0U);
}

void write_rdramb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rdram_ram(&g_rdram, address, value, mask);
}

void write_rdramh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rdram_ram(&g_rdram, address, value, mask);
}

void write_rdramd(void)
{
    write_rdram_ram(&g_rdram, address    , dword >> 32, ~0U);
    write_rdram_ram(&g_rdram, address + 4, dword      , ~0U);
}

void write_rdramFB(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address, 4);
        }
    }
    write_rdram();
}

void write_rdramFBb(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address^S8, 1);
        }
    }
    write_rdramb();
}

void write_rdramFBh(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address^S16, 2);
        }
    }
    write_rdramh();
}

void write_rdramFBd(void)
{
    int i;
    for (i=0; i<6; i++)
    {
        if (frameBufferInfos[i].addr)
        {
            unsigned int start = frameBufferInfos[i].addr & 0x7FFFFF;
            unsigned int end = start + frameBufferInfos[i].width*
                               frameBufferInfos[i].height*
                               frameBufferInfos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address, 8);
        }
    }
    write_rdramd();
}

void read_rdramreg(void)
{
    uint32_t value;

    read_rdram_regs(&g_rdram, address, &value);
    *rdword = value;
}

void read_rdramregb(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_rdram_regs(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rdramregh(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_rdram_regs(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rdramregd(void)
{
    uint32_t w[2];

    read_rdram_regs(&g_rdram, address    , &w[0]);
    read_rdram_regs(&g_rdram, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_rdramreg(void)
{
    write_rdram_regs(&g_rdram, address, word, ~0U);
}

void write_rdramregb(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_rdram_regs(&g_rdram, address, value, mask);
}

void write_rdramregh(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_rdram_regs(&g_rdram, address, value, mask);
}

void write_rdramregd(void)
{
    write_rdram_regs(&g_rdram, address    , dword >> 32, ~0U);
    write_rdram_regs(&g_rdram, address + 4, dword      , ~0U);
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
    *rdword = *(readdp[*address_low]);
}

void read_dpb(void)
{
    *rdword = *((unsigned char*)readdp[*address_low & 0xfffc]
                + ((*address_low&3)^S8) );
}

void read_dph(void)
{
    *rdword = *((unsigned short*)((unsigned char*)readdp[*address_low & 0xfffc]
                                  + ((*address_low&3)^S16) ));
}

void read_dpd(void)
{
    *rdword = ((unsigned long long int)(*readdp[*address_low])<<32) |
              *readdp[*address_low+4];
}

void write_dp(void)
{
    switch (*address_low)
    {
    case 0xc:
        dpc_register.w_dpc_status = word;
        update_DPC();
    case 0x8:
    case 0x10:
    case 0x14:
    case 0x18:
    case 0x1c:
        return;
        break;
    }
    *readdp[*address_low] = word;
    switch (*address_low)
    {
    case 0x0:
        dpc_register.dpc_current = dpc_register.dpc_start;
        break;
    case 0x4:
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;
    }
}

void write_dpb(void)
{
    switch (*address_low)
    {
    case 0xc:
    case 0xd:
    case 0xe:
    case 0xf:
        *((unsigned char*)&dpc_register.w_dpc_status
          + ((*address_low&3)^S8) ) = cpu_byte;
        update_DPC();
    case 0x8:
    case 0x9:
    case 0xa:
    case 0xb:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
    case 0x1f:
        return;
        break;
    }
    *((unsigned char*)readdp[*address_low & 0xfffc]
      + ((*address_low&3)^S8) ) = cpu_byte;
    switch (*address_low)
    {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
        dpc_register.dpc_current = dpc_register.dpc_start;
        break;
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;
    }
}

void write_dph(void)
{
    switch (*address_low)
    {
    case 0xc:
    case 0xe:
        *((unsigned short*)((unsigned char*)&dpc_register.w_dpc_status
                            + ((*address_low&3)^S16) )) = hword;
        update_DPC();
    case 0x8:
    case 0xa:
    case 0x10:
    case 0x12:
    case 0x14:
    case 0x16:
    case 0x18:
    case 0x1a:
    case 0x1c:
    case 0x1e:
        return;
        break;
    }
    *((unsigned short*)((unsigned char*)readdp[*address_low & 0xfffc]
                        + ((*address_low&3)^S16) )) = hword;
    switch (*address_low)
    {
    case 0x0:
    case 0x2:
        dpc_register.dpc_current = dpc_register.dpc_start;
        break;
    case 0x4:
    case 0x6:
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;
    }
}

void write_dpd(void)
{
    switch (*address_low)
    {
    case 0x8:
        dpc_register.w_dpc_status = (unsigned int) (dword & 0xFFFFFFFF);
        update_DPC();
        return;
        break;
    case 0x10:
    case 0x18:
        return;
        break;
    }
    *readdp[*address_low] = (unsigned int) (dword >> 32);
    *readdp[*address_low+4] = (unsigned int) (dword & 0xFFFFFFFF);
    switch (*address_low)
    {
    case 0x0:
        dpc_register.dpc_current = dpc_register.dpc_start;
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;
    }
}

void read_dps(void)
{
    *rdword = *(readdps[*address_low]);
}

void read_dpsb(void)
{
    *rdword = *((unsigned char*)readdps[*address_low & 0xfffc]
                + ((*address_low&3)^S8) );
}

void read_dpsh(void)
{
    *rdword = *((unsigned short*)((unsigned char*)readdps[*address_low & 0xfffc]
                                  + ((*address_low&3)^S16) ));
}

void read_dpsd(void)
{
    *rdword = ((unsigned long long int)(*readdps[*address_low])<<32) |
              *readdps[*address_low+4];
}

void write_dps(void)
{
    *readdps[*address_low] = word;
}

void write_dpsb(void)
{
    *((unsigned char*)readdps[*address_low & 0xfffc]
      + ((*address_low&3)^S8) ) = cpu_byte;
}

void write_dpsh(void)
{
    *((unsigned short*)((unsigned char*)readdps[*address_low & 0xfffc]
                        + ((*address_low&3)^S16) )) = hword;
}

void write_dpsd(void)
{
    *readdps[*address_low] = (unsigned int) (dword >> 32);
    *readdps[*address_low+4] = (unsigned int) (dword & 0xFFFFFFFF);
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

    read_ri_regs(&g_rdram, address, &value);
    *rdword = value;
}

void read_rib(void)
{
    uint32_t value;
    unsigned shift = bshift(address);

    read_ri_regs(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xff;
}

void read_rih(void)
{
    uint32_t value;
    unsigned shift = hshift(address);

    read_ri_regs(&g_rdram, address, &value);
    *rdword = (value >> shift) & 0xffff;
}

void read_rid(void)
{
    uint32_t w[2];

    read_ri_regs(&g_rdram, address    , &w[0]);
    read_ri_regs(&g_rdram, address + 4, &w[1]);

    *rdword = ((uint64_t)w[0] << 32) | w[1];
}

void write_ri(void)
{
    write_ri_regs(&g_rdram, address, word, ~0U);
}

void write_rib(void)
{
    unsigned shift = bshift(address);
    uint32_t value = (uint32_t)cpu_byte << shift;
    uint32_t mask = (uint32_t)0xff << shift;

    write_ri_regs(&g_rdram, address, value, mask);
}

void write_rih(void)
{
    unsigned shift = hshift(address);
    uint32_t value = (uint32_t)hword << shift;
    uint32_t mask = (uint32_t)0xffff << shift;

    write_ri_regs(&g_rdram, address, value, mask);
}

void write_rid(void)
{
    write_ri_regs(&g_rdram, address    , dword >> 32, ~0U);
    write_ri_regs(&g_rdram, address + 4, dword      , ~0U);
}

void read_si(void)
{
    *rdword = *(readsi[*address_low]);
}

void read_sib(void)
{
    *rdword = *((unsigned char*)readsi[*address_low & 0xfffc]
                + ((*address_low&3)^S8) );
}

void read_sih(void)
{
    *rdword = *((unsigned short*)((unsigned char*)readsi[*address_low & 0xfffc]
                                  + ((*address_low&3)^S16) ));
}

void read_sid(void)
{
    *rdword = ((unsigned long long int)(*readsi[*address_low])<<32) |
              *readsi[*address_low+4];
}

void write_si(void)
{
    switch (*address_low)
    {
    case 0x0:
        si_register.si_dram_addr = word;
        return;
        break;
    case 0x4:
        si_register.si_pif_addr_rd64b = word;
        dma_si_read();
        return;
        break;
    case 0x10:
        si_register.si_pif_addr_wr64b = word;
        dma_si_write();
        return;
        break;
    case 0x18:
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        si_register.si_stat &= ~0x1000;
        check_interupt();
        return;
        break;
    }
}

void write_sib(void)
{
    switch (*address_low)
    {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
        *((unsigned char*)&si_register.si_dram_addr
          + ((*address_low&3)^S8) ) = cpu_byte;
        return;
        break;
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
        *((unsigned char*)&si_register.si_pif_addr_rd64b
          + ((*address_low&3)^S8) ) = cpu_byte;
        dma_si_read();
        return;
        break;
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
        *((unsigned char*)&si_register.si_pif_addr_wr64b
          + ((*address_low&3)^S8) ) = cpu_byte;
        dma_si_write();
        return;
        break;
    case 0x18:
    case 0x19:
    case 0x1a:
    case 0x1b:
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        si_register.si_stat &= ~0x1000;
        check_interupt();
        return;
        break;
    }
}

void write_sih(void)
{
    switch (*address_low)
    {
    case 0x0:
    case 0x2:
        *((unsigned short*)((unsigned char*)&si_register.si_dram_addr
                            + ((*address_low&3)^S16) )) = hword;
        return;
        break;
    case 0x4:
    case 0x6:
        *((unsigned short*)((unsigned char*)&si_register.si_pif_addr_rd64b
                            + ((*address_low&3)^S16) )) = hword;
        dma_si_read();
        return;
        break;
    case 0x10:
    case 0x12:
        *((unsigned short*)((unsigned char*)&si_register.si_pif_addr_wr64b
                            + ((*address_low&3)^S16) )) = hword;
        dma_si_write();
        return;
        break;
    case 0x18:
    case 0x1a:
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        si_register.si_stat &= ~0x1000;
        check_interupt();
        return;
        break;
    }
}

void write_sid(void)
{
    switch (*address_low)
    {
    case 0x0:
        si_register.si_dram_addr = (unsigned int) (dword >> 32);
        si_register.si_pif_addr_rd64b = (unsigned int) (dword & 0xFFFFFFFF);
        dma_si_read();
        return;
        break;
    case 0x10:
        si_register.si_pif_addr_wr64b = (unsigned int) (dword >> 32);
        dma_si_write();
        return;
        break;
    case 0x18:
        g_mi.regs[MI_INTR_REG] &= ~MI_INTR_SI;
        si_register.si_stat &= ~0x1000;
        check_interupt();
        return;
        break;
    }
}

void read_flashram_status(void)
{
    if (flashram_info.use_flashram != -1 && *address_low == 0)
    {
        *rdword = flashram_status();
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown read in read_flashram_status()");
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
    if (flashram_info.use_flashram != -1 && *address_low == 0)
    {
        flashram_command(word);
        flashram_info.use_flashram = 1;
    }
    else
        DebugMessage(M64MSG_ERROR, "unknown write in write_flashram_command()");
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

static unsigned int lastwrite = 0;

void read_rom(void)
{
    if (lastwrite)
    {
        *rdword = lastwrite;
        lastwrite = 0;
    }
    else
        *rdword = *((unsigned int *)(rom + (address & 0x03FFFFFF)));
}

void read_romb(void)
{
    *rdword = *(rom + ((address^S8) & 0x03FFFFFF));
}

void read_romh(void)
{
    *rdword = *((unsigned short *)(rom + ((address^S16) & 0x03FFFFFF)));
}

void read_romd(void)
{
    *rdword = ((unsigned long long)(*((unsigned int *)(rom+(address&0x03FFFFFF))))<<32)|
              *((unsigned int *)(rom + ((address+4)&0x03FFFFFF)));
}

void write_rom(void)
{
    lastwrite = word;
}

void read_pif(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a word in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = sl(*((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)));
}

void read_pifb(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a byte in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = *(PIF_RAMb + ((address & 0x7FF) - 0x7C0));
}

void read_pifh(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a hword in PIF at invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = (*(PIF_RAMb + ((address & 0x7FF) - 0x7C0)) << 8) |
              *(PIF_RAMb + (((address+1) & 0x7FF) - 0x7C0));
}

void read_pifd(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "reading a double word in PIF in invalid address 0x%x", address);
        *rdword = 0;
        return;
    }

    *rdword = ((unsigned long long)sl(*((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0))) << 32)|
              sl(*((unsigned int *)(PIF_RAMb + ((address+4) & 0x7FF) - 0x7C0)));
}

void write_pif(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a word in PIF at invalid address 0x%x", address);
        return;
    }

    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) = sl(word);
    if ((address & 0x7FF) == 0x7FC)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifb(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a byte in PIF at invalid address 0x%x", address);
        return;
    }

    *(PIF_RAMb + (address & 0x7FF) - 0x7C0) = cpu_byte;
    if ((address & 0x7FF) == 0x7FF)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifh(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a hword in PIF at invalid address 0x%x", address);
        return;
    }

    *(PIF_RAMb + (address & 0x7FF) - 0x7C0) = hword >> 8;
    *(PIF_RAMb + ((address+1) & 0x7FF) - 0x7C0) = hword & 0xFF;
    if ((address & 0x7FF) == 0x7FE)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

void write_pifd(void)
{
    if ((*address_low > 0x7FF) || (*address_low < 0x7C0))
    {
        DebugMessage(M64MSG_ERROR, "writing a double word in PIF at 0x%x", address);
        return;
    }

    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) =
        sl((unsigned int)(dword >> 32));
    *((unsigned int *)(PIF_RAMb + (address & 0x7FF) - 0x7C0)) =
        sl((unsigned int)(dword & 0xFFFFFFFF));
    if ((address & 0x7FF) == 0x7F8)
    {
        if (PIF_RAMb[0x3F] == 0x08)
        {
            PIF_RAMb[0x3F] = 0;
            update_count();
            add_interupt_event(SI_INT, /*0x100*/0x900);
        }
        else
            update_pif_write();
    }
}

unsigned int *fast_mem_access(unsigned int address)
{
    /* This code is performance critical, specially on pure interpreter mode.
     * Removing error checking saves some time, but the emulator may crash. */
    if (address < 0x80000000 || address >= 0xc0000000)
        address = virtual_to_physical_address(address, 2);

    if ((address & 0x1FFFFFFF) >= 0x10000000)
        return (unsigned int *)rom + ((address & 0x1FFFFFFF) - 0x10000000)/4;
    else if ((address & 0x1FFFFFFF) < RDRAM_MAX_SIZE)
        return (unsigned int *)g_rdram.ram + (address & 0x1FFFFFFF)/4;
    else if (address >= 0xa4000000 && address <= 0xa4002000)
        return &g_sp.mem[(address & 0x1fff) / 4];
    else
        return NULL;
}
