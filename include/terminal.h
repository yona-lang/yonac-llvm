#pragma once

// Reset
#define ANSI_COLOR_RESET "\033[0m"

// Regular Colors
#define ANSI_COLOR_BLACK "\033[0;30m"
#define ANSI_COLOR_RED "\033[0;31m"
#define ANSI_COLOR_GREEN "\033[0;32m"
#define ANSI_COLOR_YELLOW "\033[0;33m"
#define ANSI_COLOR_BLUE "\033[0;34m"
#define ANSI_COLOR_MAGENTA "\033[0;35m"
#define ANSI_COLOR_CYAN "\033[0;36m"
#define ANSI_COLOR_WHITE "\033[0;37m"

// Bold Colors
#define ANSI_COLOR_BOLD_BLACK "\033[1;30m"
#define ANSI_COLOR_BOLD_RED "\033[1;31m"
#define ANSI_COLOR_BOLD_GREEN "\033[1;32m"
#define ANSI_COLOR_BOLD_YELLOW "\033[1;33m"
#define ANSI_COLOR_BOLD_BLUE "\033[1;34m"
#define ANSI_COLOR_BOLD_MAGENTA "\033[1;35m"
#define ANSI_COLOR_BOLD_CYAN "\033[1;36m"
#define ANSI_COLOR_BOLD_WHITE "\033[1;37m"

#define FULL_BLOCK L'\u2588'

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#else
#include <sys/ioctl.h>
#endif

inline std::pair<size_t, size_t> get_terminal_size() {
#if defined(_WIN32)
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return std::make_pair((size_t)(csbi.srWindow.Right - csbi.srWindow.Left + 1), (size_t)(csbi.srWindow.Bottom - csbi.srWindow.Top + 1));
#else
  winsize w{};
  ioctl(fileno(stdout), TIOCGWINSZ, &w);
  return std::make_pair(w.ws_col, w.ws_row);
#endif
}
