/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mempack.c                                               *
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mempack.h"

#include "r4300/r4300.h"
#include "r4300/interupt.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/debugger.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"
#include "memory/memory.h"
#include "plugin/plugin.h"

static char *get_mempack_path(void)
{
    return formatstr("%s%s.mpk", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void mempack_format(struct mempack_controller* mp)
{
    size_t i, j;
    static const uint8_t init[] =
    {
        0x81,0x01,0x02,0x03, 0x04,0x05,0x06,0x07, 0x08,0x09,0x0a,0x0b, 0x0c,0x0d,0x0e,0x0f,
        0x10,0x11,0x12,0x13, 0x14,0x15,0x16,0x17, 0x18,0x19,0x1a,0x1b, 0x1c,0x1d,0x1e,0x1f,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0x05,0x1a,0x5f,0x13, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff, 0xff,0xff,0x01,0xff, 0x66,0x25,0x99,0xcd,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,
        0x00,0x71,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03, 0x00,0x03,0x00,0x03
    };
    
    for(i = 0; i < MAX_CONTROLLERS; ++i)
    {
        for(j = 0; j < MEMPACK_SIZE; j += 2)
        {
            mp->mempacks[i][j] = 0;
            mp->mempacks[i][j+1] = 0x03;
        }

        memcpy(mp->mempacks[i], init, 272);
    }
}

static void mempack_read_file(struct mempack_controller* mp)
{
    char *filename = get_mempack_path();

    switch (read_from_file(filename, mp->mempacks, MAX_CONTROLLERS*MEMPACK_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open memory pack file '%s' for reading", filename);
            mempack_format(mp);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed for 128kb mempack file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void mempack_write_file(struct mempack_controller* mp)
{
    char *filename = get_mempack_path();

    switch (write_to_file(filename, mp->mempacks, MAX_CONTROLLERS*MEMPACK_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open memory pack file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed for 128kb mempack file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}



uint8_t mempack_crc(uint8_t* data)
{
    size_t i;
    uint8_t mask;
    uint8_t xor_tap;
    uint8_t crc = 0;

    for(i = 0; i <= 0x20; ++i)
    {
        for (mask = 0x80; mask >= 1; mask >>= 1)
        {
            xor_tap = (crc & 0x80) ? 0x85 : 0x00;
            crc <<= 1;
            if (i != 0x20 && (data[i] & mask)) crc |= 1;
            crc ^= xor_tap;
        }
    }

    return crc;
}

void read_mempack(struct mempack_controller* mp, uint8_t channel, uint8_t* command)
{
    uint16_t address = (command[3] << 8) | command[4];

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO,
                 "Channel %i command 2 read mempack address %04x",
                 channel, address);
#endif

    if (address == 0x8001)
    {
        memset(&command[5], 0, 0x20);
        command[0x25] = mempack_crc(&command[5]);
    }
    else
    {
        address &= 0xffe0;
        if (address <= 0x7fe0)
        {
            mempack_read_file(mp);
            memcpy(&command[5], &mp->mempacks[channel][address], 0x20);
        }
        else
        {
            memset(&command[5], 0, 0x20);
        }
        command[0x25] = mempack_crc(&command[5]);
    }
}

void write_mempack(struct mempack_controller* mp, uint8_t channel, uint8_t* command)
{
    uint16_t address = (command[3] << 8) | command[4];

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO,
                 "Channel %i command 3 write mempack address %04x",
                 channel, address);
#endif

    if (address == 0x8001)
    {
        command[0x25] = mempack_crc(&command[5]);
    }
    else
    {
        address &= 0xffe0;
        if (address <= 0x7fe0)
        {
            mempack_read_file(mp);
            memcpy(&mp->mempacks[channel][address], &command[5], 0x20);
            mempack_write_file(mp);
        }
        command[0x25] = mempack_crc(&command[5]);
    }
}

