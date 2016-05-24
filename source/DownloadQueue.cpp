#include "DownloadQueue.h"

std::vector<game_item> game_queue;

Result ProcessCIA(std::string dir, std::string titleName)
{
    FILE *tik = fopen((dir + "/ticket").c_str(), "rb");
    if (!tik) 
    {
        return -1;
    }
    TIK_CONTEXT tik_context = process_tik(tik);

    FILE *tmd = fopen((dir + "/tmd").c_str(),"rb");
    if (!tmd) 
    {
        fclose(tik);
        return -1;
    }
    TMD_CONTEXT tmd_context = process_tmd(tmd);

    if(tik_context.result != 0 || tmd_context.result != 0){
        printf("[!] Input files could not be processed successfully\n");
        free(tmd_context.content_struct);
        fclose(tik);
        fclose(tmd);
        return -1;
    }

    int result;
    if (config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        result = install_cia(tmd_context, tik_context);
    }
    else
    {
        FILE *output = fopen((dir + "/" + titleName + ".cia").c_str(),"wb");
        if (!output) 
        {
            result = -2;
        }
        else
        {
            result = generate_cia(tmd_context, tik_context, output);
            if(result != 0)
            {
                remove((dir + "/" + titleName + ".cia").c_str());
            }
        }
    }

    // free allocated memory/handles
    free(tmd_context.content_struct);
    fclose(tik);
    fclose(tmd);

    // Clean up temp files
    remove((dir + "/ticket").c_str());
    remove((dir + "/tmd").c_str());
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
    if(config.GetMode() == CConfig::Mode::DOWNLOAD_CIA)
    {
        mode_text = "create";
    }
    else if(config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        mode_text = "install";
    }


    printf("Starting - %s\n", titleName.c_str());

    // Make sure the CIA doesn't already exist
    std::string cp = outputDir + "/" + titleName + ".cia";
    char *ciaPath = new char[cp.size()+1];
    ciaPath[cp.size()]=0;
    memcpy(ciaPath,cp.c_str(),cp.size());
    if (config.GetMode() == CConfig::Mode::DOWNLOAD_CIA && FileExists(ciaPath))
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

    CreateTicket(titleId, encTitleKey, titleVersion, outputDir + "/tmp/ticket");

    printf("Now %s the CIA...\n", mode_text.c_str());

    res = ProcessCIA(outputDir + "/tmp", titleName);
    if (res != 0)
    {
        printf("Could not %s the CIA.\n", mode_text.c_str());
        return res;
    }

    if (config.GetMode() == CConfig::Mode::DOWNLOAD_CIA)
    {
        rename((outputDir + "/tmp/" + titleName + ".cia").c_str(), (outputDir + "/" + titleName + ".cia").c_str());
    }

    printf(" DONE!\n");

    return res;
}

void ProcessGameQueue()
{
    // Create the tickets folder if we're in ticket mode
    char empty_titleVersion[2] = {0x00, 0x00};

    std::vector<game_item>::iterator game = game_queue.begin();
    while(aptMainLoop() && game != game_queue.end())
    {
        std::string selected_titleid = (*game).titleid;
        std::string selected_enckey = (*game).titlekey;
        std::string selected_name = (*game).ascii_name;

        if (config.GetMode() == CConfig::Mode::INSTALL_TICKET)
        {
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tmp/ticket");
            InstallTicket("/CIAngel/tmp/ticket");
        }
        else
        {
            Result res = DownloadTitle(selected_titleid, selected_enckey, selected_name);
            if (R_FAILED(res)) {
                printf("Error processing queue. Returning to menu\n");
                break;
            }
        }

        game = game_queue.erase(game);
    }

    wait_key_specific("Press A to continue.\n", KEY_A);
}
