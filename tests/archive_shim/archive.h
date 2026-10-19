/* FALLBACK ONLY. Minimal shim matching libarchive's real, stable public
 * ABI (just the handful of functions twrp_tar.c calls) — used ONLY when
 * the real libarchive-dev package (archive.h/archive_entry.h) isn't
 * installed, so tests/test_twrp_tar.sh can still compile+link+run
 * against the actual installed libarchive runtime library instead of
 * leaving twrp_tar.c completely unverified in a dev environment without
 * network access to install -dev packages.
 *
 * tests/test_twrp_tar.sh always prefers the REAL system archive.h when
 * it's available (e.g. in CI, which installs libarchive-dev) — this
 * shim is the fallback, not the primary path. If libarchive's real ABI
 * ever changes in a way that breaks this shim, that's a sign this
 * fallback path needs updating, not that twrp_tar.c itself is wrong.
 */
#ifndef ARCHIVE_SHIM_H
#define ARCHIVE_SHIM_H
#include <stddef.h>
#include <stdint.h>

struct archive;
struct archive_entry; /* forward-declared here so archive.h's own
                          prototypes resolve before archive_entry.h
                          (which defines the struct's real users) is
                          necessarily included — matches real libarchive's
                          archive.h, which does the same forward decl. */

#define ARCHIVE_EOF    1
#define ARCHIVE_OK     0
#define ARCHIVE_RETRY  (-10)
#define ARCHIVE_WARN   (-20)
#define ARCHIVE_FAILED (-25)
#define ARCHIVE_FATAL  (-30)

#define ARCHIVE_EXTRACT_OWNER  0x0001
#define ARCHIVE_EXTRACT_PERM   0x0002
#define ARCHIVE_EXTRACT_TIME   0x0004
#define ARCHIVE_EXTRACT_ACL    0x0020
#define ARCHIVE_EXTRACT_FFLAGS 0x0040

struct archive *archive_write_new(void);
int archive_write_set_format_pax_restricted(struct archive *);
int archive_write_add_filter_gzip(struct archive *);
int archive_write_open_filename(struct archive *, const char *);
int archive_write_close(struct archive *);
int archive_write_free(struct archive *);
const char *archive_error_string(struct archive *);
int archive_write_header(struct archive *, struct archive_entry *);
ssize_t archive_write_data(struct archive *, const void *, size_t);

struct archive *archive_read_new(void);
int archive_read_support_filter_gzip(struct archive *);
int archive_read_support_format_tar(struct archive *);
int archive_read_open_filename(struct archive *, const char *, size_t);
int archive_read_next_header(struct archive *, struct archive_entry **);
int archive_read_data_block(struct archive *, const void **, size_t *, int64_t *);
int archive_read_close(struct archive *);
int archive_read_free(struct archive *);

struct archive *archive_write_disk_new(void);
int archive_write_disk_set_options(struct archive *, int);
int archive_write_data_block(struct archive *, const void *, size_t, int64_t);
int archive_write_finish_entry(struct archive *);

#endif
