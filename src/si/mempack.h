/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - mempack.h                                               *
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

#ifndef M64P_SI_MEMPACK_H
#define M64P_SI_MEMPACK_H

#include <stdint.h>

enum { MAX_CONTROLLERS = 4 };
enum { MEMPACK_SIZE  = 0x8000 };

struct mempack_controller
{
    uint8_t mempacks[MAX_CONTROLLERS][MEMPACK_SIZE];
};

uint8_t mempack_crc(uint8_t* data);
void read_mempack(struct mempack_controller* mp, uint8_t channel, uint8_t* command);
void write_mempack(struct mempack_controller* mp, uint8_t channel, uint8_t* command);

#endif

