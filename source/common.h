#pragma once
#include <3ds.h>
#include <string>
typedef struct ConsoleMenu {
	PrintConsole menuConsole;
} ConsoleMenu;

extern ConsoleMenu currentMenu;
enum install_modes {
    make_cia, install_direct, install_ticket
};

extern install_modes selected_mode;
extern std::string regionFilter;
extern bool bExit;
