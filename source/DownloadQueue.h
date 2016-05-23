#pragma once
#include "common.h"
#include <3ds.h>
#include <vector>
#include <fstream>
#include <malloc.h>
#include <unistd.h>

#include "config.h"
#include "utils2.h"
#include "utils.h"
#include "ticket.h"
#include "cia.h"

enum queue_item_status { queued, downloading, done };

typedef struct {
  int score;
  int index;
  std::string titleid;
  std::string titlekey;
  std::string name;
  bool installed;
  std::string norm_name;
  std::string region;
  std::string code;
} game_item;

extern std::vector<game_item> game_queue;

extern void ProcessGameQueue();
extern Result DownloadTitle(std::string titleId, std::string encTitleKey, std::string titleName);
