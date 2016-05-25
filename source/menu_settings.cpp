#include "menu_settings.h"

void action_toggle_install()
{
    consoleClear();
    CConfig::Mode nextMode = CConfig::Mode::INSTALL_CIA;

    switch (config.GetMode()) {
    case CConfig::Mode::DOWNLOAD_CIA:
        nextMode = CConfig::Mode::INSTALL_CIA;
        break;
    case CConfig::Mode::INSTALL_CIA:
        nextMode = CConfig::Mode::INSTALL_TICKET;
        break;
    case CConfig::Mode::INSTALL_TICKET:
        nextMode = CConfig::Mode::DOWNLOAD_CIA;
        break;
    }

    if (nextMode == CConfig::Mode::INSTALL_TICKET || nextMode == CConfig::Mode::INSTALL_CIA) {
        if (!bSvcHaxAvailable) {
            nextMode = CConfig::Mode::DOWNLOAD_CIA;
            screen_begin_frame(true);
            setTextColor(COLOR_RED);
            renderText(0, 0, 1.0f, 1.0f, false,
                       "Kernel access not available.\nCan't enable Install modes.\nYou can only make a CIA.\n");
            screen_end_frame();
            wait_key_specific("\nPress A to continue.", KEY_A);
        }
    }

    config.SetMode(nextMode);
}

void action_toggle_region()
{
    consoleClear();
    std::string regionFilter = config.GetRegionFilter();
    if (regionFilter == "off") {
        regionFilter = "ALL";
    } else if (regionFilter == "ALL") {
        regionFilter = "EUR";
    } else if (regionFilter == "EUR") {
        regionFilter = "USA";
    } else if (regionFilter == "USA") {
        regionFilter = "JPN";
    } else if (regionFilter == "JPN") {
        regionFilter = "off";
    }
    config.SetRegionFilter(regionFilter);
}

void action_enable_svchax()
{
    svchax_init(true);
    if (__ctr_svchax && __ctr_svchax_srv) {
        bSvcHaxAvailable = true;
    }
}

// Settings menu keypress callback
bool menu_settings_keypress(int selected, u32 key, void *)
{
    // If key is 0, it means aptMainLoop() returned false, so we're quitting
    if (!key) {
        return true;
    }

    // A button triggers standard actions
    if (key & KEY_A) {
        switch (selected) {
        case 0:
            action_toggle_install();
            break;
        case 1:
            action_toggle_region();
            break;
        case 2:
            action_enable_svchax();
            break;
        case 3:
            bExit = 1;
            break;
        }
        return true;
    }
    // L button triggers mode toggle
    else if (key & KEY_L) {
        action_toggle_install();
        return true;
    }
    // R button triggers region toggle
    else if (key & KEY_R) {
        action_toggle_region();
        return true;
    }

    return false;
}

// Draw the main menu
void menu_settings()
{
    const char *options[] = {
        "Toggle Mode", "Toggle Region", "Enable svchax", "back",
    };
    char footer[50];

    while (!bExit && aptMainLoop()) {
        std::string mode_text;
        switch (config.GetMode()) {
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

        // We have to update the footer every draw, incase the user switches install mode or region
        sprintf(footer, "Mode (L):%s    Region (R):%s", mode_text.c_str(), config.GetRegionFilter().c_str());

        menu_multkey_draw("Settings", footer, 0, sizeof(options) / sizeof(char *), options, NULL, menu_settings_keypress);
    }
    bExit = 0;
}
