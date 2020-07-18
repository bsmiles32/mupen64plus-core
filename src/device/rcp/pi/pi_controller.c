/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pi_controller.c                                         *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
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

#include "pi_controller.h"

#define M64P_CORE_PROTOTYPES 1
#include <stdint.h>
#include <string.h>

#include "api/callbacks.h"
#include "api/m64p_types.h"
#include "device/device.h"
#include "device/dd/dd_controller.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"
#include "device/rcp/mi/mi_controller.h"
#include "device/rcp/rdp/rdp_core.h"
#include "device/rcp/ri/ri_controller.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

int validate_pi_request(struct pi_controller* pi)
{
    if (pi->regs[PI_STATUS_REG] & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {
        pi->regs[PI_STATUS_REG] |= PI_STATUS_ERROR;
        return 0;
    }

    return 1;
}

static unsigned bsd_domain(uint32_t address)
{
    uint8_t x = address >> 24;
    return (x == 0x05 || ((x >= 0x08) && (x < 0x10)))
        ? 1
        : 0;
}

static unsigned int compute_cycles(uint32_t length, uint8_t lat, uint8_t pwd, uint8_t rls, uint8_t pgs)
{
    if (length == 0)
        return 0;

    // ALE,ALH are maintained for 7 cycles each
    // plus 1+LAT cycles of extra latency before word transfer
    unsigned int c_per_address_latch = 7+7+1+lat;
    // each word takes 1+PWD, 1+RLS cycles
    unsigned int c_per_word = 2 + pwd + rls;
    // ??? there might be some extra latency each 64 word (internal PI buffering ?) given the chronograms (http://n64.icequake.net/mirror/www.crazynation.org/N64/)
    unsigned int c_per_64w = 28; // PI_tests suggests 28 ??? Kx>=25, SC, PG>=30

    // AL is necessary at each page boundary (e.g. after 2^(1+PGS) words)
    unsigned int n_al = 1 + ((length-1) >> (1 + 1 + pgs));
    // 1 word = 2 bytes
    unsigned int n_words = 1 + (length-1) / 2;
    unsigned int n_64w = (length-1) / 128;

    // 3/2 comes from 93.75/62.5 (3 CPU cycles for 2 PI cycles).
    // 1/2 comes from CP0_COUNT getting incremented at 1/2 MClock
    return (n_al * c_per_address_latch
          + n_words * c_per_word
          + n_64w * c_per_64w) * 3/4;
}

static void dma_pi_read(struct pi_controller* pi)
{
    if (!validate_pi_request(pi))
        return;

    uint32_t cart_addr = pi->regs[PI_CART_ADDR_REG] & ~UINT32_C(1);
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG] & 0xffffff;
    uint32_t length = (pi->regs[PI_RD_LEN_REG] & UINT32_C(0x00fffffe)) + 2;
    const uint8_t* dram = (uint8_t*)pi->ri->rdram->dram;

    const struct pi_dma_handler* handler = NULL;
    void* opaque = NULL;

    pi->get_pi_dma_handler(pi->cart, pi->dd, cart_addr, &opaque, &handler);

    if (handler == NULL) {
        DebugMessage(M64MSG_WARNING, "Unknown PI DMA read: 0x%" PRIX32 " -> 0x%" PRIX32 " (0x%" PRIX32 ")", dram_addr, cart_addr, length);
        return;
    }

    unsigned b = bsd_domain(cart_addr) * 4;
    unsigned int cycles = compute_cycles(length,
        pi->regs[b+PI_BSD_DOM1_LAT_REG],
        pi->regs[b+PI_BSD_DOM1_PWD_REG],
        pi->regs[b+PI_BSD_DOM1_RLS_REG],
        pi->regs[b+PI_BSD_DOM1_PGS_REG]);

    /* XXX: simulate bus contention */
    cycles += add_random_interrupt_time(pi->mi->r4300);

#if 0
    DebugMessage(M64MSG_WARNING, "DMA READ: %s, cycles=%d, length=%08x, lat=%02x pwd=%02x rls=%02x pgs=%02x", handler->name, cycles, length,
        pi->regs[b+PI_BSD_DOM1_LAT_REG], pi->regs[b+PI_BSD_DOM1_PWD_REG], pi->regs[b+PI_BSD_DOM1_RLS_REG], pi->regs[b+PI_BSD_DOM1_PGS_REG]);
#endif

    pre_framebuffer_read(&pi->dp->fb, dram_addr);

    handler->dma_read(opaque, dram, dram_addr, cart_addr, length);

    /* Mark DMA as busy */
    pi->regs[PI_STATUS_REG] |= PI_STATUS_DMA_BUSY;

    /* schedule end of dma interrupt event */
    cp0_update_count(pi->mi->r4300);
    add_interrupt_event(&pi->mi->r4300->cp0, PI_INT, cycles);
}

static void dma_pi_write(struct pi_controller* pi)
{
    if (!validate_pi_request(pi))
        return;

    uint32_t cart_addr = pi->regs[PI_CART_ADDR_REG] & ~UINT32_C(1);
    uint32_t dram_addr = pi->regs[PI_DRAM_ADDR_REG] & 0xffffff;
    uint32_t length = (pi->regs[PI_WR_LEN_REG] & UINT32_C(0x00fffffe)) + 2;
    uint8_t* dram = (uint8_t*)pi->ri->rdram->dram;

    const struct pi_dma_handler* handler = NULL;
    void* opaque = NULL;

    pi->get_pi_dma_handler(pi->cart, pi->dd, cart_addr, &opaque, &handler);

    if (handler == NULL) {
        DebugMessage(M64MSG_WARNING, "Unknown PI DMA write: 0x%" PRIX32 " -> 0x%" PRIX32 " (0x%" PRIX32 ")", cart_addr, dram_addr, length);
        return;
    }

    unsigned b = bsd_domain(cart_addr) * 4;
    unsigned int cycles = compute_cycles(length,
        pi->regs[b+PI_BSD_DOM1_LAT_REG],
        pi->regs[b+PI_BSD_DOM1_PWD_REG],
        pi->regs[b+PI_BSD_DOM1_RLS_REG],
        pi->regs[b+PI_BSD_DOM1_PGS_REG]);

    /* XXX: simulate bus contention */
    cycles += add_random_interrupt_time(pi->mi->r4300);

#if 0
    DebugMessage(M64MSG_WARNING, "DMA WRITE: %s, cycles=%d, length=%08x, lat=%02x pwd=%02x rls=%02x pgs=%02x", handler->name, cycles, length,
        pi->regs[b+PI_BSD_DOM1_LAT_REG], pi->regs[b+PI_BSD_DOM1_PWD_REG], pi->regs[b+PI_BSD_DOM1_RLS_REG], pi->regs[b+PI_BSD_DOM1_PGS_REG]);
#endif

    handler->dma_write(opaque, dram, dram_addr, cart_addr, length);


    post_framebuffer_write(&pi->dp->fb, dram_addr, length);

    /* Mark DMA as busy */
    pi->regs[PI_STATUS_REG] |= PI_STATUS_DMA_BUSY;

    /* schedule end of dma interrupt event */
    cp0_update_count(pi->mi->r4300);
    add_interrupt_event(&pi->mi->r4300->cp0, PI_INT, cycles);
}


void init_pi(struct pi_controller* pi,
             pi_dma_handler_getter get_pi_dma_handler,
             struct cart* cart,
             struct dd_controller* dd,
             struct mi_controller* mi,
             struct ri_controller* ri,
             struct rdp_core* dp)
{
    pi->get_pi_dma_handler = get_pi_dma_handler;
    pi->cart = cart;
    pi->dd = dd;
    pi->mi = mi;
    pi->ri = ri;
    pi->dp = dp;
}

void poweron_pi(struct pi_controller* pi)
{
    memset(pi->regs, 0, PI_REGS_COUNT*sizeof(uint32_t));
}

void read_pi_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t reg = pi_reg(address);

    *value = pi->regs[reg];
}

void write_pi_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    uint32_t reg = pi_reg(address);

    switch (reg)
    {
    case PI_CART_ADDR_REG:
        if (pi->dd != NULL) {
            masked_write(&pi->regs[PI_CART_ADDR_REG], value, mask);
            dd_on_pi_cart_addr_write(pi->dd, pi->regs[PI_CART_ADDR_REG]);
            return;
        }
        break;

    case PI_RD_LEN_REG:
        masked_write(&pi->regs[PI_RD_LEN_REG], value, mask);
        dma_pi_read(pi);
        return;

    case PI_WR_LEN_REG:
        masked_write(&pi->regs[PI_WR_LEN_REG], value, mask);
        dma_pi_write(pi);
        return;

    case PI_STATUS_REG:
        if (value & mask & 2)
            clear_rcp_interrupt(pi->mi, MI_INTR_PI);
        if (value & mask & 1)
            pi->regs[PI_STATUS_REG] = 0;
        return;

    case PI_BSD_DOM1_LAT_REG:
    case PI_BSD_DOM1_PWD_REG:
    case PI_BSD_DOM1_PGS_REG:
    case PI_BSD_DOM1_RLS_REG:
    case PI_BSD_DOM2_LAT_REG:
    case PI_BSD_DOM2_PWD_REG:
    case PI_BSD_DOM2_PGS_REG:
    case PI_BSD_DOM2_RLS_REG:
        masked_write(&pi->regs[reg], value & 0xff, mask);
        return;
    }

    masked_write(&pi->regs[reg], value, mask);
}

void pi_end_of_dma_event(void* opaque)
{
    struct pi_controller* pi = (struct pi_controller*)opaque;
    pi->regs[PI_STATUS_REG] &= ~(PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);

    if (pi->dd != NULL) {
        if ((pi->regs[PI_CART_ADDR_REG] == MM_DD_C2S_BUFFER) ||
            (pi->regs[PI_CART_ADDR_REG] == MM_DD_DS_BUFFER)) {
            dd_update_bm(pi->dd);
        }
    }

    raise_rcp_interrupt(pi->mi, MI_INTR_PI);
}
