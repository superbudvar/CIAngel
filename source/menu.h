#pragma once

#ifndef __MENU_H_INCLUDED__
#define __MENU_H_INCLUDED__
#include <3ds.h>
#include <string>
#include <vector>
#include <stdio.h>
#include "common.h"

#include <stdint.h>
#include <stddef.h>

#define MAX_SELECTED_OPTIONS 0x10

#define COLOR_TITLE 0xFF0000ff
#define COLOR_NEUTRAL 0xFFffffff
#define COLOR_MENU_ITEM 0xFFffffff
#define COLOR_SELECTED 0xFF00ff00
#define COLOR_FOOTER 0xFFd126b8
#define COLOR_FOOTER_SELECTED 0xFFd19426
#define COLOR_BACKGROUND 0xFF000000

#define CONSOLE_REVERSE		CONSOLE_ESC(7m)

typedef struct {
  int score;
  int index;
  std::string titleid;
  std::string titlekey;
  std::string name;
  std::string norm_name;
  std::string region;
  std::string code;
} game_item;

extern std::vector<game_item> game_queue;

void init_menu(gfxScreen_t screen);
void ui_menu_draw_string(const char* str, int pos_x, int pos_y, u32 color);
void menu_draw_string(const char* str, int pos_x, int pos_y, const char* color);
void menu_draw_string_full(const char* str, int pos_y, const char* color);
void titles_multkey_draw(const char *title, const char* footer, int back, std::vector<game_item> *options, void* data,
                         bool (*callback)(int result, u32 key, void* data));
void menu_multkey_draw(const char *title, const char* footer, int back, int count, const char *options[], void* data,
                       bool (*callback)(int result, u32 key, void* data));

#endif // __MENU_H_INCLUDED__
