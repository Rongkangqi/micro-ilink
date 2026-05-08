#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

char *ilink_generate_uuid(void) {
    char *uuid = malloc(37);
    if (!uuid) return NULL;

    snprintf(uuid, 37, "%08x-%04x-%04x-%04x-%012x",
             (unsigned int)time(NULL),
             (unsigned int)(rand() & 0xffff),
             (unsigned int)(rand() & 0xffff),
             (unsigned int)((rand() & 0x0fff) | 0x4000),
             (unsigned int)(((long)rand() << 32) | rand()));
    return uuid;
}

char *ilink_generate_client_id(void) {
    char *uuid = ilink_generate_uuid();
    if (!uuid) return NULL;

    char *client_id = malloc(32);
    if (!client_id) {
        free(uuid);
        return NULL;
    }
    snprintf(client_id, 32, "wechat-bot-%.12s", uuid);
    free(uuid);
    return client_id;
}

char *ilink_base64_encode(const unsigned char *data, size_t len) {
    BIO *b64_filter, *file_sink;
    FILE *fp;
    char *result;
    size_t result_len;

    fp = open_memstream(&result, &result_len);
    if (!fp) return NULL;

    file_sink = BIO_new_fp(fp, BIO_CLOSE);
    b64_filter = BIO_new(BIO_f_base64());
    // Chain: b64_filter -> file_sink  (encode, then write to memstream)
    BIO *chain = BIO_push(b64_filter, file_sink);

    BIO_set_flags(chain, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(chain, data, len);
    BIO_flush(chain);

    // Ensure null termination in the memstream buffer
    fputc('\0', fp);
    fflush(fp);

    BIO_free_all(chain);
    return result;
}

char *ilink_md5_file(const char *file_path) {
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_CTX ctx;
    FILE *f;
    unsigned char buf[8192];
    size_t n;

    f = fopen(file_path, "rb");
    if (!f) return NULL;

    MD5_Init(&ctx);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        MD5_Update(&ctx, buf, n);
    }
    MD5_Final(result, &ctx);
    fclose(f);

    char *hex = malloc(MD5_DIGEST_LENGTH * 2 + 1);
    if (!hex) return NULL;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", result[i]);
    }
    hex[MD5_DIGEST_LENGTH * 2] = '\0';
    return hex;
}

char *ilink_md5_string(const char *str) {
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5((const unsigned char *)str, strlen(str), result);

    char *hex = malloc(MD5_DIGEST_LENGTH * 2 + 1);
    if (!hex) return NULL;

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", result[i]);
    }
    hex[MD5_DIGEST_LENGTH * 2] = '\0';
    return hex;
}

char *ilink_hex_encode(const unsigned char *data, size_t len) {
    char *hex = malloc(len * 2 + 1);
    if (!hex) return NULL;

    for (size_t i = 0; i < len; i++) {
        sprintf(hex + i * 2, "%02x", data[i]);
    }
    hex[len * 2] = '\0';
    return hex;
}
