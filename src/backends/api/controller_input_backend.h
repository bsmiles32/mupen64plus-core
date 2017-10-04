/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - controller_input_backend.h                              *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2016 Bobby Smiles                                       *
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

#ifndef M64P_BACKENDS_API_CONTROLLER_INPUT_BACKEND_H
#define M64P_BACKENDS_API_CONTROLLER_INPUT_BACKEND_H

#include "api/m64p_types.h"

#include <stdint.h>

enum standard_controller_input {
    CI_STD_R_DPAD = 0x0001,
    CI_STD_L_DPAD = 0x0002,
    CI_STD_D_DPAD = 0x0004,
    CI_STD_U_DPAD = 0x0008,
    CI_STD_START  = 0x0010,
    CI_STD_Z      = 0x0020,
    CI_STD_B      = 0x0040,
    CI_STD_A      = 0x0080,
    CI_STD_R_CBTN = 0x0100,
    CI_STD_L_CBTN = 0x0200,
    CI_STD_D_CBTN = 0x0400,
    CI_STD_U_CBTN = 0x0800,
    CI_STD_R      = 0x1000,
    CI_STD_L      = 0x2000,
    /* bits 14 and 15 are reserved */
    /* bits 23-16 are for X-axis */
    /* bits 31-24 are for Y-axis */
};

enum mouse_controller_input {
    CI_MOUSE_RIGHT = 0x0040,
    CI_MOUSE_LEFT  = 0x0080,
    /* bits 23-16 are for X-axis */
    /* bits 31-24 are for Y-axis */
};

enum train_controller_input {

    /* brake states */
    CI_TRAIN_BRAKE_0      = 0x00000000,
    CI_TRAIN_BRAKE_1      = 0x00010000,
    CI_TRAIN_BRAKE_2      = 0x00020000,
    CI_TRAIN_BRAKE_3      = 0x00030000,
    CI_TRAIN_BRAKE_4      = 0x00040000,
    CI_TRAIN_BRAKE_5      = 0x00050000,
    CI_TRAIN_BRAKE_6      = 0x00060000,
    CI_TRAIN_BRAKE_7      = 0x00070000,
    CI_TRAIN_BRAKE_8      = 0x00080000,
    CI_TRAIN_BRAKE_9      = 0x00090000,
    CI_TRAIN_BRAKE_A      = 0x000a0000,
    CI_TRAIN_BRAKE_B      = 0x000b0000,
    CI_TRAIN_BRAKE_C      = 0x000c0000,
    CI_TRAIN_BRAKE_D      = 0x000d0000,
    CI_TRAIN_BRAKE_E      = 0x000e0000,
    CI_TRAIN_BRAKE_UNK    = 0x000f0000, /* for state between values */

    /* throttle */
    CI_TRAIN_THROTTLE_5   = 0x01000000,
    CI_TRAIN_THROTTLE_4   = 0x08000000,
    CI_TRAIN_THROTTLE_3   = 0x09000000,
    CI_TRAIN_THROTTLE_2   = 0x20000000,
    CI_TRAIN_THROTTLE_1   = 0x21000000,
    CI_TRAIN_THROTTLE_OFF = 0x28000000,

    /* buttons */
    CI_TRAIN_SELECT       = 0x00100000,
    CI_TRAIN_C            = 0x00200000,
    CI_TRAIN_START        = 0x10000000,
    CI_TRAIN_A            = 0x40000000,
    CI_TRAIN_B            = 0x80000000,
};


struct controller_input_backend_interface
{
    /* Get emulated controller input status (32-bit)
     * Encoding of the input status depends on the emulated controller flavor.
     * Returns M64ERR_SUCCESS on success
     */
    m64p_error (*get_input)(void* cin, uint32_t* input);
};

#endif
