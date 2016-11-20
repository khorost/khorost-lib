
#if defined(_WIN32) || defined(_WIN64)
 #include <Windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <iostream>

#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>

#include "util/logger.h"

#pragma comment(lib,"dbghelp.lib")

#if defined(_WIN32) || defined(_WIN64)

struct ColorCoutSinkWin32 {
    bool textcolorprotect = true;
    /*doesn't let textcolor be the same as backgroung color if true*/
    enum concol {
        black = 0,
        dark_blue = 1,
        dark_green = 2,
        dark_aqua, dark_cyan = 3,
        dark_red = 4,
        dark_purple = 5, dark_pink = 5, dark_magenta = 5,
        dark_yellow = 6,
        dark_white = 7,
        gray = 8,
        blue = 9,
        green = 10,
        aqua = 11, cyan = 11,
        red = 12,
        purple = 13, pink = 13, magenta = 13,
        yellow = 14,
        white = 15
    };

    int textcolor() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int a = csbi.wAttributes;
        return a % 16;
    }
    int backcolor() {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        int a = csbi.wAttributes;
        return (a / 16) % 16;
    }

    concol GetColor(const LEVELS level, concol defaultColor) const {
        if (level.value == WARNING.value) { return yellow; }
        if (level.value == DEBUG.value) { return green; }
        if (g3::internal::wasFatal(level)) { return red; }

        return defaultColor;
    }

    inline void setcolor(concol textcol, concol backcol) {
        setcolor(int(textcol), int(backcol));
    }

    inline void setcolor(int textcol, int backcol) {
        if (textcolorprotect) {
            if ((textcol % 16) == (backcol % 16))textcol++;
        }
        textcol %= 16; backcol %= 16;
        unsigned short wAttributes = ((unsigned)backcol << 4) | (unsigned)textcol;
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wAttributes);
    }

    void ReceiveLogMessage(g3::LogMessageMover logEntry) {
        auto level = logEntry.get()._level;
        auto defaultColor = concol(textcolor());
        auto color = GetColor(level, defaultColor);
        
        if (color != defaultColor) {
            setcolor(color, backcolor());
        }
        std::cout << logEntry.get().toString() /*<< std::endl*/;
        if (color != defaultColor) {
            setcolor(defaultColor, backcolor());
        }
    }
};

#else
struct ColorCoutSink {
    // Linux xterm color
    // http://stackoverflow.com/questions/2616906/how-do-i-output-coloured-text-to-a-linux-terminal
    enum FG_Color { YELLOW = 33, RED = 31, GREEN = 32, WHITE = 97 };

    FG_Color GetColor(const LEVELS level) const {
        if (level.value == WARNING.value) { return YELLOW; }
        if (level.value == DEBUG.value) { return GREEN; }
        if (g3::internal::wasFatal(level)) { return RED; }

        return WHITE;
    }

    void ReceiveLogMessage(g3::LogMessageMover logEntry) {
        auto level = logEntry.get()._level;
        auto color = GetColor(level);

        std::cout << "\033[" << color << "m"
            << logEntry.get().toString() << "\033[m" << std::endl;
    }
    };
#endif // WIN

std::unique_ptr<g3::LogWorker>  g_Logger;

void khorost::log::prepare(const std::string& sFolder_, const std::string& sPrefix_, const std::string& sID_) {
    if (g_Logger.get() == NULL) {
        g_Logger = g3::LogWorker::createLogWorker();
        g3::initializeLogging(g_Logger.get());

#if defined(_WIN32) || defined(_WIN64)
        g_Logger->addSink(std::make_unique<ColorCoutSinkWin32>(), &ColorCoutSinkWin32::ReceiveLogMessage);
#else
        g_Logger->addSink(std::make_unique<ColorCoutSink>(), &ColorCoutSink::ReceiveLogMessage);
#endif  // WIN
    }

    if (!sFolder_.empty()) {
        g_Logger->addDefaultLogger(sPrefix_, sFolder_, sID_);
    }
}

