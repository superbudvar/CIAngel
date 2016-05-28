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

Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string titleName, std::string region)
{
    // Convert the titleid to a u64 for later use
    char* nTitleId = parse_string(titleId);
    u64 uTitleId = u8_to_u64((u8*)nTitleId, BIG_ENDIAN);
    free (nTitleId);

    // Wait for wifi to be available
    u32 wifi = 0;
    Result ret = 0;
    Result res = 0;
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

    // Include region in filename
    if (region.length() > 0)
    {
        titleName = titleName + " (" + region + ")";
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

    // If in install mode, download/install the SEED entry
    if (config.GetMode() == CConfig::Mode::INSTALL_CIA)
    {
        // Download and install the SEEDDB entry if install mode
        // Code based on code from FBI: https://github.com/Steveice10/FBI/blob/master/source/core/util.c#L254
        // Copyright (C) 2015 Steveice10
        u8 seed[16];
        static const char* regionStrings[] = {"JP", "US", "GB", "GB", "HK", "KR", "TW"};
        u8 region = CFG_REGION_USA;
        CFGU_GetSystemLanguage(&region);

        if(region <= CFG_REGION_TWN) {
            char url[128];
            snprintf(url, 128, SEED_URL "0x%016llX/ext_key?country=%s", uTitleId, regionStrings[region]);

            httpcContext context;
            if(R_SUCCEEDED(res = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1))) {
                httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);

                u32 responseCode = 0;
                if(R_SUCCEEDED(res = httpcBeginRequest(&context)) && R_SUCCEEDED(res = httpcGetResponseStatusCode(&context, &responseCode, 0))) {
                    if(responseCode == 200) {
                        u32 pos = 0;
                        u32 bytesRead = 0;
                        while(pos < sizeof(seed) && (R_SUCCEEDED(res = httpcDownloadData(&context, &seed[pos], sizeof(seed) - pos, &bytesRead)) || (u32)res == HTTPC_RESULTCODE_DOWNLOADPENDING)) {
                            pos += bytesRead;
                        }
                    } else {
                        res = -1;
                    }
                }

                httpcCloseContext(&context);
            }

            if (R_SUCCEEDED(res))
            {
                res = InstallSeed(uTitleId, seed);
                if (R_FAILED(res))
                {
                    printf("Error installing SEEDDB entry: 0x%lx\n", res);
                }
            }
        }
    }

    // Make sure the CIA doesn't already exist
    std::string cp = outputDir + "/" + titleName + ".cia";
    if (config.GetMode() == CConfig::Mode::DOWNLOAD_CIA && FileExists(cp.c_str()))
    {
        printf("%s already exists.\n", cp.c_str());
        return 0;
    }

    std::ofstream ofs;

    FILE *oh = fopen((outputDir + "/tmp/tmd").c_str(), "wb");
    if (!oh) 
    {
        printf("Error opening %s/tmp/tmd\n", outputDir.c_str());
        return -1;
    }
    res = DownloadFile((NUS_URL + titleId + "/tmd").c_str(), oh, false);
    fclose(oh);
    if (res != 0)
    {
        printf("Could not download TMD. Internet/Title ID is OK?\n");
        return res;
    }

    // Read version
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
        char error[50];
        sprintf(error, "Could not %s the CIA.\n", mode_text.c_str());
        screen_begin_frame(true);
        printf(error);
        setTextColor(COLOR_DEFAULT);
        ui_printf(error);
        setTextColor(COLOR_PROMPT);
        ui_printf("\nPress A to continue\n");
        screen_end_frame();
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
        std::string selected_region = (*game).region;

        if (config.GetMode() == CConfig::Mode::INSTALL_TICKET)
        {
            CreateTicket(selected_titleid, selected_enckey, empty_titleVersion, "/CIAngel/tmp/ticket");
            InstallTicket("/CIAngel/tmp/ticket", selected_titleid);
        }
        else
        {
            Result res = DownloadTitle(selected_titleid, selected_enckey, selected_name, selected_region);
            if (R_FAILED(res)) {
                printf("Error processing queue. Returning to menu\n");
                break;
            }
        }

        game = game_queue.erase(game);
    }

    wait_key_specific("Press A to continue.\n", KEY_A);
}
