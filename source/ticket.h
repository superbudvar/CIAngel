#pragma once
#include <fstream>
#include <3ds.h>

#include "data.h"
#include "types.h"
#include "utils2.h"
#include "utils.h"

const u16 tmd_top = 0x140;

void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath);
void InstallTicket(std::string FullPath, std::string TitleId);
