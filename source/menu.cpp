/* Code borrowed from https://github.com/mid-kid/CakesForeveryWan/blob/master/source/menu.c and tortured until it bent to my will */
#include "menu.h"

#include <string>
#include <vector>
#include <stdio.h>

#include "common.h"
#include "display.h"

void init_menu(gfxScreen_t screen)
{
    // Create our new console, initialize it, and switch back to the previous console
    currentMenu.menuConsole = *consoleGetDefault();
    PrintConsole *currentConsole = consoleSelect(&currentMenu.menuConsole);
    consoleInit(screen, &currentMenu.menuConsole);

    consoleSelect(currentConsole);
}

void menu_draw_string(const char *str, int pos_x, int pos_y, const char *color)
{
    currentMenu.menuConsole.cursorX = pos_x;
    currentMenu.menuConsole.cursorY = pos_y;

    gfxFlushBuffers();
}

void ui_menu_draw_string(const char *str, int pos_x, int pos_y, float fontSize, u32 color)
{
    setTextColor(color); // black
    renderText(pos_x, pos_y, fontSize, fontSize, false, str);
}

void menu_draw_string_full(const char *str, int pos_y, const char *color)
{
    currentMenu.menuConsole.cursorX = 0;
    currentMenu.menuConsole.cursorY = pos_y;
    printf(color);

    if (currentMenu.menuConsole.consoleWidth == 50) {
        printf("%-50s", str);
    } else {
        printf("%-40s", str);
    }
    printf(CONSOLE_RESET);

    gfxFlushBuffers();
}

void menu_draw_info(PrintConsole &console, const game_item &game)
{
    PrintConsole* currentConsole = consoleSelect(&console);
    consoleClear();

    printf("Name:    %s\n", game.name.c_str());
    printf("Serial:  %s\n", game.code.c_str());
    printf("Region:  %s\n", game.region.c_str());
    printf("TitleID: %s\n", game.titleid.c_str());
    printf("Type:    %s\n", GetSerialType(game.code).c_str());
    if(bSvcHaxAvailable) {
        printf("Title/Ticket Installed: ");
        if (game.installed) {
            printf("yes\n");
        } else {
            printf("no\n");
        }
    }
    consoleSelect(currentConsole);
}

void titles_multkey_draw(const char *title, const char* footer, int back, std::vector<game_item> *options, void* data,
                      bool (*callback)(int result, u32 key, void* data))

{
    // Set up a console on the bottom screen for info
    GSPGPU_FramebufferFormats infoOldFormat = gfxGetScreenFormat(GFX_BOTTOM);
    PrintConsole infoConsole;
    PrintConsole* currentConsole = consoleSelect(&infoConsole);
    consoleInit(GFX_BOTTOM, &infoConsole);

    // Select our menu console and clear the screen
    consoleSelect(&currentMenu.menuConsole);
    
    int count = options->size();
    int current = 0;
    bool firstLoop = true;
    int previous_index = 0;
    int menu_offset = 0;
    int menu_pos_y;
    int menu_end_y = 19;
    int current_pos_y = 0;
    float menuFontSize = 0.5f;
    int text_offset_x = 20;
    int menuLineHeight = (menuFontSize * fontGetInfo()->lineFeed);
    int results_per_page = (TOP_SCREEN_HEIGHT - 18) / (menuLineHeight);
    //    screen_get_texture_size(&topScreenBgWidth, &topScreenBgHeight, TEXTURE_SCREEN_TOP_SPLASH_BG);

    while (!bExit && aptMainLoop()) {
        if (firstLoop || previous_index != current) {
            gspWaitForVBlank();
            screen_begin_frame();
            firstLoop = false;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            current_pos_y = 0;
            // Draw the header
            ui_menu_draw_string(title, 0, 0, 0.7f, COLOR_TITLE);
            menu_pos_y = current_pos_y;
            if(count > 0) {
                for (int i = 0; menu_offset + i < count && i < results_per_page; i++) {
                    int y_pos = 18 + (i * menuLineHeight);
                    u64 color = COLOR_MENU_ITEM;
                    if (i + menu_offset == current) {
                        color = COLOR_SELECTED;
                    } else if ((*options)[i + menu_offset].installed) {
                        color = COLOR_MENU_INSTALLED;
                    }

                    u32 flagWidth = 0;
                    u32 flagHeight = 0;
                    int flagType;
                    std::string region = (*options)[menu_offset + i].region;
                    flagType = TEXTURE_FLAG_ALL;
                    if (region == "EUR") {
                        flagType = TEXTURE_FLAG_EUR;
                    } else if (region == "USA") {
                        flagType = TEXTURE_FLAG_USA;
                    } else if (region == "JPN") {
                        flagType = TEXTURE_FLAG_JPN;
                    }
                    screen_get_texture_size(&flagWidth, &flagHeight, flagType);
                    //                ui_menu_draw_string( region.c_str(), 1, y_pos, menuFontSize, color);
                    screen_draw_texture(flagType, 0, y_pos, flagWidth, flagHeight);
                    ui_menu_draw_string((*options)[menu_offset + i].name.c_str(), text_offset_x, y_pos, menuFontSize, color);
                    current_pos_y++;
                }
                consoleClear();
            }
            if (footer != NULL) {
                // Draw the footer if one is provided
                current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
            }
            previous_index = current;
            screen_end_frame();
            menu_draw_info(infoConsole, (*options)[current]);
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
            if (current >= count)
                current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0)
                current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);

    // Reset the gfx format on the bottom screen
    gfxSetScreenFormat(GFX_BOTTOM, infoOldFormat);
}

void menu_multkey_draw(const char *title, const char *footer, int back, int count, const char *options[], void *data,
                       bool (*callback)(int result, u32 key, void *data))

{
    // Select our menu console and clear the screen
    PrintConsole *currentConsole = consoleSelect(&currentMenu.menuConsole);

    int current = 0;
    bool firstLoop = true;
    int previous_index = 0;
    int menu_offset = 0;
    int menu_end_y = 18;
    int current_pos_y = 0;
    float menuFontSize = 0.5f;
    int menuLineHeight = (menuFontSize * fontGetInfo()->lineFeed);
    int results_per_page = (TOP_SCREEN_HEIGHT - 32) / (menuLineHeight);

    while (!bExit && aptMainLoop()) {
        std::string mode_text;
        switch (config.GetMode())
        {
            case CConfig::Mode::DOWNLOAD_CIA:
                mode_text = "Create CIA";
            break;
            case CConfig::Mode::INSTALL_CIA:
                mode_text = "Install CIA";
            break;
            case CConfig::Mode::INSTALL_TICKET:
                mode_text = "Create Ticket";
            break;
        }

        if (firstLoop || previous_index != current) {
            screen_begin_frame();
            firstLoop = false;
            int current_page = current / results_per_page;
            menu_offset = current_page * results_per_page;
            current_pos_y = 0;
            consoleClear();
            // Draw the header
            ui_menu_draw_string(title, 0, 0, 0.7f, COLOR_TITLE);
            for (int i = 0; (menu_offset + i) < count && i < results_per_page; i++) {
                int y_pos = 32 + (i * menuLineHeight);
                u64 color = COLOR_MENU_ITEM;
                if (i + menu_offset == current)
                    color = COLOR_SELECTED;
                ui_menu_draw_string(options[menu_offset + i], 15, y_pos, menuFontSize, color);
                current_pos_y++;
            }
            setTextColor(COLOR_FOOTER);
            renderText(0, 220, 0.7f, 0.7f, false, "Mode:");
            renderText(180, 220, 0.7f, 0.7f, false, "Region:");
            renderText(300, 220, 0.7f, 0.7f, false, "Queue:");
            setTextColor(COLOR_FOOTER_SELECTED);
            renderText(60, 220, 0.7f, 0.7f, false, mode_text.c_str());
            renderText(250, 220, 0.7f, 0.7f, false, config.GetRegionFilter().c_str());
            char qSize[5];
            sprintf(qSize, "%d", game_queue.size());
            renderText(363, 220, 0.7f, 0.7f, false, qSize);
            if (footer != NULL) {
                // Draw the footer if one is provided
                current_pos_y = currentMenu.menuConsole.consoleHeight - 1;
                menu_draw_string_full(footer, current_pos_y, CONSOLE_BLUE CONSOLE_REVERSE);
            }
            previous_index = current;
            screen_end_frame();
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
            if (current >= count)
                current = count - 1;
        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0)
                current = 0;
        } else if (callback(current, key, data)) {
            break;
        }
    }

    // Reselect the original console
    consoleSelect(currentConsole);
}
