//==========================================================================
//
//      swab.c
//
//      RedBoot memory byte-swapping (swab) routine
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 2005 Red Hat, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msalter
// Contributors: msalter
// Date:         2005-02-14
// Purpose:      
// Description:  
//              
// This code is part of RedBoot (tm).
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <redboot.h>

static void
do_swab(int argc, char *argv[])
{
    // Fill a region of memory with a pattern
    struct option_info opts[4];
    unsigned long base;
    long len;
    bool base_set, len_set;
    bool set_32bit, set_16bit;

    init_opts(&opts[0], 'b', true, OPTION_ARG_TYPE_NUM, 
              (void *)&base, (bool *)&base_set, "base address");
    init_opts(&opts[1], 'l', true, OPTION_ARG_TYPE_NUM, 
              (void *)&len, (bool *)&len_set, "length");
    init_opts(&opts[2], '4', false, OPTION_ARG_TYPE_FLG,
              (void *)&set_32bit, (bool *)0, "fill 32 bit units");
    init_opts(&opts[3], '2', false, OPTION_ARG_TYPE_FLG,
              (void *)&set_16bit, (bool *)0, "fill 16 bit units");
    if (!scan_opts(argc, argv, 1, opts, 4, 0, 0, "")) {
        return;
    }
    if (!base_set || !len_set) {
        diag_printf("usage: swab -b <addr> -l <length> [-2|-4]\n");
        return;
    }
    if (set_16bit) {
        // 16 bits at a time
        while ((len -= sizeof(cyg_uint16)) >= 0) {
	    *(cyg_uint16 *)base = CYG_SWAP16(*(cyg_uint16 *)base);
	    base += sizeof(cyg_uint16);
	}
    } else {
        // Default - 32 bits
        while ((len -= sizeof(cyg_uint32)) >= 0) {
	    *(cyg_uint32 *)base = CYG_SWAP32(*(cyg_uint32 *)base);
	    base += sizeof(cyg_uint32);
	}
    }
}

RedBoot_cmd("swab", 
            "Swap bytes in 16-bit or 32-bit words in a block of memory",
            "-b <location> -l <length> [-2|-4]",
            do_swab
    );

