/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eeprom.c                                                *
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

#include "eeprom.h"
#include "pif.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "main/main.h"
#include "main/rom.h"
#include "main/util.h"

static char *get_eeprom_path(void)
{
    return formatstr("%s%s.eep", get_savesrampath(), ROM_SETTINGS.goodname);
}

static void eeprom_format(struct eeprom_controller* eeprom)
{
    memset(eeprom->eeprom, 0xff, EEPROM_SIZE);
}

static void eeprom_read_file(struct eeprom_controller* eeprom)
{
    char *filename = get_eeprom_path();

    switch (read_from_file(filename, eeprom->eeprom, EEPROM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_VERBOSE, "couldn't open eeprom file '%s' for reading", filename);
            eeprom_format(eeprom);
            break;
        case file_read_error:
            DebugMessage(M64MSG_WARNING, "fread() failed for 2kb eeprom file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static void eeprom_write_file(struct eeprom_controller* eeprom)
{
    char *filename = get_eeprom_path();

    switch (write_to_file(filename, eeprom->eeprom, EEPROM_SIZE))
    {
        case file_open_error:
            DebugMessage(M64MSG_WARNING, "couldn't open eeprom file '%s' for writing", filename);
            break;
        case file_write_error:
            DebugMessage(M64MSG_WARNING, "fwrite() failed for 2kb eeprom file '%s'", filename);
            break;
        default: break;
    }

    free(filename);
}

static uint8_t two_digit_bcd(unsigned int n)
{
    n %= 100;
    return ((n / 10) << 4) | (n % 10);
}


static void process_check_command(struct eeprom_controller* eeprom, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "process_eeprom_command() check size");
#endif
    if (command[1] != 3)
    {
        command[1] |= 0x40;
        if ((command[1] & 3) > 0)
            command[3] = 0;
        if ((command[1] & 3) > 1)
            command[4] = (ROM_SETTINGS.savetype != EEPROM_16KB) ? 0x80 : 0xc0;
        if ((command[1] & 3) > 2)
            command[5] = 0;
    }
    else
    {
        command[3] = 0;
        command[4] = (ROM_SETTINGS.savetype != EEPROM_16KB) ? 0x80 : 0xc0;
        command[5] = 0;
    }
}

static void process_read_command(struct eeprom_controller* eeprom, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "process_eeprom_command() read 8-byte block %i", command[3]);
#endif
    eeprom_read_file(eeprom);
    memcpy(&command[4], eeprom->eeprom + command[3]*8, 8);
}


static void process_write_command(struct eeprom_controller* eeprom, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "process_eeprom_command() write 8-byte block %i", command[3]);
#endif
    eeprom_read_file(eeprom);
    memcpy(eeprom->eeprom + command[3]*8, &command[4], 8);
    eeprom_write_file(eeprom);
}

static void process_rtc_status_query(struct eeprom_controller* eeprom, uint8_t* command)
{
#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "process_eeprom_command() RTC status query");
#endif
    command[3] = 0x00;
    command[4] = 0x10;
    command[5] = 0x00;
}

static void process_read_rtc_block(struct eeprom_controller* eeprom, uint8_t* command)
{
    time_t curtime_time;
    struct tm* curtime;

#ifdef DEBUG_PIF
    DebugMessage(M64MSG_INFO, "process_eeprom_command() read RTC block %i", command[3]);
#endif
    switch (command[3]) // block number
    {
    case 0:
        command[4] = 0x00;
        command[5] = 0x02;
        command[12] = 0x00;
        break;
    case 1:
        DebugMessage(M64MSG_ERROR,
                     "RTC command in process_eeprom_command(): read block %d",
                     command[2]);
        break;
    case 2:
        time(&curtime_time);
        curtime = localtime(&curtime_time);
        command[4] = two_digit_bcd(curtime->tm_sec);
        command[5] = two_digit_bcd(curtime->tm_min);
        command[6] = 0x80 + two_digit_bcd(curtime->tm_hour);
        command[7] = two_digit_bcd(curtime->tm_mday);
        command[8] = two_digit_bcd(curtime->tm_wday);
        command[9] = two_digit_bcd(curtime->tm_mon + 1);
        command[10] = two_digit_bcd(curtime->tm_year);
        command[11] = two_digit_bcd(curtime->tm_year / 100);
        command[12] = 0x00;	// status
        break;
    }
}

static void process_write_rtc_block(struct eeprom_controller* eeprom, uint8_t* command)
{
    DebugMessage(M64MSG_ERROR,
                 "RTC write in process_eeprom_command(): %d not yet implemented",
                 command[2]);
}



void process_eeprom_command(struct eeprom_controller* eeprom, uint8_t* command)
{
    switch (command[2])
    {
    case PIF_CMD_GET_STATUS:
        process_check_command(eeprom, command);
        break;
    case PIF_CMD_EEPROM_READ:
        process_read_command(eeprom, command);
        break;
    case PIF_CMD_EEPROM_WRITE:
        process_write_command(eeprom, command);
        break;
    case PIF_CMD_RTC_GET_STATUS:
        process_rtc_status_query(eeprom, command);
        break;
    case PIF_CMD_RTC_READ:
        process_read_rtc_block(eeprom, command);
        break;
    case PIF_CMD_RTC_WRITE:
        process_write_rtc_block(eeprom, command);
        break;
    default:
        DebugMessage(M64MSG_ERROR, "Unknown command in process_eeprom_command(): %x", command[2]);
    }
}

