
#include "unicode/convert.h"

#pragma warning(disable:4309)


#include "noppage.h"
#include "toupper.h"
#include "tolower.h"

wchar_t Unicode::towupper (wchar_t wc) {
    if ((unsigned int) wc < 0x10800u)
        return (unsigned short) (wc + toupper_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff]);
    else
        return wc;
}

wchar_t Unicode::towlower (wchar_t wc) {
    if ((unsigned int) wc < 0x10800u)
        return (unsigned short) (wc + tolower_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff]);
    else
        return wc;
}
