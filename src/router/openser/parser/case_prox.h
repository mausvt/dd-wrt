/* 
 * $Id: case_prox.h,v 1.2 2005/06/16 11:37:54 miconda Exp $ 
 *
 * Proxy-Require, Proxy-Authorization Header Field Name Parsing Macros
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27 next baby-step to removing ZT - PRESERVE_ZT (jiri)
 */


#ifndef CASE_PROX_H
#define CASE_PROX_H


#define ION_CASE                           \
        switch(LOWER_DWORD(val)) {         \
        case _ion1_:                       \
	        hdr->type = HDR_PROXYAUTH_T; \
	        hdr->name.len = 19;        \
	        return (p + 4);            \
                                           \
        case _ion2_:                       \
                hdr->type = HDR_PROXYAUTH_T; \
                p += 4;                    \
	        goto dc_end;               \
        }


#define IZAT_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _izat_:               \
                p += 4;            \
                val = READ(p);     \
                ION_CASE;          \
                goto other;        \
        }


#define THOR_CASE                  \
        switch(LOWER_DWORD(val)) { \
        case _thor_:               \
                p += 4;            \
                val = READ(p);     \
                IZAT_CASE;         \
                goto other;        \
        }


#define QUIR_CASE                                     \
        switch(LOWER_DWORD(val)) {                    \
        case _quir_:                                  \
	        p += 4;                               \
                switch(LOWER_BYTE(*p)) {              \
                case 'e':                             \
                        hdr->type = HDR_PROXYREQUIRE_T; \
	                p++;                          \
                        goto dc_end;                  \
                }                                     \
                goto other;                           \
        }


#define PROX2_CASE                 \
        switch(LOWER_DWORD(val)) { \
        case _y_au_:               \
                p += 4;            \
                val = READ(p);     \
                THOR_CASE;         \
                goto other;        \
                                   \
        case _y_re_:               \
                p += 4;            \
                val = READ(p);     \
                QUIR_CASE;         \
                goto other;        \
        }


#define prox_CASE         \
        p += 4;           \
        val = READ(p);    \
        PROX2_CASE;       \
         goto other;


#endif /* CASE_PROX_H */
