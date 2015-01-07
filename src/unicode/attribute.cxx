
#include "unicode/attribute.h"

#include "iswbits.h"
#include "attribute.h"

int Unicode::iswalnum (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_alnum)) != 0
            && (attributes & wmask_excl(wctype_alnum)) == 0);
    }
    return 0;
}

int Unicode::iswalpha (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_alpha)) != 0
            && (attributes & wmask_excl(wctype_alpha)) == 0);
    }
    return 0;
}

int Unicode::iswblank (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_blank)) != 0
            && (attributes & wmask_excl(wctype_blank)) == 0);
    }
    return 0;
}

int Unicode::iswcntrl (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_cntrl)) != 0
            && (attributes & wmask_excl(wctype_cntrl)) == 0);
    }
    return 0;
}

int Unicode::iswdigit (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_digit)) != 0
            && (attributes & wmask_excl(wctype_digit)) == 0);
    }
    return 0;
}

int Unicode::iswgraph (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_graph)) != 0
            && (attributes & wmask_excl(wctype_graph)) == 0);
    }
    return 0;
}

int Unicode::iswlower (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_lower)) != 0
            && (attributes & wmask_excl(wctype_lower)) == 0);
    }
    return 0;
}

int Unicode::iswprint (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_print)) != 0
            && (attributes & wmask_excl(wctype_print)) == 0);
    }
    return 0;
}

int Unicode::iswpunct (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_punct)) != 0
            && (attributes & wmask_excl(wctype_punct)) == 0);
    }
    return 0;
}

int Unicode::iswspace (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_space)) != 0
            && (attributes & wmask_excl(wctype_space)) == 0);
    }
    return 0;
}

int Unicode::iswupper (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_upper)) != 0
            && (attributes & wmask_excl(wctype_upper)) == 0);
    }
    return 0;
}

int Unicode::iswxdigit (wchar_t wc) {
    if ((unsigned int) wc <= 0xffffu) {
        unsigned char attributes = attribute_table[((unsigned int) wc) >> 8][((unsigned int) wc) & 0xff];
        return ((attributes & wmask_incl(wctype_xdigit)) != 0
            && (attributes & wmask_excl(wctype_xdigit)) == 0);
    }
    return 0;
}
