#include "ota_verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/wait.h>

#define EOCD_SIG_SIZE 4
static const unsigned char EOCD_SIGNATURE[EOCD_SIG_SIZE] = {0x50, 0x4b, 0x05, 0x06};
static const unsigned char FOOTER_MAGIC[2] = {0xff, 0xff};

static void set_error(char *error_out, int cap, const char *msg) {
    if (error_out && cap > 0) snprintf(error_out, (size_t)cap, "%s", msg);
}

static unsigned char *read_whole_file(const char *path, long *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);

    unsigned char *buf = malloc((size_t)size);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size_out = size;
    return buf;
}

static long find_eocd(const unsigned char *data, long size) {
    long search_start = size - 65557;
    if (search_start < 0) search_start = 0;
    for (long i = size - EOCD_SIG_SIZE; i >= search_start; i--) {
        if (memcmp(data + i, EOCD_SIGNATURE, EOCD_SIG_SIZE) == 0) return i;
    }
    return -1;
}

static uint16_t read_u16le(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Extracts signed_data (ptr+len into `data`, NOT a copy) and sig_der
 * (ptr+len into `data`, NOT a copy) — both point into the caller's
 * buffer, valid as long as `data` is alive. */
static bool extract_signature(const unsigned char *data, long size,
                               const unsigned char **signed_data, long *signed_data_len,
                               const unsigned char **sig_der, long *sig_der_len,
                               char *error_out, int error_out_cap) {
    long eocd = find_eocd(data, size);
    if (eocd < 0) {
        set_error(error_out, error_out_cap, "not a valid zip (EOCD not found)");
        return false;
    }

    uint16_t comment_len = read_u16le(data + eocd + 20);
    long comment_start = eocd + 22;
    if (comment_start + comment_len > size) {
        set_error(error_out, error_out_cap, "malformed EOCD comment length");
        return false;
    }

    if (comment_len < 4) {
        set_error(error_out, error_out_cap, "no signature footer present (comment too short)");
        return false;
    }

    const unsigned char *comment = data + comment_start;
    const unsigned char *magic = comment + comment_len - 2;
    if (memcmp(magic, FOOTER_MAGIC, 2) != 0) {
        set_error(error_out, error_out_cap, "no signature footer present (bad magic)");
        return false;
    }

    uint16_t sig_size = read_u16le(comment + comment_len - 4);
    if (sig_size > comment_len - 4) {
        set_error(error_out, error_out_cap, "malformed footer: declared signature size exceeds comment length");
        return false;
    }

    *sig_der = comment + comment_len - 4 - sig_size;
    *sig_der_len = sig_size;
    *signed_data = data;
    *signed_data_len = comment_start;
    return true;
}

static bool write_buf_to_file(const char *path, const unsigned char *buf, long len) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(buf, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    return ok;
}

/* Runs a shell command, returns its exit code (0 = success), optionally
 * capturing stdout into `out` (may be NULL). */
static int run_capture(const char *cmd, char *out, int out_cap) {
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    if (out && out_cap > 0) {
        out[0] = '\0';
        char line[512];
        while (fgets(line, sizeof(line), p)) {
            strncat(out, line, (size_t)(out_cap - (int)strlen(out) - 1));
        }
    } else {
        char discard[512];
        while (fgets(discard, sizeof(discard), p)) { /* drain */ }
    }
    int status = pclose(p);
    return WEXITSTATUS(status);
}

static bool get_cert_fingerprint(const char *openssl_path, const char *cert_path,
                                  char *out, int out_cap) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s x509 -in '%s' -noout -fingerprint -sha256 2>/dev/null",
             openssl_path, cert_path);
    char buf[512];
    int rc = run_capture(cmd, buf, sizeof(buf));
    if (rc != 0 || buf[0] == '\0') return false;
    snprintf(out, (size_t)out_cap, "%s", buf);
    return true;
}

static bool get_embedded_signer_fingerprint(const char *openssl_path, const char *sig_der_path,
                                             char *out, int out_cap, char *tmp_cert_path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s pkcs7 -inform DER -in '%s' -print_certs > '%s' 2>/dev/null",
             openssl_path, sig_der_path, tmp_cert_path);
    if (system(cmd) != 0) return false;

    FILE *check = fopen(tmp_cert_path, "r");
    if (!check) return false;
    fseek(check, 0, SEEK_END);
    long sz = ftell(check);
    fclose(check);
    if (sz <= 0) return false;

    return get_cert_fingerprint(openssl_path, tmp_cert_path, out, out_cap);
}

bool cf_ota_verify_zip(const char *zip_path, const char *trusted_cert_pem_path,
                        const cf_ota_verify_options_t *options,
                        char *error_out, int error_out_cap) {
    cf_ota_verify_options_t default_opts = { .require_chain = true, .openssl_path = "openssl" };
    if (!options) options = &default_opts;

    long size = 0;
    unsigned char *data = read_whole_file(zip_path, &size);
    if (!data) {
        set_error(error_out, error_out_cap, "could not read zip file");
        return false;
    }

    const unsigned char *signed_data, *sig_der;
    long signed_data_len, sig_der_len;
    if (!extract_signature(data, size, &signed_data, &signed_data_len,
                            &sig_der, &sig_der_len, error_out, error_out_cap)) {
        free(data);
        return false;
    }

    char data_path[] = "/tmp/cf_ota_verify_data_XXXXXX";
    char sig_path[]  = "/tmp/cf_ota_verify_sig_XXXXXX";
    char cert_path[] = "/tmp/cf_ota_verify_cert_XXXXXX";
    int data_fd = mkstemp(data_path);
    int sig_fd = mkstemp(sig_path);
    int cert_fd = mkstemp(cert_path);
    bool ok = false;

    if (data_fd < 0 || sig_fd < 0 || cert_fd < 0) {
        set_error(error_out, error_out_cap, "could not create temp files");
        goto cleanup;
    }
    close(data_fd); close(sig_fd); close(cert_fd);

    if (!write_buf_to_file(data_path, signed_data, signed_data_len) ||
        !write_buf_to_file(sig_path, sig_der, sig_der_len)) {
        set_error(error_out, error_out_cap, "could not write temp files");
        goto cleanup;
    }

    {
        char expected_fp[512], actual_fp[512];
        if (!get_cert_fingerprint(options->openssl_path, trusted_cert_pem_path, expected_fp, sizeof(expected_fp))) {
            set_error(error_out, error_out_cap, "could not read trusted cert fingerprint");
            goto cleanup;
        }
        if (!get_embedded_signer_fingerprint(options->openssl_path, sig_path, actual_fp, sizeof(actual_fp), cert_path)) {
            set_error(error_out, error_out_cap, "could not extract embedded signer cert");
            goto cleanup;
        }
        if (strcmp(expected_fp, actual_fp) != 0) {
            set_error(error_out, error_out_cap, "signer certificate does not match expected cert");
            goto cleanup;
        }
    }

    {
        char cmd[2048];
        if (options->require_chain) {
            snprintf(cmd, sizeof(cmd),
                     "%s smime -verify -in '%s' -inform DER -content '%s' -CAfile '%s' -out /dev/null 2>/dev/null",
                     options->openssl_path, sig_path, data_path, trusted_cert_pem_path);
        } else {
            snprintf(cmd, sizeof(cmd),
                     "%s smime -verify -in '%s' -inform DER -content '%s' -noverify -certfile '%s' -out /dev/null 2>/dev/null",
                     options->openssl_path, sig_path, data_path, trusted_cert_pem_path);
        }
        int rc = system(cmd);
        if (rc != 0) {
            set_error(error_out, error_out_cap, "openssl signature verification failed");
            goto cleanup;
        }
    }

    ok = true;

cleanup:
    if (data_fd >= 0) unlink(data_path);
    if (sig_fd >= 0) unlink(sig_path);
    if (cert_fd >= 0) unlink(cert_path);
    free(data);
    return ok;
}
