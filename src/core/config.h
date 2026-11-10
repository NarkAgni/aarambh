#ifndef AARAMBH_CORE_CONFIG_H
#define AARAMBH_CORE_CONFIG_H

#include "core/entries.h"

#define CONFIG_FILE_NAME "aarambh.conf"
#define CONFIG_NAME_MAX 128
#define CONFIG_DEFAULT_MAX 64

typedef struct {
    int timeout_seconds;
    int default_index;
    int utc_offset_minutes;
    int autodetect;
    int allow_edit;
    int write_combining;
    char theme[32];
    char default_entry[CONFIG_DEFAULT_MAX];
    char font_light[CONFIG_NAME_MAX];
    char font_regular[CONFIG_NAME_MAX];
    char font_bold[CONFIG_NAME_MAX];
    char font_mono[CONFIG_NAME_MAX];
} Config;

void config_load(Config *config, EntryList *entries);

#endif