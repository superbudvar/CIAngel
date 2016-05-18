#pragma once

#ifndef __MENU_H_INCLUDED__
#define __MENU_H_INCLUDED__
#include <3ds.h>
#include <citro3d.h>
#include <string>
#include <vector>
#include <stdio.h>
#include "common.h"
#include "DownloadQueue.h"

#include <stdint.h>
#include <stddef.h>

#define MAX_SELECTED_OPTIONS 0x10

#define COLOR_TITLE COLOR_RED
#define COLOR_NEUTRAL COLOR_WHITE
#define COLOR_MENU_ITEM COLOR_WHITE
#define COLOR_MENU_INSTALLED 0xFFff0080
#define COLOR_SELECTED COLOR_GREEN
#define COLOR_FOOTER COLOR_PURPLE
#define COLOR_FOOTER_SELECTED COLOR_CYAN
#define COLOR_BACKGROUND COLOR_BLACK

#define CONSOLE_REVERSE CONSOLE_ESC(7m)

void init_menu(gfxScreen_t screen);
void ui_menu_draw_string(const char *str, int pos_x, int pos_y, float fontSize, u32 color);
void menu_draw_string(const char *str, int pos_x, int pos_y, const char *color);
void menu_draw_string_full(const char *str, int pos_y, const char *color);
void titles_multkey_draw(const char *title, const char *footer, int back, std::vector<game_item> *options, void *data,
                         bool (*callback)(int result, u32 key, void *data));
void menu_multkey_draw(const char *title, const char *footer, int back, int count, const char *options[], void *data,
                       bool (*callback)(int result, u32 key, void *data));
#endif // __MENU_H_INCLUDED__
