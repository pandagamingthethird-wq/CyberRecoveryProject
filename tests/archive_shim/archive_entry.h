#ifndef ARCHIVE_ENTRY_SHIM_H
#define ARCHIVE_ENTRY_SHIM_H
#include <sys/stat.h>

struct archive_entry;

struct archive_entry *archive_entry_new(void);
void archive_entry_set_pathname(struct archive_entry *, const char *);
void archive_entry_copy_stat(struct archive_entry *, const struct stat *);
void archive_entry_free(struct archive_entry *);
const char *archive_entry_pathname(struct archive_entry *);

#endif
