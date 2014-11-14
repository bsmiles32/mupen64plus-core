/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pif.c                                                   *
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

#include "pif.h"

#include "controller.h"
#include "eeprom.h"
#include "game_controller.h"
#include "n64_cic_nus_6105.h"

#include "api/m64p_types.h"
#include "api/callbacks.h"

#include "plugin/plugin.h"

#include <stddef.h>

//#define DEBUG_PIF


void update_pif_write(struct si_controller* si)
{
    char challenge[30], response[30];
    int i=0, channel=0;
    if (si->pif_ram[0x3F] > 1)
    {
        switch (si->pif_ram[0x3F])
        {
        case 0x02:
#ifdef DEBUG_PIF
            DebugMessage(M64MSG_INFO, "update_pif_write() pif_ram[0x3f] = 2 - CIC challenge");
#endif
            // format the 'challenge' message into 30 nibbles for X-Scale's CIC code
            for (i = 0; i < 15; i++)
            {
                challenge[i*2] =   (si->pif_ram[48+i] >> 4) & 0x0f;
                challenge[i*2+1] =  si->pif_ram[48+i]       & 0x0f;
            }
            // calculate the proper response for the given challenge (X-Scale's algorithm)
            n64_cic_nus_6105(challenge, response, CHL_LEN - 2);
            si->pif_ram[46] = 0;
            si->pif_ram[47] = 0;
            // re-format the 'response' into a byte stream
            for (i = 0; i < 15; i++)
            {
                si->pif_ram[48+i] = (response[i*2] << 4) + response[i*2+1];
            }
            // the last byte (2 nibbles) is always 0
            si->pif_ram[63] = 0;
            break;
        case 0x08:
#ifdef DEBUG_PIF
            DebugMessage(M64MSG_INFO, "update_pif_write() pif_ram[0x3f] = 8");
#endif
            si->pif_ram[0x3F] = 0;
            break;
        default:
            DebugMessage(M64MSG_ERROR, "error in update_pif_write(): %x", si->pif_ram[0x3F]);
        }
        return;
    }

    while (i<0x40)
    {
        switch (si->pif_ram[i])
        {
        case 0x00:
            channel++;
            if (channel > 6) i=0x40;
            break;
        case 0xFF:
            break;
        default:
            if (!(si->pif_ram[i] & 0xC0))
            {
                if (channel < 4)
                    process_controller_command(si, channel, &si->pif_ram[i]);
                else if (channel == 4)
                    process_eeprom_command(&si->eeprom, &si->pif_ram[i]);
                else
                    DebugMessage(M64MSG_ERROR, "channel >= 4 in update_pif_write");
                i += si->pif_ram[i] + (si->pif_ram[(i+1)] & 0x3F) + 1;
                channel++;
            }
            else
                i=0x40;
        }
        i++;
    }
    //si->pif_ram[0x3F] = 0;
    input.controllerCommand(-1, NULL);
}

void update_pif_read(struct si_controller* si)
{
    int i=0, channel=0;
    while (i<0x40)
    {
        switch (si->pif_ram[i])
        {
        case 0x00:
            channel++;
            if (channel > 6) i=0x40;
            break;
        case 0xFE:
            i = 0x40;
            break;
        case 0xFF:
            break;
        case 0xB4:
        case 0x56:
        case 0xB8:
            break;
        default:
            if (!(si->pif_ram[i] & 0xC0))
            {
                if (channel < 4)
                    read_controller(si, channel, &si->pif_ram[i]);

                i += si->pif_ram[i] + (si->pif_ram[(i+1)] & 0x3F) + 1;
                channel++;
            }
            else
                i=0x40;
        }
        i++;
    }
    input.readController(-1, NULL);
}

