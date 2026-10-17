/*
 * *** NOT COMPILE-TESTED IN THIS SANDBOX *** — see twrp_tar.h for why.
 * Written against libarchive's documented, long-stable API. Compile this
 * against a real libarchive-dev before trusting it (e.g. `apt-get install
 * libarchive-dev` and add it to the project's CI build step, same as
 * build-boot-image.yml already does for other tools).
 */

#include "twrp_tar.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>
#include <ftw.h>

/* ---- create ------------------------------------------------------- */

static struct archive *g_write_archive = NULL;
static const char *g_source_dir_prefix = NULL;

static int add_file_to_archive(const char *fpath, const struct stat *sb,
                                int typeflag, struct FTW *ftwbuf) {
    (void)typeflag;

    /* path inside the archive, relative to source_dir */
    const char *rel_path = fpath + ftwbuf->base - ftwbuf->level; /* approximate; corrected below */
    rel_path = fpath + strlen(g_source_dir_prefix);
    while (*rel_path == '/') rel_path++;
    if (*rel_path == '\0') return 0; /* skip the root dir entry itself */

    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, rel_path);
    archive_entry_copy_stat(entry, sb);

    if (archive_write_header(g_write_archive, entry) != ARCHIVE_OK) {
        fprintf(stderr, "twrp_tar: archive_write_header failed for %s: %s\n",
                rel_path, archive_error_string(g_write_archive));
        archive_entry_free(entry);
        return -1;
    }

    if (S_ISREG(sb->st_mode)) {
        FILE *f = fopen(fpath, "rb");
        if (!f) {
            fprintf(stderr, "twrp_tar: could not open %s for reading\n", fpath);
            archive_entry_free(entry);
            return -1;
        }
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            if (archive_write_data(g_write_archive, buf, n) < 0) {
                fprintf(stderr, "twrp_tar: archive_write_data failed: %s\n",
                        archive_error_string(g_write_archive));
                fclose(f);
                archive_entry_free(entry);
                return -1;
            }
        }
        fclose(f);
    }

    archive_entry_free(entry);
    return 0;
}

bool cf_tar_create(const char *source_dir, const char *out_tar_path) {
    struct archive *a = archive_write_new();
    archive_write_set_format_pax_restricted(a); /* same format class TWRP uses */
    archive_write_add_filter_gzip(a);

    if (archive_write_open_filename(a, out_tar_path) != ARCHIVE_OK) {
        fprintf(stderr, "twrp_tar: could not open %s for writing: %s\n",
                out_tar_path, archive_error_string(a));
        archive_write_free(a);
        return false;
    }

    g_write_archive = a;
    g_source_dir_prefix = source_dir;

    /* nftw walks the tree; FTW_PHYS avoids following symlinks (matches
     * TWRP's default behavior of backing up the link itself, not its
     * target — important for things like /vendor symlinked elsewhere). */
    int rc = nftw(source_dir, add_file_to_archive, 16, FTW_PHYS);

    g_write_archive = NULL;
    g_source_dir_prefix = NULL;

    archive_write_close(a);
    archive_write_free(a);

    return rc == 0;
}

/* ---- extract -------------------------------------------------------- */

bool cf_tar_extract(const char *tar_path, const char *dest_dir) {
    struct archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);

    if (archive_read_open_filename(a, tar_path, 65536) != ARCHIVE_OK) {
        fprintf(stderr, "twrp_tar: could not open %s for reading: %s\n",
                tar_path, archive_error_string(a));
        archive_read_free(a);
        return false;
    }

    struct archive *ext = archive_write_disk_new();
    archive_write_disk_set_options(ext,
        ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
        ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);

    struct archive_entry *entry;
    bool ok = true;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir,
                 archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, full_path);

        if (archive_write_header(ext, entry) != ARCHIVE_OK) {
            fprintf(stderr, "twrp_tar: extract header failed for %s: %s\n",
                    full_path, archive_error_string(ext));
            ok = false;
            continue;
        }

        const void *buf;
        size_t size;
        int64_t offset;
        int r;
        while ((r = archive_read_data_block(a, &buf, &size, &offset)) == ARCHIVE_OK) {
            if (archive_write_data_block(ext, buf, size, offset) != ARCHIVE_OK) {
                fprintf(stderr, "twrp_tar: write_data_block failed: %s\n",
                        archive_error_string(ext));
                ok = false;
                break;
            }
        }
        if (r != ARCHIVE_EOF && r != ARCHIVE_OK) ok = false;

        archive_write_finish_entry(ext);
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return ok;
}
