#include <string>
#include <stddef.h>
#include <vector>
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

#include "utils.h"
#include "cia.h"
#include "data.h"
#include "menu.h"

#include "svchax/svchax.h"
#include "json/json.h"

static const u16 top = 0x140;
static bool bSvcHaxAvailable = true;
enum install_modes {make_cia, install_direct, install_ticket};
install_modes selected_mode = make_cia;
int selectedOption = -2;
int sub_menu_stage = 0;
static int current = 0;
static bool updateScreen = true;
static std::string regionFilter = "off";
u32 kDown;
std::stack<int> currentSelection;

struct display_item {
  int ld;
  int index;
  u64 titleId;
  std::string name;
  std::string region;
  std::string serial;
};

Json::Value characters;



std::string upper(std::string s)
{
  std::string ups;
  
  for(unsigned int i = 0; i < s.size(); i++)
  {
    ups.push_back(std::toupper(s[i]));
  }
  
  return ups;
}


bool compareByLD(const display_item &a, const display_item &b)
{
    return a.ld < b.ld;
}

bool FileExists (std::string name){
    struct stat buffer;
    return (stat (name.c_str(), &buffer) == 0);
}

Result ConvertToCIA(std::string dir, std::string titleId)
{
    char cwd[1024];
    if (getcwdir(cwd, sizeof(cwd)) == NULL){
        printf("[!] Could not store Current Working Directory\n");
        return -1;
    }
    chdir(dir.c_str());
    FILE *tik = fopen("cetk", "rb");
    if (!tik) return -1;
    TIK_CONTEXT tik_context = process_tik(tik);

    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) return -1;
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        fclose(tik);
        fclose(tmd);
        return -1;
    }

    chdir(cwd);

    int result;
    if (selected_mode == install_direct)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        FILE *output = fopen((dir + "/" + titleId + ".cia").c_str(),"wb");
        if (!output) return -2;

        result = generate_cia(tmd_context, tik_context, output);
        if(result != 0){
            remove((dir + "/" + titleId + ".cia").c_str());
        }
    }

    return result;
}

std::string u32_to_hex_string(u32 i)
{
    std::stringstream stream;
    stream << std::setfill ('0') << std::setw(sizeof(u32)*2) << std::hex << i;
    return stream.str();
}

int mkpath(std::string s,mode_t mode)
{
    size_t pre=0,pos;
    std::string dir;
    int mdret = 0;

    if(s[s.size()-1]!='/'){
        // force trailing / so we can handle everything in loop
        s+='/';
    }

    while((pos=s.find_first_of('/',pre))!=std::string::npos){
        dir=s.substr(0,pos++);
        pre=pos;
        if(dir.size()==0) continue; // if leading / first time is 0 length
        if((mdret=mkdir(dir.c_str(),mode)) && errno!=EEXIST){
            return mdret;
        }
    }
    return mdret;
}

char parse_hex(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    std::abort();
}

char* parse_string(const std::string & s)
{
    char* buffer = new char[s.size() / 2];
    for (std::size_t i = 0; i != s.size() / 2; ++i)
        buffer[i] = 16 * parse_hex(s[2 * i]) + parse_hex(s[2 * i + 1]);
    return buffer;
}

void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath)
{
    std::ofstream ofs;

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    ofs.write(tikTemp, 0xA50);
    ofs.close();

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::in | std::ofstream::binary);

    //write version
    ofs.seekp(top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);

    //write title id
    ofs.seekp(top+0x9C, std::ios::beg);
    ofs.write(parse_string(titleId), 0x8);

    //write key
    ofs.seekp(top+0x7F, std::ios::beg);
    ofs.write(parse_string(encTitleKey), 0x10);

    ofs.close();
}

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string outputDir)
{
    std::string mode_text;
    if(selected_mode == make_cia){
        mode_text = "creating";
    }
    else if(selected_mode == install_direct){
        mode_text = "installing";
    }


    printf("Starting - %s\n", titleId.c_str());

    mkpath((outputDir + "/tmp/").c_str(), 0777);

    // Make sure the CIA doesn't already exist
    if ( (selected_mode == make_cia) && FileExists(outputDir + "/" + titleId + ".cia"))
    {
        printf("%s/%s.cia already exists.\n", outputDir.c_str(), titleId.c_str());
        return 0;
    }

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) return -1;
    Result res = DownloadFile((NUS_URL + titleId + "/tmd").c_str(), oh, false);
    fclose(oh);
    if (res != 0)
    {
        printf("Could not download TMD. Internet/Title ID is OK?\n");
        return res;
    }

    //read version
    std::ifstream tmdfs;
    tmdfs.open(outputDir + "/tmp/tmd", std::ofstream::out | std::ofstream::in | std::ofstream::binary);
    char titleVersion[2];
    tmdfs.seekg(top+0x9C, std::ios::beg);
    tmdfs.read(titleVersion, 0x2);
    tmdfs.close();

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/cetk");

    printf("Now %s the CIA...\n", mode_text.c_str());

    res = ConvertToCIA(outputDir + "/tmp", titleId);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", mode_text.c_str());
        return res;
    }

    if(selected_mode == make_cia)
    {
        rename((outputDir + "/tmp/" + titleId + ".cia").c_str(), (outputDir + "/" + titleId + ".cia").c_str());
    }

    printf(" DONE!\n");
    printf("Enjoy the game :)\n");

    // TODO remove tmp dir

    return res;
}

std::string getInput(HB_Keyboard* sHBKB, bool &bCancelled)
{
    GSPGPU_FramebufferFormats format = gfxGetScreenFormat(GFX_BOTTOM);
    gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    clear_screen(GFX_BOTTOM);
    sHBKB->HBKB_Clean();
    touchPosition touch;
    u8 KBState = 4;
    std::string input;
    std::string last_input;
    while (KBState != 1 || input.length() == 0)
    {
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
                ui_menu_draw_string(input.c_str(), 10, 10, COLOR_SELECTED);
                sceneDraw();
                last_input = input;
                // Flush and swap framebuffers
                gfxFlushBuffers();
                gfxSwapBuffers();
            }
        }


        //Wait for VBlank
        gspWaitForVBlank();
    }
    gfxSetScreenFormat(GFX_BOTTOM, format);
    clear_screen(GFX_BOTTOM);
    return input;
}

void removeForbiddenChar(std::string* s)
{
    std::string::iterator it;
    std::string illegalChars = "\\/:?\"<>|";
    for (it = s->begin() ; it < s->end() ; ++it){
        bool found = illegalChars.find(*it) != std::string::npos;
        if(found)
        {
            *it = ' ';
        }
    }
}

std::istream& GetLine(std::istream& is, std::string& t)
{
    t.clear();
    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
              return is;
            case '\r':
              if (sb->sgetc() == '\n')
                sb->sbumpc();
              return is;
            case  EOF:
              if (t.empty())
                is.setstate(std::ios::eofbit);
              return is;
            default:
              t += (char)c;
        }
    }
}

u64 hex_to_u64( std::string value) {
    u64 out;
    std::istringstream(value) >> std::hex >> out;
    return out;
}

std::string ToHex(const std::string& s)
{
    std::ostringstream ret;
    for (std::string::size_type i = 0; i < s.length(); ++i)
    {
        int z = s[i]&0xff;
        ret << std::hex << std::setfill('0') << std::setw(2) << z;
    }
    return ret.str();
}

int levenshtein_distance(const std::string &s1, const std::string &s2)
{
    // To change the type this function manipulates and returns, change
    // the return type and the types of the two variables below.
    int s1len = s1.size();
    int s2len = s2.size();
    
    auto column_start = (decltype(s1len))1;
    
    auto column = new decltype(s1len)[s1len + 1];
    std::iota(column + column_start, column + s1len + 1, column_start);
    
    for (auto x = column_start; x <= s2len; x++) {
        column[0] = x;
        auto last_diagonal = x - column_start;
        for (auto y = column_start; y <= s1len; y++) {
            auto old_diagonal = column[y];
            auto possibilities = {
                column[y] + 1,
                column[y - 1] + 1,
                last_diagonal + (s1[y - 1] == s2[x - 1]? 0 : 1)
            };
            column[y] = std::min(possibilities);
            last_diagonal = old_diagonal;
        }
    }
    auto result = column[s1len];
    delete[] column;
    return result;
}

int menu_draw_list(const char *title, const char* footer, int back, int count, std::vector<display_item> options, bool forceRefresh)
{
    int selected = -2;
    u32 key = hidKeysDown();
    int menu_items = count;
    int max_items_onscreen = 15;
    
    if(forceRefresh || key) {
       // int current = 0;
        if (key & KEY_UP) {
            if (current <= 0) {
                current = menu_items - 1;
            } else {
                current--;
            }
        } else if (key & KEY_DOWN) {
            if (current >= menu_items - 1) {
                current = 0;
            } else {
                current++;
            }
        } else if (key & KEY_RIGHT) {
            current += 5;
            if (current >= menu_items) current = menu_items - 1;

        } else if (key & KEY_LEFT) {
            current -= 5;
            if (current < 0) current = 0;

        } else if (key & KEY_A) {
            selected = current;
        } else if ((key & KEY_B) && back) {
            selected = -1;
        }
        //  ui_menu_draw(title, footer, back, count, options);
        setTextColor(0xFF00FF00);
        renderText(0, 8, 0.7f, 0.7f, false, title);
        int start_index = 0;
        if(current > max_items_onscreen-1) {
            start_index = (current+1)-max_items_onscreen;
        }
        for (int i = 0; i + start_index < menu_items && i < max_items_onscreen; i++) {
            int y_pos = 32+(i*12);
            ui_menu_draw_string(options[start_index+i].region.c_str(), 1, y_pos, i+start_index==current? 0xFF0000FF: 0xFFFFFFFF);
            ui_menu_draw_string(options[start_index+i].name.c_str(), 35, y_pos, i+start_index==current? 0xFF0000FF: 0xFFFFFFFF);
            ui_menu_draw_string(options[start_index+i].serial.c_str(), 290, y_pos, i+start_index==current? 0xFF0000FF: 0xFFFFFFFF);
        }
    }
    return selected;
}

/* Menu Action Functions */
void action_search()
{
    consoleClear();
    printf("Loading json\n");
    HB_Keyboard sHBKB;
    std::string searchstring;
    std::string mode_text;
    std::vector<display_item> display_output;
    int result = -2; // Set as unselected
    int displayCount = 0;
    bool bKBCancelled = false;
    bool refresh = true;
    bool finished = false;
    // We technically have 30 rows to work with, minus 2 for header/footer. But stick with 20 entries for now
    unsigned int display_amount = 100; 
    char* results[100];
    char productCode;
    char footer[51];
    Result res = 0;
    AM_TitleEntry entry;

    consoleClear();
    clear_screen(GFX_BOTTOM);
    while(!finished) {
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown) {
            refresh= true;
            if(kDown & KEY_START) {
                finished = true;
                break; // break in order to return to hbmenu
            }
        }
        if(sub_menu_stage == 0) {
                // Get the search value
                ui_menu_draw_string("Please enter text to search for:", 0, 0, COLOR_NEUTRAL);
                searchstring = getInput(&sHBKB, bKBCancelled);
                if (bKBCancelled) {
                    if(searchstring.length() == 0)
                    { 
                        finished = true;
                        sub_menu_stage = -1;
                    }
                }
                sub_menu_stage++;
        } else if(sub_menu_stage == 1) {
                // Get Results
                for (unsigned int i = 0; i < characters.size(); i++) {
                    std::string temp;
                    temp = characters[i]["name"].asString();
                    u64 titleId;
                    titleId = hex_to_u64(characters[i]["titleID"].asString());  
                    if(temp.size() >0) {    
                        if(regionFilter == "off" || (characters[i]["region"].asString() == "ALL" || characters[i]["region"].asString() == regionFilter)) {
                    //      printf("Name: %s, Region: %s, len: %i\n", temp.c_str(), characters[i]["region"].asString().c_str(), temp.size());
                            if(temp.find("-System") == std::string::npos) {
                                int ld = levenshtein_distance(upper(temp), upper(searchstring));
                                if (ld < 10)
                                {
                                    display_item item;
                                    item.ld = ld;
                                    item.index = i;
                                    item.titleId = titleId;
                                    item.name = characters[i]["name"].asString();
                                    item.region = characters[i]["region"].asString();
                                    item.serial = characters[i]["serial"].asString();
                                    display_output.push_back(item);
                    
                                    FS_MediaType mediaType = ((titleId >> 32) & 0x8010) != 0 ? MEDIATYPE_NAND : MEDIATYPE_SD;
//                                  FS_MediaType mediaType = MEDIATYPE_NAND;
//                                  if(R_SUCCEEDED(res = AM_GetTitleInfo(mediaType, 1, &titleId, &entry))) {
                                    if( R_SUCCEEDED(res = AM_GetTitleProductCode(mediaType, titleId, nullptr)) ) {
                                        printf("%s\n",characters[i]["name"].asString().c_str());
                                    }
                                }
                            }
                        }
                    }
                }

                // sort similar names by levenshtein distance
                std::sort(display_output.begin(), display_output.end(), compareByLD);
                displayCount = display_amount;
                if ( display_output.size() < display_amount )
                {
                    displayCount = display_output.size();
                }

                if (displayCount == 0)
                {
                    printf("No matching titles found.\n");
                    wait_key_specific("\nPress A to return.\n", KEY_A);
                    sub_menu_stage = 0;
                    finished = true;
                } else {
                    // Eh, allocated memory because we need to format the data
           /*         for (u8 i = 0; i < displayCount; i++)
                    {
                        results[i] = &display_output[i](char*)malloc(51 * sizeof(char));
                        sprintf(results[i], "%s: %-30s \t %s",
                        characters[display_output[i].index]["region"].asString().c_str(),
                        characters[display_output[i].index]["name"].asString().c_str(),
                        characters[display_output[i].index]["serial"].asString().c_str());
                    } */
                    sub_menu_stage++;
                }
        } else if(sub_menu_stage == 2) {
                if(selected_mode == make_cia) {
                    mode_text = "Create CIA";
                } else if (selected_mode == install_direct) {
                    mode_text = "Install CIA";
                } else if (selected_mode == install_ticket) {
                    mode_text = "Create Ticket";
                }

                sprintf(footer, "Press A to %s. Press B to return.", mode_text.c_str());
                result = -2;
                refresh = true;
                result = menu_draw_list("Select a Title", footer, 1, displayCount, display_output, refresh);
                //printf("%s, %llx\n", characters[display_output[getCurrent()].index]["titleID"].asString().c_str(), hex_to_u64(characters[display_output[getCurrent()].index]["titleID"].asString()));
                if(result < 0) {
                    if (result == -1)
                    {
                        // Free our allocated memory
/*                        for (u8 i = 0; i < displayCount; i++)
                        {
                            free(results[i]);
                        }*/
                        finished = true;
                        sub_menu_stage = 0;
                    }
                } else {
                    sub_menu_stage++;
                    // Clean up the console since we'll be using it
                    consoleClear();
                    sceneDraw();
                }
        } else if(sub_menu_stage == 3) {
                // Fetch the title data and start downloading
                std::string selected_titleid = characters[display_output[result].index]["titleID"].asString();
                std::string selected_enckey = characters[display_output[result].index]["encTitleKey"].asString();
                std::string selected_name = characters[display_output[result].index]["name"].asString();

                printf("OK - %s\n", selected_name.c_str());
                //removes any problem chars, not sure if whitespace is a problem too...?
                removeForbiddenChar(&selected_name);

                if(selected_mode == install_ticket){
                    char empty_titleVersion[2] = {0x00, 0x00};
                    mkpath("/CIAngel/tickets/", 0777); 
                    CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tickets/" + selected_name + ".tik"); 
                }
                else{
                    DownloadTitle(selected_titleid, selected_enckey, "/CIAngel/" + selected_name);
                }

                wait_key_specific("\nPress A to continue.\n", KEY_A);
                sub_menu_stage=0;
                selectedOption = -1;
                finished = true;
        }
        if(refresh) {
            sceneDraw();
            refresh=false;
        }
        gspWaitForVBlank();
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
        std::string titleId = getInput(&sHBKB, bKBCancelled);
        if (bKBCancelled)
        {
            break;
        }

        printf("Please enter the corresponding encTitleKey:\n");
        std::string key = getInput(&sHBKB, bKBCancelled);
        if (bKBCancelled)
        {
            break;
        }

        if (titleId.length() == 16 && key.length() == 32)
        {
            DownloadTitle(titleId, key, "/CIAngel");
            wait_key_specific("\nPress A to continue.\n", KEY_A);
            break;
        }
        else
        {
            printf("encTitleKeys are 32 characters long,\nand titleIDs are 16 characters long.\n");
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
    DownloadTitle(titleId, key, "/CIAngel");

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
            printf(CONSOLE_RED "Kernel access not available.\nCan't enable Install modes. You can only make a CIA.\n" CONSOLE_RESET);
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
        regionFilter = "---";
    } else if (regionFilter == "---") {
        regionFilter = "off";
    }
}

void action_about()
{
    consoleClear();

    printf(CONSOLE_RED "CIAngel by cearp and Drakia\n" CONSOLE_RESET);
    printf("Download, create, and install CIAs directly\n");
    printf("from Nintendo's CDN servers. Grabbing the\n");
    printf("latest games has never been so easy.\n");
    setTextColor(0xFF0000FF);
    renderText(0, 2, 1.0f, 1.0f, false, "CIAngel by cearp and Drakia\n");
    setTextColor(0xFFCCCCCC);
    renderText(0, 32, 0.6f, 0.6f, false, "Download, create, and install CIAs directly from\nNintendo's CDN servers. Grabbing the latest games\nhas never been so easy.\n");
sceneDraw();
    wait_key_specific("\nPress A to continue.\n", KEY_A);
}

/* Menu functions */
void menu_main(bool refresh)
{
    std::string mode_text;
    if(!updateScreen) {
        updateScreen = refresh;
    }
    const char *options[] = {
        "Search for a title by name",
        "Enable region filter for search",
        "Enter a title key/ID pair",
        "Fetch title key/ID from input.txt",
        "Change Install mode (see footer)",
        "About CIAngel",
        "Exit"
    };
    char footer[42];

        if(selected_mode == make_cia) {
            mode_text = "Create CIA";
        }
        else if (selected_mode == install_direct) {
            mode_text = "Install CIA";
        }
        else if (selected_mode == install_ticket) {
            mode_text = "Create Ticket";
        }

        // We have to update the footer every draw, incase the user switches install mode
        sprintf(footer, "Mode:%s Region:%s", mode_text.c_str(), regionFilter.c_str());

        int result = menu_draw_nb("CIAngel by cearp and Drakia", footer, 0, sizeof(options) / sizeof(char*), options, refresh);
        if(result != -2) {
            selectedOption = result;
        }
}

bool runLoop() {
        switch (selectedOption)
        {
            case -1:
                menu_main(false);
                return true;
            break;
            case 0:
                if(currentSelection.size() == 0) {
                    currentSelection.push(-2);
                }
                action_search();
                currentSelection.pop();
            break;
            case 1:
                action_toggle_region();
            break;
            case 2:
                action_manual_entry();
    clear_screen(GFX_BOTTOM);
            break;
            case 3:
                action_input_txt();
    clear_screen(GFX_BOTTOM);
            break;
            case 4:
                action_toggle_install();
            break;
            case 5:
                action_about();
            break;
            case 6:
                return false;
            break;
        }
        menu_main(true);
        selectedOption = -1;
    return true;
}
int main(int argc, const char* argv[])
{
    /* Sadly svchax crashes too much, so only allow install mode when running as a CIA
    // Trigger svchax so we can install CIAs
    if(argc > 0) {
        svchax_init(true);
        if(!__ctr_svchax || !__ctr_svchax_srv) {
            bSvcHaxAvailable = false;
            //printf("Failed to acquire kernel access. Install mode disabled.\n");
        }
    }
    */
    
    // argc is 0 when running as a CIA, and 1 when running as a 3dsx
    if (argc > 0)
    {
        bSvcHaxAvailable = false;
    }

    u32 *soc_sharedmem, soc_sharedmem_size = 0x100000;
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);

    httpcInit(0);
    soc_sharedmem = (u32 *)memalign(0x1000, soc_sharedmem_size);
    socInit(soc_sharedmem, soc_sharedmem_size);
    sslcInit(0);
    hidInit();

    if (bSvcHaxAvailable)
    {
        amInit();
        AM_InitializeExternalTitleDatabase(false);
    }
    // Initialize the scene
    sceneInit();
    sceneRender(1.0f);  
    // Set up the reading of json
    std::ifstream ifs("/CIAngel/wings.json");
    Json::Reader reader;
    Json::Value obj;
    reader.parse(ifs, obj);
    characters = obj; // array of characters
    init_menu(GFX_BOTTOM);
    
    currentSelection.push(0);
    while (aptMainLoop()) {
        hidScanInput();
        kDown = hidKeysDown();

        if (kDown & KEY_START) break; // break in order to return to hbmenu
        if(kDown) {
        }
            if(!runLoop()) break;

        // Flush and swap framebuffers
        if(updateScreen) {
            updateScreen = false;
//          sceneDraw();
            gfxFlushBuffers();
            gfxSwapBuffers();
        }
        //Wait for VBlank
        gspWaitForVBlank();
    }

    if (bSvcHaxAvailable)
    {
        amExit();
    }

    sceneExit();
    C3D_Fini();
    gfxExit();
    hidExit();
    httpcExit();
    socExit();
    sslcExit();
}
