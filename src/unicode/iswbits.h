/* Copyright (C) 1999 Free Software Foundation, Inc.
   This file is part of the GNU UTF-8 Library.

   The GNU UTF-8 Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU UTF-8 Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU UTF-8 Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* How the 12 character attributes are encoded in 8 bits: Every attribute is
   represented by an "include bitmask" and an "exclude bitmask".
    Attribute     bit/formula          comment
     upper        bit               implies towlower(x) != x == towupper(x)
     lower        bit               implies towlower(x) == x != towupper(x)
     alpha        bit               superset of upper || lower
     digit        xdigit && !alpha  '0'..'9' and more
     xdigit       bit               '0'..'9','a'..'f','A'..'F' and more
     space        bit               ' ', '\f', '\n', '\r', '\t', '\v'
     print        bit
     graph        print && !space
     blank        bit               ' ', '\t'
     cntrl        bit               0x00..0x1F,0x7F
     punct        print && !(alpha || xdigit || space)
     alnum        alpha || xdigit
*/

#define iswmask(number,incl,excl)  ((incl) | ((excl) << 8) | ((number) << 16))
#define wmask_incl(mask)  (mask) & 0xFF
#define wmask_excl(mask)  ((mask) >> 8) & 0xFF
#define wmask_number(mask)  ((mask) >> 16)

#define upper    1
#define lower    2
#define alpha    4
#define digit    0
#define xdigit   8
#define space   16
#define print   32
#define graph    0
#define blank   64
#define cntrl  128
#define punct    0
#define alnum    0

#define wctype_upper  iswmask(0, upper,0)
#define wctype_lower  iswmask(1, lower,0)
#define wctype_alpha  iswmask(2, alpha,0)
#define wctype_digit  iswmask(3, xdigit,alpha)
#define wctype_xdigit iswmask(4, xdigit,0)
#define wctype_space  iswmask(5, space,0)
#define wctype_print  iswmask(6, print,0)
#define wctype_graph  iswmask(7, print,space)
#define wctype_blank  iswmask(8, blank,0)
#define wctype_cntrl  iswmask(9, cntrl,0)
#define wctype_punct  iswmask(10, print,alpha|xdigit|space)
#define wctype_alnum  iswmask(11, alpha|xdigit,0)

extern const unsigned char * const attribute_table[0x1100];
