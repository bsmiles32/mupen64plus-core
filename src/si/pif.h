/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - pif.h                                                   *
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

#ifndef M64P_SI_PIF_H
#define M64P_SI_PIF_H

struct si_controller;

enum
{
    PIF_CMD_GET_STATUS,
    PIF_CMD_CONTROLLER_READ,
    PIF_CMD_CONTROLLERPAK_READ,
    PIF_CMD_CONTROLLERPAK_WRITE,
    PIF_CMD_EEPROM_READ,
    PIF_CMD_EEPROM_WRITE,
    PIF_CMD_RTC_GET_STATUS,
    PIF_CMD_RTC_READ,
    PIF_CMD_RTC_WRITE,
    PIF_CMD_RESET = 0xff
};

void update_pif_write(struct si_controller* si);
void update_pif_read(struct si_controller* si);

#endif

