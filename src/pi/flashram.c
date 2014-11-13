/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - flashram.c                                              *
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

#include "flashram.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"
#include "memory/memory.h"
#include "r4300/r4300.h"
#include "rdram/controller.h"


static char *get_flashram_path(void)
{
    return formatstr("%s%s.fla", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void flashram_format(struct flashram_controller* flashram)
{
    memset(flashram->flashram, 0xff, FLASHRAM_SIZE);
}

static void flashram_read_file(struct flashram_controller* flashram)
{
    char *filename = get_flashram_path();

    flashram_format(flashram);
    switch (read_from_file(filename, flashram->flashram, FLASHRAM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open flash ram file '%s' for reading", filename);
            flashram_format(flashram);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "couldn't read 128kb flash ram file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void flashram_write_file(struct flashram_controller* flashram)
{
    char *filename = get_flashram_path();

    switch (write_to_file(filename, flashram->flashram, FLASHRAM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open flash ram file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "couldn't write 128kb flash ram file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void flashram_command(struct flashram_controller* flashram,
                             uint8_t* ram, unsigned int command)
{
    switch (command & 0xff000000)
    {
    case 0x4b000000:
        flashram->erase_offset = (command & 0xffff) * 128;
        break;
    case 0x78000000:
        flashram->mode = FLASHRAM_MODE_ERASE;
        flashram->status = 0x1111800800c20000LL;
        break;
    case 0xa5000000:
        flashram->erase_offset = (command & 0xffff) * 128;
        flashram->status = 0x1111800400c20000LL;
        break;
    case 0xb4000000:
        flashram->mode = FLASHRAM_MODE_WRITE;
        break;
    case 0xd2000000:  // execute
        switch (flashram->mode)
        {
        case FLASHRAM_MODE_NOPES:
        case FLASHRAM_MODE_READ:
            break;
        case FLASHRAM_MODE_ERASE:
        {
            unsigned int i;
            flashram_read_file(flashram);
            for (i=flashram->erase_offset; i<(flashram->erase_offset+128); i++)
            {
                flashram->flashram[i^S8] = 0xff;
            }
            flashram_write_file(flashram);
        }
        break;
        case FLASHRAM_MODE_WRITE:
        {
            int i;
            flashram_read_file(flashram);
            for (i=0; i<128; i++)
            {
                flashram->flashram[(flashram->erase_offset+i)^S8] =
                    ram[(flashram->write_pointer+i)^S8];
            }
            flashram_write_file(flashram);
        }
        break;
        case FLASHRAM_MODE_STATUS:
            break;
        default:
            DebugMessage(M64MSG_WARNING, "unknown flashram command with mode:%x", (int)flashram->mode);
            stop=1;
            break;
        }
        flashram->mode = FLASHRAM_MODE_NOPES;
        break;
    case 0xe1000000:
        flashram->mode = FLASHRAM_MODE_STATUS;
        flashram->status = 0x1111800100c20000LL;
        break;
    case 0xf0000000:
        flashram->mode = FLASHRAM_MODE_READ;
        flashram->status = 0x11118004f0000000LL;
        break;
    default:
        DebugMessage(M64MSG_WARNING, "unknown flashram command: %x", (int)command);
        break;
    }
}



int init_flashram(struct flashram_controller* flashram)
{
    flashram->mode = FLASHRAM_MODE_NOPES;
    flashram->status = 0;

    return 0;
}

int pi_read_flashram_status(struct pi_controller* pi,
                            uint32_t address, uint32_t* value)
{
    struct flashram_controller* flashram = &pi->flashram;

    if (flashram->use_flashram != -1 && (address & 0xffff) == 0)
    {
        *value = (flashram->status >> 32);
        flashram->use_flashram = 1;
    }
    else
    {
        DebugMessage(M64MSG_ERROR, "unknown read in read_flashram_status()");
        return -1;
    }

    return 0;
}

int pi_write_flashram_command(struct pi_controller* pi,
                              uint32_t address, uint32_t value, uint32_t mask)
{
    struct flashram_controller* flashram = &pi->flashram;

    if (flashram->use_flashram != -1 && (address & 0xffff) == 0)
    {
        // FIXME
        flashram_command(flashram, (uint8_t*)g_rdram.ram, value & mask);
        flashram->use_flashram = 1;
    }
    else
    {
        DebugMessage(M64MSG_ERROR, "unknown write in write_flashram_command()");
        return -1;
    }

    return 0;
}


void dma_read_flashram(struct pi_controller* pi, uint32_t* ram)
{
    unsigned int i;
    struct flashram_controller* flashram = &pi->flashram;

    switch (flashram->mode)
    {
    case FLASHRAM_MODE_STATUS:
        ram[pi->regs[PI_DRAM_ADDR_REG]/4] = (uint32_t)(flashram->status >> 32);
        ram[pi->regs[PI_DRAM_ADDR_REG]/4+1] = (uint32_t)(flashram->status);
        break;
    case FLASHRAM_MODE_READ:
        flashram_read_file(flashram);
        for (i=0; i<(pi->regs[PI_WR_LEN_REG] & 0x0FFFFFF)+1; i++)
        {
            ((unsigned char*)ram)[(pi->regs[PI_DRAM_ADDR_REG]+i)^S8]=
                flashram->flashram[(((pi->regs[PI_CART_ADDR_REG]-0x08000000)&0xFFFF)*2+i)^S8];
        }
        break;
    default:
        DebugMessage(M64MSG_WARNING, "unknown dma_read_flashram: %x", flashram->mode);
        stop=1;
        break;
    }
}

void dma_write_flashram(struct pi_controller* pi)
{
    struct flashram_controller* flashram = &pi->flashram;

    switch (flashram->mode)
    {
    case FLASHRAM_MODE_WRITE:
        flashram->write_pointer = pi->regs[PI_DRAM_ADDR_REG];
        break;
    default:
        DebugMessage(M64MSG_ERROR, "unknown dma_write_flashram: %x", flashram->mode);
        stop=1;
        break;
    }
}

