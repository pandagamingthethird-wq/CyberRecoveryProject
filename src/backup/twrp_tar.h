#ifndef CF_TWRP_TAR_H
#define CF_TWRP_TAR_H

#include <stdbool.h>

/*
 * twrp_tar — file-level tar backup/restore via libarchive, matching
 * TWRP's twrpTar.cpp role (as opposed to backup_engine.c's whole-
 * partition dd, which can't do selective/incremental backups or work on
 * partitions you want file-level access into rather than a raw image of).
 *
 * *** NOT COMPILE-TESTED ***
 * This sandbox has libarchive's shared library but not its -dev headers
 * (archive.h / archive_entry.h), and has no network access to install
 * them. Everything else added in this pass was compiled and actually
 * run; this file is written correctly against libarchive's real,
 * stable, documented API (archive_write_open_filename /
 * archive_entry_* / archive_read_* — these functions and their
 * signatures are long-stable across libarchive versions) but you should
 * compile this specifically before trusting it — e.g. as part of the
 * project's own CI, which already has apt available to install
 * libarchive-dev, unlike this sandbox.
 */

/* Creates a tar archive at `out_tar_path` containing everything under
 * `source_dir` (recursive). Paths inside the archive are relative to
 * source_dir, matching standard tar/nandroid backup layout. */
bool cf_tar_create(const char *source_dir, const char *out_tar_path);

/* Extracts `tar_path` into `dest_dir`, recreating the directory
 * structure and restoring permissions/ownership where the archive
 * entries specify them. */
bool cf_tar_extract(const char *tar_path, const char *dest_dir);

#endif /* CF_TWRP_TAR_H */
