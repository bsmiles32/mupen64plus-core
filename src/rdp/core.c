/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - core.c                                                  *
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

#include "core.h"

#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "memory/memory.h"
#include "plugin/plugin.h"
#include "r4300/cached_interp.h"
#include "r4300/interupt.h"
#include "r4300/mi.h"
#include "r4300/recomph.h"

#ifdef DBG
#include "debugger/dbg_types.h"
#include "debugger/dbg_memory.h"
#include "debugger/dbg_breakpoints.h"
#endif

void do_SP_Task(void);

#if 0
static const char* dpc_regs_name[DPC_REGS_COUNT] =
{
    "DPC_START_REG",
    "DPC_END_REG",
    "DPC_CURRENT_REG",
    "DPC_STATUS_REG",
    "DPC_CLOCK_REG",
    "DPC_BUFBUSY_REG",
    "DPC_PIPEBUSY_REG",
    "DPC_TMEM_REG"
};

static const char* dps_regs_name[DPS_REGS_COUNT] =
{
    "DPS_TBIST_REG",
    "DPS_TEST_MODE_REG",
    "DPS_BUFTEST_ADDR_REG",
    "DPS_BUFTEST_DATA_REG"
};
#endif

int init_rdp(struct rdp_core* dp)
{
    memset(dp, 0, sizeof(*dp));

    fast_memory = 1;
    dp->fb_first_protection = 1;

    return 0;
}

static inline uint32_t dpc_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_dpc_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value)
{
    uint32_t reg = dpc_reg(address);

    *value = dp->dpc_regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", dpc_regs_name[reg], *value);

    return 0;
}

int write_dpc_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t v;
    uint32_t reg = dpc_reg(address);

    switch(reg)
    {
    case DPC_START_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        dp->dpc_regs[DPC_CURRENT_REG] = dp->dpc_regs[DPC_START_REG];
        break;
    case DPC_END_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        gfx.processRDPList();
        g_mi.regs[MI_INTR_REG] |= MI_INTR_DP;
        check_interupt();
        break;

    case DPC_STATUS_REG:
        v = value & mask;

        /* clear / set xbus_dmem_dma */
        if (v & 0x1) dp->dpc_regs[DPC_STATUS_REG] &= ~0x1;
        if (v & 0x2) dp->dpc_regs[DPC_STATUS_REG] |= 0x1;

        /* clear freeze */
        if (v & 0x4)
        {
            dp->dpc_regs[DPC_STATUS_REG] &= ~0x2;

            /* see do_SP_task for more info */
            if ((g_sp.regs[SP_STATUS_REG] & 0x3) == 0) // !halt && !broke
                do_SP_Task();
        }
        /* set freeze */
        if (v & 0x8) dp->dpc_regs[DPC_STATUS_REG] |= 0x2;
        /* clear / set flush */
        if (v & 0x10) dp->dpc_regs[DPC_STATUS_REG] &= ~0x4;
        if (v & 0x20) dp->dpc_regs[DPC_STATUS_REG] |= 0x4;
        break;

    case DPC_CURRENT_REG:
    case DPC_CLOCK_REG:
    case DPC_BUFBUSY_REG:
    case DPC_PIPEBUSY_REG:
        /* read only */
        break;

    case DPC_TMEM_REG:
        masked_write(&dp->dpc_regs[reg], value, mask);
        break;

    default:
        DebugMessage(M64MSG_WARNING, "Illegal access @=%08x v=%08x m=%08x",
                     address, value, mask);
        return -1;
    }

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", dpc_regs_name[reg], value);
    return 0;
}



static inline uint32_t dps_reg(uint32_t address)
{
    return (address & 0xffff) >> 2;
}

int read_dps_regs(struct rdp_core* dp,
                  uint32_t address, uint32_t* value)
{
    uint32_t reg = dps_reg(address);

    *value = dp->dps_regs[reg];

//    DebugMessage(M64MSG_WARNING, "%s -> %08x", dps_regs_name[reg], *value);

    return 0;
}

int write_dps_regs(struct rdp_core* dp,
                   uint32_t address, uint32_t value, uint32_t mask)
{
    uint32_t reg = dps_reg(address);

    masked_write(&dp->dps_regs[reg], value, mask);

//    DebugMessage(M64MSG_WARNING, "%s <- %08x", dps_regs_name[reg], value);

    return 0;
}



void pre_framebuffer_read(struct rdp_core* dp, uint32_t address)
{
    int i;
    for(i = 0; i < FB_INFO_COUNT; ++i)
    {
        if (dp->fb_infos[i].addr)
        {
            unsigned int start = dp->fb_infos[i].addr & 0x7FFFFF;
            unsigned int end = start + dp->fb_infos[i].width*
                               dp->fb_infos[i].height*
                               dp->fb_infos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end &&
                    dp->fb_read_dirty[(address & 0x7FFFFF)>>12])
            {
                gfx.fBRead(address);
                dp->fb_read_dirty[(address & 0x7FFFFF)>>12] = 0;
            }
        }
    }
}

void pre_framebuffer_write(struct rdp_core* dp, uint32_t address, size_t size)
{
    int i;
    for(i = 0; i < FB_INFO_COUNT; ++i)
    {
        if (dp->fb_infos[i].addr)
        {
            unsigned int start = dp->fb_infos[i].addr & 0x7FFFFF;
            unsigned int end = start + dp->fb_infos[i].width*
                               dp->fb_infos[i].height*
                               dp->fb_infos[i].size - 1;
            if ((address & 0x7FFFFF) >= start && (address & 0x7FFFFF) <= end)
                gfx.fBWrite(address, size);
        }
    }
}

#define R(x) read_ ## x ## b, read_ ## x ## h, read_## x, read_## x ## d
#define W(x) write_ ## x ## b, write_ ## x ## h, write_ ## x, write_ ## x ## d

void unprotect_framebuffers(struct rdp_core* dp)
{
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite &&
            dp->fb_infos[0].addr)
    {
        int i;
        for(i = 0; i < FB_INFO_COUNT; ++i)
        {
            if (dp->fb_infos[i].addr)
            {
                int j;
                int start = dp->fb_infos[i].addr & 0x7FFFFF;
                int end = start + dp->fb_infos[i].width*
                          dp->fb_infos[i].height*
                          dp->fb_infos[i].size - 1;
                start = start >> 16;
                end = end >> 16;

                for (j=start; j<=end; j++)
                {
#ifdef DBG
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        map_region_r(0x8000+j,
                                read_rdramb_break,
                                read_rdramh_break,
                                read_rdram_break,
                                read_rdramd_break);
                    }
                    else
                    {
#endif
                        map_region_r(0x8000+j, R(rdram));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        map_region_r(0xa000+j,
                                read_rdramb_break,
                                read_rdramh_break,
                                read_rdram_break,
                                read_rdramd_break);
                    }
                    else
                    {
#endif
                        map_region_r(0xa000+j, R(rdram));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        map_region_w(0x8000+j,
                                write_rdramb_break,
                                write_rdramh_break,
                                write_rdram_break,
                                write_rdramd_break);
                    }
                    else
                    {
#endif
                        map_region_w(0x8000+j, W(rdram));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        map_region_w(0xa000+j,
                                write_rdramb_break,
                                write_rdramh_break,
                                write_rdram_break,
                                write_rdramd_break);
                    }
                    else
                    {
#endif
                        map_region_w(0xa000+j, W(rdram));
#ifdef DBG
                    }
#endif
                }
            }
        }
    }
}

void protect_framebuffers(struct rdp_core* dp)
{
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite)
        gfx.fBGetFrameBufferInfo(dp->fb_infos);
    if (gfx.fBGetFrameBufferInfo && gfx.fBRead && gfx.fBWrite
            && dp->fb_infos[0].addr)
    {
        int i;
        for(i = 0; i < FB_INFO_COUNT; ++i)
        {
            if (dp->fb_infos[i].addr)
            {
                int j;
                int start = dp->fb_infos[i].addr & 0x7FFFFF;
                int end = start + dp->fb_infos[i].width*
                          dp->fb_infos[i].height*
                          dp->fb_infos[i].size - 1;
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
                        map_region_r(0x8000+j,
                                read_rdramFBb_break,
                                read_rdramFBh_break,
                                read_rdramFB_break,
                                read_rdramFBd_break);
                    }
                    else
                    {
#endif
                        map_region_r(0x8000+j, R(rdramFB));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_READ) != -1)
                    {
                        map_region_r(0xa000+j,
                                read_rdramFBb_break,
                                read_rdramFBh_break,
                                read_rdramFB_break,
                                read_rdramFBd_break);
                    }
                    else
                    {
#endif
                        map_region_r(0xa000+j, R(rdramFB));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0x80000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        map_region_w(0x8000+j,
                                write_rdramFBb_break,
                                write_rdramFBh_break,
                                write_rdramFB_break,
                                write_rdramFBd_break);
                    }
                    else
                    {
#endif
                        map_region_w(0x8000+j, W(rdramFB));
#ifdef DBG
                    }
                    if (lookup_breakpoint(0xa0000000 + j * 0x10000, 0x10000,
                                          M64P_BKP_FLAG_ENABLED | M64P_BKP_FLAG_WRITE) != -1)
                    {
                        map_region_w(0xa000+j,
                                write_rdramFBb_break,
                                write_rdramFBh_break,
                                write_rdramFB_break,
                                write_rdramFBd_break);
                    }
                    else
                    {
#endif
                        map_region_w(0xa000+j, W(rdramFB));
#ifdef DBG
                    }
#endif
                }
                start <<= 4;
                end <<= 4;
                for (j=start; j<=end; j++)
                {
                    if (j>=start1 && j<=end1) dp->fb_read_dirty[j]=1;
                    else dp->fb_read_dirty[j] = 0;
                }

                if (dp->fb_first_protection)
                {
                    dp->fb_first_protection = 0;
                    fast_memory = 0;
                    for (j=0; j<0x100000; j++)
                        invalid_code[j] = 1;
                }
            }
        }
    }
}

