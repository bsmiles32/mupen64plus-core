/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#ifndef M64P_BIG_ENDIAN
#if defined(__GNUC__) && (__GNUC__ > 4  || (__GNUC__ == 4 && __GNUC_MINOR__ >= 2))
#define sl(x) __builtin_bswap32(x)
#else
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)
#endif
#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif



int init_memory(void);
void free_memory(void);

void map_region_r(uint16_t region,
                  int (*readfn)(uint32_t, uint32_t*));
void map_region_w(uint16_t region,
                  int (writefn)(uint32_t, uint32_t, uint32_t));
void map_region(uint16_t region,
        int (*readfn)(uint32_t, uint32_t*),
        int (*writefn)(uint32_t, uint32_t, uint32_t));

/* read / write a 32bit aligned word */
int read_word(uint32_t address, uint32_t* value);
int write_word(uint32_t address, uint32_t value, uint32_t mask);

/* Returns a pointer to a block of contiguous memory
 * Can access RDRAM, SP_DMEM, SP_IMEM and ROM, using TLB if necessary
 * Useful for getting fast access to a zone with executable code. */
unsigned int *fast_mem_access(unsigned int address);


int read_rdram(uint32_t address, uint32_t* value);
int write_rdram(uint32_t address, uint32_t value, uint32_t mask);
int read_rdramFB(uint32_t address, uint32_t* value);
int write_rdramFB(uint32_t address, uint32_t value, uint32_t mask);



static inline void masked_write(uint32_t* dst, uint32_t value, uint32_t mask)
{
    *dst = (mask & value) | (~mask & *dst);
}

#endif

