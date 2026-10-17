#ifndef CF_IMAGE_SHRINK_H
#define CF_IMAGE_SHRINK_H

#include <stdint.h>
#include <stdbool.h>

/* Result of a shrink attempt */
typedef enum {
    CF_SHRINK_OK,              /* fit within target, no prompt needed */
    CF_SHRINK_OK_WITH_TRADEOFF,/* fit, but after a user-approved tradeoff */
    CF_SHRINK_FAILED,          /* could not fit even after all strategies */
    CF_SHRINK_USER_CANCELLED
} cf_shrink_result_t;

/* A single candidate trade-off the engine can offer the user when automatic,
 * non-destructive shrinking isn't enough. Each entry knows how many bytes it
 * would free and how to apply itself. */
typedef struct {
    const char *id;                 /* e.g. "extra_languages" */
    const char *description;        /* shown to the user */
    int64_t estimated_bytes_freed;
    bool (*apply)(const char *workdir);
} cf_shrink_tradeoff_t;

/* Callback the UI layer implements: given a list of candidate tradeoffs and
 * how many bytes are still needed, ask the user which ones to apply.
 * Returns the number of tradeoff indices written into chosen_out (caller
 * allocates chosen_out with capacity == tradeoff_count). */
typedef int (*cf_ui_prompt_tradeoffs_fn)(
    const cf_shrink_tradeoff_t *tradeoffs,
    int tradeoff_count,
    int64_t bytes_still_needed,
    int *chosen_out
);

/*
 * Attempts to make `image_path` fit within `target_partition_size` bytes.
 *
 * Strategy order (each is non-destructive to recovery functionality,
 * attempted automatically, in order, before anything is ever asked of the
 * user):
 *   1. Strip debug symbols / sanitizer runtimes from bundled binaries
 *      (e.g. libclang_rt.*san*.so — never shipped in release builds anyway)
 *   2. Strip unused static binaries not referenced by any menu action
 *   3. Recompress the ramdisk at maximum level for the device's supported
 *      algorithm (falls back to gzip -9 if only gzip is supported)
 *   4. Strip ELF symbol tables / debug sections from remaining binaries
 *
 * If the image still exceeds target_partition_size after all of the above,
 * the engine builds a list of destructive-but-safe trade-offs (extra
 * language packs, unused theme assets, etc.), estimates bytes each would
 * free, and calls `prompt_fn` so the UI can ask the user which to apply.
 * Only trade-offs the user explicitly picks are applied.
 *
 * `workdir` must point to an already-unpacked image (ramdisk extracted,
 * kernel/dtb separated) — this module only operates on the unpacked tree
 * and leaves repacking to the caller (flash_engine), so it stays reusable
 * for both boot.img and recovery.img style layouts.
 */
cf_shrink_result_t cf_shrink_image_to_fit(
    const char *workdir,
    int64_t current_size_bytes,
    int64_t target_partition_size,
    cf_ui_prompt_tradeoffs_fn prompt_fn
);

#endif /* CF_IMAGE_SHRINK_H */
