#ifndef _ATTRIBUTE__H_
#define _ATTRIBUTE__H_

namespace khorost {
    namespace Unicode {
        int iswalnum(wchar_t wc);
        int iswalpha(wchar_t wc);
        int iswblank(wchar_t wc);
        int iswcntrl(wchar_t wc);
        int iswdigit(wchar_t wc);
        int iswgraph(wchar_t wc);
        int iswlower(wchar_t wc);
        int iswprint(wchar_t wc);
        int iswpunct(wchar_t wc);
        int iswspace(wchar_t wc);
        int iswupper(wchar_t wc);
        int iswxdigit(wchar_t wc);
    }
}

#endif  // _ATTRIBUTE__H_
