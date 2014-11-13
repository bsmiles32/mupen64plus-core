/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - eeprom.h                                                *
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

#ifndef M64P_SI_EEPROM_H
#define M64P_SI_EEPROM_H

#include <stdint.h>

enum
{
    EEPROM_CMD_CHECK = 0,
    EEPROM_CMD_READ = 4,
    EEPROM_CMD_WRITE = 5,
    EEPROM_CMD_RTC_GET_STATUS = 6,
    EEPROM_CMD_RTC_READ_BLOCK = 7,
    EEPROM_CMD_RTC_WRITE_BLOCK = 8
};

enum { EEPROM_SIZE  = 0x800 };

struct eeprom_controller
{
    uint8_t eeprom[EEPROM_SIZE];
};

int init_eeprom(struct eeprom_controller* eeprom);

void process_eeprom_command(struct eeprom_controller* eeprom, uint8_t* command);

#endif

