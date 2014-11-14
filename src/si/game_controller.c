/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - game_controller.c                                       *
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


#include "game_controller.h"
#include "controller.h"
#include "mempack.h"
#include "pif.h"

#include <stdint.h>
#include <string.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#ifdef COMPARE_CORE
#include "api/debugger.h"
#endif

#include "plugin/plugin.h"

static void read_buttons(uint8_t channel, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO,
                 "Channel %i Command 1 read buttons",
                 channel);
#endif

    if (Controls[channel].Present)
    {
        BUTTONS Keys;
        input.getKeys(channel, &Keys);
        *((unsigned int *)(command + 3)) = Keys.Value;
#ifdef COMPARE_CORE
        CoreCompareDataSync(4, command+3);
#endif
    }
}

static void read_pack(uint8_t channel, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO,
                 "Channel %i Command 2 read controller pack (in Input plugin)",
                 channel);
#endif

    if (Controls[channel].Present)
    {
        if (Controls[channel].Plugin == PLUGIN_RAW)
            if (input.readController)
                input.readController(channel, command);
    }
}

static void write_pack(uint8_t channel, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO,
                 "Channel %i Command 3 write controller pack (in Input plugin)",
                 channel);
#endif

    if (Controls[channel].Present)
    {
        if (Controls[channel].Plugin == PLUGIN_RAW)
            if (input.readController)
                input.readController(channel, command);
    }
}


static void internal_ReadController(uint8_t channel, uint8_t* command)
{
    switch (command[2])
    {
    case PIF_CMD_CONTROLLER_READ:
        read_buttons(channel, command);
        break;
    case PIF_CMD_CONTROLLERPAK_READ:
        read_pack(channel, command);
        break;
    case PIF_CMD_CONTROLLERPAK_WRITE:
        write_pack(channel, command);
        break;
    }
}

static void internal_ControllerCommand(struct si_controller* si, uint8_t channel, uint8_t* command)
{
    switch (command[2])
    {
    case PIF_CMD_GET_STATUS:
    case PIF_CMD_RESET:
        if ((command[1] & 0x80))
            break;
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO,
                     "Channel %i Command %02x check pack present",
                     channel, command[2]);
#endif
        if (Controls[channel].Present)
        {
            command[3] = 0x05;
            command[4] = 0x00;
            switch (Controls[channel].Plugin)
            {
            case PLUGIN_MEMPAK:
                command[5] = 1;
                break;
            case PLUGIN_RAW:
                command[5] = 1;
                break;
            default:
                command[5] = 0;
                break;
            }
        }
        else
            command[1] |= 0x80;
        break;

    case PIF_CMD_CONTROLLER_READ:
#ifdef DEBUG_PIF
        DebugMessage(M64MSG_INFO,
                     "Channel %i Command 1 check controller present",
                     channel);
#endif
        if (!Controls[channel].Present)
            command[1] |= 0x80;
        break;

    case PIF_CMD_CONTROLLERPAK_READ:
        if (Controls[channel].Present)
        {
            switch (Controls[channel].Plugin)
            {
            case PLUGIN_MEMPAK:
                read_mempack(&si->mempack, channel, command);
                break;
            case PLUGIN_RAW:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO,
                             "Channel %i Command 2 controllerCommand (in Input plugin)",
                             channel);
#endif
                if (input.controllerCommand)
                    input.controllerCommand(channel, command);
                break;
            default:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO,
                             "Channel %i Command 2 (no pack plugged in)",
                             channel);
#endif
                memset(&command[5], 0, 0x20);
                command[0x25] = 0;
            }
        }
        else
            command[1] |= 0x80;
        break;

    case PIF_CMD_CONTROLLERPAK_WRITE:
        if (Controls[channel].Present)
        {
            switch (Controls[channel].Plugin)
            {
            case PLUGIN_MEMPAK:
                write_mempack(&si->mempack, channel, command);
                break;
            case PLUGIN_RAW:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO,
                             "Channel %i Command 3 controllerCommand (in Input plugin)",
                             channel);
#endif
                if (input.controllerCommand)
                    input.controllerCommand(channel, command);
                break;
            default:
#ifdef DEBUG_PIF
                DebugMessage(M64MSG_INFO,
                             "Channel %i Command 3 (no pack plugged in)",
                             channel);
#endif
                command[0x25] = mempack_crc(&command[5]);
            }
        }
        else
            command[1] |= 0x80;
        break;
    }
}



void read_controller(struct si_controller* si, uint8_t channel, uint8_t* command)
{
    if (Controls[channel].Present && Controls[channel].RawData)
        input.readController(channel, command);
    else
        internal_ReadController(channel, command);
}

void process_controller_command(struct si_controller* si, uint8_t channel, uint8_t* command)
{
    if (Controls[channel].Present && Controls[channel].RawData)
        input.controllerCommand(channel, command);
    else
        internal_ControllerCommand(si, channel, command);
}
