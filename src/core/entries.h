#ifndef AARAMBH_CORE_ENTRIES_H
#define AARAMBH_CORE_ENTRIES_H

#include "aarambh/entry.h"

#define ENTRY_FIELD_COUNT 6

typedef struct {
    Entry items[AARAMBH_MAX_ENTRIES];
    int count;
} EntryList;

void entries_add(EntryList *list, char **fields, int field_count);
void entries_add_on(EntryList *list, EFI_HANDLE device, char **fields, int field_count);
void entries_add_line(EntryList *list, char *line);
void entries_add_actions(EntryList *list);

int entries_has_system(const EntryList *list);
int entries_uses_directory(const EntryList *list, const char *directory);
void entries_move_actions_last(EntryList *list);

#endif
