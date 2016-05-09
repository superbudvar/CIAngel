#pragma once
#include <fstream>
#include <3ds.h>

#include "data.h"
#include "utils2.h"

const u16 tmd_top = 0x140;

extern void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath);
extern void InstallTicket(std::string FullPath);

