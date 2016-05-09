#include "DownloadQueue.h"

std::vector<game_item> game_queue;

Result ConvertToCIA(std::string dir, std::string titleName)
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
        FILE *output = fopen((dir + "/" + titleName + ".cia").c_str(),"wb");
        if (!output) return -2;

        result = generate_cia(tmd_context, tik_context, output);
        if(result != 0){
            remove((dir + "/" + titleName + ".cia").c_str());
        }
    }

    return result;
}
void DownloadQueueItem(std::string titleId, std::string encTitleKey, std::string titleName) {
}

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string titleName)
{
    // Wait for wifi to be available
    u32 wifi = 0;
    Result ret;
    while(R_SUCCEEDED(ret = ACU_GetWifiStatus(&wifi)) && wifi == 0)
    {
        hidScanInput();
        if (hidKeysDown() & KEY_B)
        {
            ret = -1;
            break;
        }
    }

    if (R_FAILED(ret))
    {
        printf("Unable to access internet.\n");
        return ret;
    }

    std::string outputDir = "/CIAngel";

    if (titleName.length() == 0)
    {
        titleName = titleId;
    }

    std::string mode_text;
    if(selected_mode == make_cia){
        mode_text = "create";
    }
    else if(selected_mode == install_direct){
        mode_text = "install";
    }


    printf("Starting - %s\n", titleName.c_str());

    mkpath((outputDir + "/tmp/").c_str(), 0777);

    // Make sure the CIA doesn't already exist
    std::string cp = outputDir + "/" + titleName + ".cia";
    char *ciaPath = new char[cp.size()+1];
    ciaPath[cp.size()]=0;
    memcpy(ciaPath,cp.c_str(),cp.size());
    if ( (selected_mode == make_cia) && FileExists(ciaPath))
    {
        free(ciaPath);
        printf("%s/%s.cia already exists.\n", outputDir.c_str(), titleName.c_str());
        return 0;
    }
    free(ciaPath);

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) 
    {
        printf("Error opening %s/tmp/tmd\n", outputDir.c_str());
        return -1;
    }
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
    tmdfs.seekg(tmd_top+0x9C, std::ios::beg);
    tmdfs.read(titleVersion, 0x2);
    tmdfs.close();

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/cetk");

    printf("Now %s the CIA...\n", mode_text.c_str());

    res = ConvertToCIA(outputDir + "/tmp", titleName);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", mode_text.c_str());
        return res;
    }

    if (selected_mode == make_cia)
    {
        rename((outputDir + "/tmp/" + titleName + ".cia").c_str(), (outputDir + "/" + titleName + ".cia").c_str());
    }

    printf(" DONE!\n");

    // TODO remove tmp dir

    return res;
}

void ProcessGameQueue()
{
    // Create the tickets folder if we're in ticket mode
    char empty_titleVersion[2] = {0x00, 0x00};
    if (selected_mode == install_ticket)
    {
        mkpath("/CIAngel/tickets/", 0777); 
    }

    std::vector<game_item>::iterator game = game_queue.begin();
    while(aptMainLoop() && game != game_queue.end())
    {
        std::string selected_titleid = (*game).titleid;
        std::string selected_enckey = (*game).titlekey;
        std::string selected_name = (*game).norm_name;

        if (selected_mode == install_ticket)
        {
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tickets/" + selected_name + ".tik");
            InstallTicket("/CIAngel/tickets/" + selected_name + ".tik");
        }
        else
        {
            DownloadTitle(selected_titleid, selected_enckey, selected_name);
        }

        game = game_queue.erase(game);
    }

    wait_key_specific("Press A to continue.\n", KEY_A);
}
