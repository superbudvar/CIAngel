#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <malloc.h>

#include <typeinfo>
#include <cmath>
#include <numeric>
#include <iterator>
#include <algorithm>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <hbkb.h>

#include <3ds.h>
#include <citro3d.h>
#include "vshader_shbin.h"
#include "display.h"

#include "utils2.h"
#include "DownloadQueue.h"

#include "svchax/svchax.h"
#include "common.h"
#include "json/json.h"
#include "fts_fuzzy_match.h"
#include "utf8proc/utf8proc.h"
#include "menu.h"
#include "stb/stb_image.h"


// Vector used for download queue
static bool bSvcHaxAvailable = true;
static bool updateScreen = true;
int sourceDataType;
Json::Value sourceData;

u32 kDown;

struct find_game_item {
    std::string titleid;
    find_game_item(std::string titleid) : titleid(titleid) {}
    bool operator () ( const game_item& gi ) const {
        return gi.titleid == titleid;
    }
};

bool compareByScore(const game_item &a, const game_item &b)
{
    return a.score > b.score;
}



std::string getInput(std::string prompt, HB_Keyboard* sHBKB, bool &bCancelled)
{
    GSPGPU_FramebufferFormats format = gfxGetScreenFormat(GFX_BOTTOM);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    clear_screen(GFX_BOTTOM);
    sHBKB->HBKB_Clean();
    touchPosition touch;
    u8 KBState = 4;
    std::string input;
    std::string last_input;
                
    // draw initial prompt
    screen_begin_frame();
    ui_menu_draw_string(prompt.c_str(), 0, 0, 0.6f, COLOR_TITLE);
    screen_end_frame();
    while (KBState != 1 || input.length() == 0)
    {
        if (!aptMainLoop())
        {
            bCancelled = true;
            break;
        }

        hidScanInput();
        hidTouchRead(&touch);
        KBState = sHBKB->HBKB_CallKeyboard(touch);
        input = sHBKB->HBKB_CheckKeyboardInput();

        // If the user cancelled the input
        if (KBState == 3)
        {
            bCancelled = true;
            input = "";
            break;
        }
        // Otherwise if the user has entered a key
        else if (KBState != 4)
        {
            if(strcmp(last_input.c_str(),input.c_str()) != 0) {
                screen_begin_frame();
                ui_menu_draw_string(prompt.c_str(), 0, 0, 0.6f, COLOR_TITLE);
                ui_menu_draw_string(input.c_str(), 10, 30, 0.5f, COLOR_WHITE);
                screen_end_frame();
                last_input = input;
            }
            //Wait for VBlank
            //gfxFlushBuffers();
            //gfxSwapBuffers();
        }
        gfxFlushBuffers();
        gspWaitForVBlank();

    }
    gfxSetScreenFormat(GFX_BOTTOM, format);
    clear_screen(GFX_BOTTOM);
    return input;
}

void load_JSON_data() 
{
    struct stat s_tmp;
    sourceDataType = JSON_TYPE_NONE;
    if(stat("/CIAngel/wings.json", &s_tmp) == 0) {
        screen_begin_frame();
        renderBG();
        setTextColor(COLOR_BLACK);
        renderText(0,220, 0.7f, 0.7f, false, "Loading wings.json...");
        screen_end_frame();
        std::ifstream ifs("/CIAngel/wings.json");
        Json::Reader reader;
        Json::Value obj;
        reader.parse(ifs, sourceData);
        
        if(sourceData[0]["titleID"].isString()) {
          sourceDataType = JSON_TYPE_ONLINE;
        } else if (sourceData[0]["titleid"].isString()) {
          sourceDataType = JSON_TYPE_WINGS;
        }
    }
}

// Search menu keypress callback
bool menu_search_keypress(int selected, u32 key, void* data)
{
    std::vector<game_item>* cb_data = (std::vector<game_item>*)data;

    // If key is 0, it means aptMainLoop() returned false, so we're exiting
    // Go back to the previous menu which will handle quitting
    if (!key) {
        return true;
    }

    // B goes back a screen
    if (key & KEY_B)
    {
        return true;
    }

    // A triggers the default action on the selected title
    if (key & KEY_A)
    {
        // Clean up the console since we'll be using it
        consoleClear();

        // Fetch the title data and start downloading
        std::string selected_titleid = (*cb_data)[selected].titleid;
        std::string selected_enckey = (*cb_data)[selected].titlekey;
        std::string selected_name = (*cb_data)[selected].norm_name;

        printf("OK - %s\n", selected_name.c_str());
        //removes any problem chars, not sure if whitespace is a problem too...?
        removeForbiddenChar(&selected_name, false);

        if(selected_mode == install_ticket){
            char empty_titleVersion[2] = {0x00, 0x00};
            mkpath("/CIAngel/tickets/", 0777); 
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tickets/" + selected_name + ".tik");
            InstallTicket("/CIAngel/tickets/" + selected_name + ".tik");
        }
        else{
            DownloadTitle(selected_titleid, selected_enckey, selected_name);
        }

        wait_key_specific("\nPress A to continue.\n", KEY_A);
        return true;
    }

    // X triggers adding items to the download queue
    if (key & KEY_X)
    {
        consoleClear();
        std::string titleid = (*cb_data)[selected].titleid;
        if (std::find_if(game_queue.begin(), game_queue.end(), find_game_item(titleid)) == game_queue.end())
        {
            game_queue.push_back((*cb_data)[selected]);

            printf("Game added to queue.\n");
        }
        else
        {
            printf("Game already in queue.\n");
        }

        printf("Queue size: %d\n", game_queue.size());
        wait_key_specific("\nPress A to continue.\n", KEY_A);

        return true;
    }

    return false;
}

/* Menu Action Functions */
void action_search()
{
    consoleClear();
    
    if(sourceDataType == JSON_TYPE_NONE) {
        printf("You can't fly without wings.\n\nPress any key to continue\n");
        wait_key();
        return;
    }
    HB_Keyboard sHBKB;
    bool bKBCancelled = false;

    consoleClear();

    std::string searchString = getInput("Please enter text to search for:",&sHBKB, bKBCancelled);
    if (bKBCancelled)
    {
        return;
    }

    // User has entered their input, so let's scrap the keyboard
    clear_screen(GFX_BOTTOM);

    std::vector<game_item> display_output;

    // UTF8 normalization stuff
    utf8proc_option_t options = (utf8proc_option_t)(UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT | UTF8PROC_STRIPMARK | UTF8PROC_STRIPCC);
    utf8proc_uint8_t* szName;
    utf8proc_uint8_t *str;
    int outScore;
    
    for (unsigned int i = 0; i < sourceData.size(); i++) {
        if( regionFilter != "off" && (sourceData[i]["region"].asString() != "ALL" && sourceData[i]["region"].asString() != regionFilter) ) {
            continue;
        }

        // Normalize the name down to ASCII. This may break Japanese characters...
        str = (utf8proc_uint8_t*)sourceData[i]["name"].asCString();
        utf8proc_map(str, 0, &szName, options);
        // Fuzzy match based on the search term
        if (fts::fuzzy_match(searchString.c_str(), (const char*)szName, outScore))
        {
            game_item item;
            item.score = outScore;
            item.index = i;
            item.name = sourceData[i]["name"].asString();
            removeForbiddenChar(&item.name, true);
            item.norm_name = (const char*)szName;
            item.region = sourceData[i]["region"].asString();
            item.installed = false;
            switch(sourceDataType) {
            case JSON_TYPE_WINGS:
              item.titleid = sourceData[i]["titleid"].asString();
              item.titlekey = sourceData[i]["enckey"].asString();
              item.code = sourceData[i]["code"].asString();
              break;
            case JSON_TYPE_ONLINE:
              item.titleid = sourceData[i]["titleID"].asString();
              item.titlekey = sourceData[i]["encTitleKey"].asString();
              item.code = sourceData[i]["serial"].asString();
              break;
            }
            if( bSvcHaxAvailable ) {
                u64 titleId = hex_to_u64(item.titleid);  
                FS_MediaType mediaType = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;
                Result res = 0;
                if( R_SUCCEEDED(res = AM_GetTitleProductCode(mediaType, titleId, nullptr)) ) {
                    item.installed = true;
                }
            }
            std::string typeCheck = item.titleid.substr(4,4);
            //if title id belongs to gameapp/dlc/update/dsiware, use it. if not, ignore. case sensitve of course
            if(typeCheck == "0000" || typeCheck == "008c" || typeCheck == "000e" || typeCheck == "8004"){
                display_output.push_back(item);
            }
        
        }

        free(szName);
    }

    unsigned int display_amount = display_output.size();

    // We technically have 30 rows to work with, minus 2 for header/footer. But stick with 20 entries for now

    if (display_amount == 0)
    {
        printf("No matching titles found.\n");
        wait_key_specific("\nPress A to return.\n", KEY_A);
        return;
    }

    // sort similar names by fuzzy score
    if(display_amount>1) {
        std::sort(display_output.begin(), display_output.end(), compareByScore);
    }
    
    std::string mode_text;
    if(selected_mode == make_cia) {
        mode_text = "Create CIA";
    } else if (selected_mode == install_direct) {
        mode_text = "Install CIA";
    } else if (selected_mode == install_ticket) {
        mode_text = "Create Ticket";
    }

    char footer[51];
    char header[51];
    sprintf(header, "Select a Title (found %i results)", display_amount);
    sprintf(footer, "Press A to %s, X to queue.", mode_text.c_str());
    titles_multkey_draw(header, footer, 1, &display_output, &display_output, menu_search_keypress);
}

void action_prompt_queue()
{
    consoleClear();

    std::string mode_text;
    if(selected_mode == make_cia) {
        mode_text = "download";
    }
    else if (selected_mode == install_direct) {
        mode_text = "install";
    }
    else if (selected_mode == install_ticket) {
        mode_text = "create tickets for";
    }

    printf("Queue contains %d items.\n", game_queue.size());
    for(int i=0; i< game_queue.size(); ++i) {
        printf("%i; %s\n", i+1, game_queue[i].norm_name.c_str() );
    }

    printf("Press A to %s queue.\n", mode_text.c_str());
    printf("Press B to return to menu.\n");
    printf("Press X to clear queue.\n");

    while (aptMainLoop())
    {
        u32 key = wait_key();
        if (key & KEY_B)
        {
            break;
        }

        if (key & KEY_X)
        {
            game_queue.clear();
            break;
        }

        if (key & KEY_A)
        {
            ProcessGameQueue();
            break;
        }
    }

}

void action_manual_entry()
{
    HB_Keyboard sHBKB;
    bool bKBCancelled = false;

    consoleClear();

    // Keep looping so the user can retry if they enter a bad id/key
    while(true)
    {
        printf("Please enter a titleID:\n");
        std::string titleId = getInput("Please enter a titleID:", &sHBKB, bKBCancelled);
        std::string key;
        if (bKBCancelled)
        {
            break;
        }

        for (unsigned int i = 0; i < sourceData.size(); i++){
            std::string tempId = sourceData[i]["titleID"].asString();
            std::string tempKey = sourceData[i]["encTitleKey"].asString();
            if(sourceDataType == JSON_TYPE_WINGS) {
                tempId = sourceData[i]["titleid"].asString();
                tempKey = sourceData[i]["enckey"].asString();
            }

            if(tempId.compare(titleId) == 0 && tempKey.length() == 32) {
               printf("Found encTitleKey, proceeding automatically\n"); 
               key = tempKey;
               break;
            }
        }
        if(key.length() != 32) {
            printf("Please enter the corresponding encTitleKey:\n");
            key = getInput("Please enter the corresponding encTitleKey:", &sHBKB, bKBCancelled);
            if (bKBCancelled)
            {
                break;
            }
        }
        if (titleId.length() == 16 && key.length() == 32)
        {
            DownloadTitle(titleId, key, "");
            wait_key_specific("\nPress A to continue.\n", KEY_A);
            break;
        }
        else
        {   
            printf("There was an error in you input:\n");  
            if(titleId.length() != 16) {
                printf("titleIDs are 16 chars long, not %i\n", titleId.length());
            }
            if(key.length() != 32) {
                printf("encTitleKeys are 32 chars long, not %i\n", key.length());
            }
            printf("\nPress any key\n");
            wait_key();
        }
    }
}

void action_input_txt()
{
    consoleClear();

    std::ifstream input;
    std::string titleId;
    std::string key;

    input.open("/CIAngel/input.txt", std::ofstream::in);
    GetLine(input, titleId);
    GetLine(input, key);
    DownloadTitle(titleId, key, "");

    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

void action_toggle_install()
{
    consoleClear();

    if(selected_mode == make_cia) {
        selected_mode = install_direct;
    } else if (selected_mode == install_direct) {
        selected_mode = install_ticket;
    } else if (selected_mode == install_ticket) {
        selected_mode = make_cia;
    }
    
    if ( (selected_mode == install_ticket) || (selected_mode == install_direct) )
    {
        if (!bSvcHaxAvailable)
        {
            selected_mode = make_cia;
            printf(CONSOLE_RED "Kernel access not available.\nCan't enable Install modes.\nYou can only make a CIA.\n" CONSOLE_RESET);
            wait_key_specific("\nPress A to continue.", KEY_A);
        }
    }
}

void action_toggle_region()
{
    if(regionFilter == "off") {
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
}

void action_about()
{
    screen_begin_frame();
    setTextColor(COLOR_RED);
    renderText(0, 2, 1.0f, 1.0f, false, "CIAngel by cearp and Drakia\n");
    setTextColor(0xFFCCCCCC);
    renderText(0, 32, 0.6f, 0.6f, false, "Download, create and install CIAs directly from\nNintendo's CDN servers. Grabbing the latest games\nhas never been so easy.\n");
    screen_end_frame();
    
    consoleClear();
    printf(CONSOLE_RED "CIAngel by cearp and Drakia\n");
    printf( CONSOLE_RESET);
    printf("Download, create and install CIAs\n");
    printf("directly from Nintendo's CDN servers.\n");
    printf("Grabbing the latest games has never been");
    printf("so easy.\n");
    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

void action_exit()
{
    bExit = true;
}

void action_download()
{
    consoleClear();

    download_JSON();
    load_JSON_data();
}

// Main menu keypress callback
bool menu_main_keypress(int selected, u32 key, void*)
{
    // If key is 0, it means aptMainLoop() returned false, so we're quitting
    if (!key) {
        return true;
    }

    // A button triggers standard actions
    if (key & KEY_A)
    {
        switch (selected)
        {
            case 0:
                action_search();
            break;
            case 1:
                action_prompt_queue();
            break;
            case 2:
                action_manual_entry();
            break;
            case 3:
                action_input_txt();
            break;
            case 4:
                action_download();
            break;
            case 5:
                action_about();
            break;
            case 6:
                action_exit();
            break;
        }
        return true;
    }
    // L button triggers mode toggle
    else if (key & KEY_L)
    {
        action_toggle_install();
        return true;
    }
    // R button triggers region toggle
    else if (key & KEY_R)
    {
        action_toggle_region();
        return true;
    }

    return false;
}

// Draw the main menu
void menu_main()
{
    const char *options[] = {
        "Search for a title by name",
        "Process download queue",
        "Enter a title key/ID pair",
        "Fetch title key/ID from input.txt",
        "Download wings.json",
        "About CIAngel",
        "Exit",
    };
    char footer[50];

    while (!bExit && aptMainLoop())
    {
        //Wait for VBlank
        
        // We have to update the footer every draw, incase the user switches install mode or region
        sprintf(footer, "(L):Install Mode (R):Region |  Queue: %d", game_queue.size());

        menu_multkey_draw("CIAngel by cearp and Drakia", footer, 0, sizeof(options) / sizeof(char*), options, NULL, menu_main_keypress);
//        if(updateScreen) {
            updateScreen = false;
            clear_screen(GFX_BOTTOM);
//        }

        menu_multkey_draw("CIAngel by cearp and Drakia", footer, 0, sizeof(options) / sizeof(char*), options, NULL, menu_main_keypress);

    }
}

int main(int argc, const char* argv[])
{
    romfsInit();
    gfxInitDefault();
    consoleInit(GFX_BOTTOM,NULL); 
    sceneInit();
    screen_begin_frame();
    //screen_load_texture_file(TEXTURE_BOTTOM_SCREEN_BG, "bottom_screen_bg.png", true);
    renderBG();
    screen_end_frame();
    // Sadly svchax crashes too much, so only allow install mode when running as a CIA
    // Trigger svchax so we can install CIAs
    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            bSvcHaxAvailable = false;
            printf("Failed to acquire kernel access. Install mode disabled.\n");
        }
    }
    
    // argc is 0 when running as a CIA, and 1 when running as a 3dsx
    if (argc > 0)
    {
   //     bSvcHaxAvailable = false;
    }

    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();
    acInit();

    init_menu(GFX_BOTTOM);

    if (bSvcHaxAvailable)
    {
        amInit();
        AM_InitializeExternalTitleDatabase(false);
    }

    // Make sure /CIAngel exists on the SD card
    mkpath("/CIAngel", 0777);
    
    // Set up the reading of json
    check_JSON();
    load_JSON_data();
    
    menu_main();

    if (bSvcHaxAvailable)
    {
        amExit();
    }

    sceneExit();
    C3D_Fini();
    acExit();
    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
