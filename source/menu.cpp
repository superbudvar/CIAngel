/* Code borrowed from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/menu.c and tortured until it bent to my will */
#include "menu.h"

#include <string>
#include <vector>
#include <stdio.h>

#include "common.h"
#include "display.h"

int selected_options[MAX_SELECTED_OPTIONS];

void init_menu(gfxScreen_t screen)
{
    // Create our new console, initialize it, and switch back to the previous console
    currentMenu.menuConsole = *consoleGetDefault();
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    consoleInit(screen, &currentMenu.menuConsole);

    consoleSelect(currentConsole);
}


void menu_draw_string(const char* str, int pos_x, int pos_y, const char* color)
{
    currentMenu.menuConsole.cursorX = pos_x;
    currentMenu.menuConsole.cursorY = pos_y;
    
    gfxFlushBuffers();
}

void ui_menu_draw_string(const char* str, int pos_x, int pos_y, u32 color)
{
    setTextColor(color); // black
    renderText(pos_x, pos_y, FONT_DEFAULT_SIZE, FONT_DEFAULT_SIZE, false, str);
}

void ui_menu_draw(const char *title, const char* footer, int back, int count, const char *options[]) {
    setTextColor(COLOR_GREEN);
    renderText(0, TOP_SCREEN_HEIGHT-20, 0.7f, 0.7f, false, title);
    for (int i = 0; i < count && i < (currentMenu.menuConsole.consoleHeight - 2); i++) {
        ui_menu_draw_string(options[i], 1, 32+(i*12), 0xFF000000);
        //ui_menu_draw_string(options[i], 1, 32+(i*12), i==current? 0xFF0000FF: 0xFF000000);
    }
    if (footer != NULL)
    {
        sceneRenderFooter(footer);
    }
}

void menu_draw_string_full(const char* str, int pos_y, const char* color)
{
    currentMenu.menuConsole.cursorX = 0;
    currentMenu.menuConsole.cursorY = pos_y;
    printf(color);

    if (currentMenu.menuConsole.consoleWidth == 50)
    {
        printf("%-50s", str);
    }
    else
    {
        printf("%-40s", str);
    }
    printf(CONSOLE_RESET);

    gfxFlushBuffers();
}

void titles_multkey_draw(const char *title, const char* footer, int back, std::vector<game_item> *options, void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);
    
    int count = options->size();
    bool firstLoop = true;
    int current = 0;
    int previous_index = 0;
    int menu_offset = 0;
    int menu_pos_y;
    int menu_end_y = 19;
    int current_pos_y = 0;

    while (!bExit && aptMainLoop()) {
        if(firstLoop || previous_index != current) {
            firstLoop = false;
            int results_per_page = menu_end_y - menu_pos_y;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            current_pos_y=0;
            // Draw the header
            setTextColor(COLOR_TITLE);
            renderText(0, 0, 0.7f, 0.7f, false, title);
            menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);
            menu_pos_y = current_pos_y;
            for (int i = 0; menu_offset + i < count && i < results_per_page; i++) {
                int y_pos = 18+(i*12);
                u64 color = COLOR_MENU_ITEM;
                if(i+menu_offset == current) {
                    color = COLOR_SELECTED;
                } else if((*options)[i+menu_offset].installed) {
                    color = COLOR_MENU_INSTALLED;
                }
                ui_menu_draw_string((*options)[menu_offset+i].region.c_str(), 1, y_pos, color);
                ui_menu_draw_string((*options)[menu_offset+i].name.c_str(), 32, y_pos, color);
                current_pos_y++;
            }
            consoleClear();
            printf("Title: %s\nRegion: %s\ncode: %s\nTitle/Ticket Installed: ",
                        (*options)[current].norm_name.c_str(),
                        (*options)[current].region.c_str(),
                        (*options)[current].code.c_str());
            if((*options)[current].installed) {
                printf("yes\n");
            } else {
                printf("no\n");
            }
            if (footer != NULL)
            {
                // Draw the footer if one is provided
                current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
            }
            previous_index = current;
            sceneDraw();
        }
        u32 key = wait_key();
        
        if (key & KEY_UP) {
            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= count) current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}

void menu_multkey_draw(const char *title, const char* footer, int back, int count, const char *options[], void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Select our menu console and clear the screen
    PrintConsole* currentConsole = consoleSelect(&currentMenu.menuConsole);

    int current = 0;
    bool firstLoop = true;
    int previous_index = 0;
    int menu_offset = 0;
    int menu_pos_y;
    int menu_end_y = 18; 
    int current_pos_y = 0;

    while (!bExit && aptMainLoop()) {
        std::string mode_text;
        if(selected_mode == make_cia) {
            mode_text = "Create CIA";
        }
        else if (selected_mode == install_direct) {
            mode_text = "Install CIA";
        }
        else if (selected_mode == install_ticket) {
            mode_text = "Create Ticket";
        }

        if(firstLoop || previous_index != current) {
            firstLoop = false;
            int results_per_page = menu_end_y - menu_pos_y;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            current_pos_y=0;
            consoleClear();
            // Draw the header
            setTextColor(COLOR_TITLE);
            renderText(0, 8, 0.7f, 0.7f, false, title);
            menu_draw_string(title, 0, current_pos_y++, CONSOLE_RED);
            menu_pos_y = current_pos_y;
            for (int i = 0; (menu_offset + i) < count && i < results_per_page; i++) {
                int y_pos = 32+(i*12);
                u64 color = COLOR_MENU_ITEM;
                if(i+menu_offset == current) color = COLOR_SELECTED;
                ui_menu_draw_string(options[menu_offset+i], 1, y_pos, color);
                current_pos_y++;
            }
            setTextColor(COLOR_FOOTER);
            renderText(0, 220, 0.7f, 0.7f, false, "Mode:");
            renderText(165, 220, 0.7f, 0.7f, false, "Region:");
            renderText(300, 220, 0.7f, 0.7f, false, "Queue:");
            setTextColor(COLOR_FOOTER_SELECTED);
            renderText(60, 220, 0.7f, 0.7f, false, mode_text.c_str());
            renderText(235, 220, 0.7f, 0.7f, false, regionFilter.c_str());
            int qSize = game_queue.size();
            char qsOut[5];
            itoa(qSize, qsOut, 4);
            renderText(363, 220, 0.7f, 0.7f, false, qsOut);
            if (footer != NULL)
            {
                // Draw the footer if one is provided
                current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
            }
            previous_index = current;
            sceneDraw();
        }
        
        u32 key = wait_key();
        
        if (key & KEY_UP) {
            if (current <= 0) {
                current = count - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= count - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= count) current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}
