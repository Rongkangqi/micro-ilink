#include "cdn.h"
#include "crypto.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <curl/curl.h>

#define CDN_UPLOAD_URL "https://novac2c.cdn.weixin.qq.com/c2c/upload"

struct IlinkCdnClient {
    IlinkHttpClient *http;
};

IlinkCdnClient *ilink_cdn_client_new(IlinkHttpClient *http) {
    if (!http) return NULL;
    IlinkCdnClient *client = calloc(1, sizeof(IlinkCdnClient));
    if (!client) return NULL;
    client->http = http;
    return client;
}

void ilink_cdn_client_free(IlinkCdnClient *client) {
    free(client);
}

static char *url_encode(const char *str) {
    if (!str) return strdup("");

    static const char hex_chars[] = "0123456789ABCDEF";
    char *result = malloc(strlen(str) * 3 + 1);
    char *p = result;

    while (*str) {
        if (isalnum((unsigned char)*str) || *str == '-' || *str == '_' ||
            *str == '.' || *str == '~') {
            *p++ = *str;
        } else {
            *p++ = '%';
            *p++ = hex_chars[((unsigned char)*str) >> 4];
            *p++ = hex_chars[((unsigned char)*str) & 0x0F];
        }
        str++;
    }
    *p = '\0';
    return result;
}

IlinkError ilink_cdn_build_upload_url(const char *base_url, const char *upload_param,
                                       const char *filekey, char **url) {
    if (!base_url || !upload_param || !filekey || !url) {
        return ILINK_ERR_PARAM;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return ILINK_ERR_PARAM;

    char *enc_param = curl_easy_escape(curl, upload_param, strlen(upload_param));
    char *enc_filekey = curl_easy_escape(curl, filekey, strlen(filekey));
    curl_easy_cleanup(curl);

    if (!enc_param || !enc_filekey) {
        if (enc_param) curl_free(enc_param);
        if (enc_filekey) curl_free(enc_filekey);
        return ILINK_ERR_PARAM;
    }

    size_t result_size = strlen(base_url) + 32 + strlen(enc_param) + 12 + strlen(enc_filekey) + 8;
    char *result = malloc(result_size);
    if (!result) {
        curl_free(enc_param);
        curl_free(enc_filekey);
        return ILINK_ERR_PARAM;
    }

    snprintf(result, result_size, "%s?encrypted_query_param=%s&filekey=%s",
             base_url, enc_param, enc_filekey);

    curl_free(enc_param);
    curl_free(enc_filekey);
    *url = result;
    return ILINK_ERR_OK;
}

static char *generate_hex_random(size_t len) {
    char *result = malloc(len + 1);
    if (!result) return NULL;

    for (size_t i = 0; i < len; i++) {
        snprintf(result + i * 2, 3, "%02x", rand() & 0xFF);
    }
    result[len * 2] = '\0';
    return result;
}

IlinkError ilink_cdn_upload_media(IlinkCdnClient *cdn, const BotConfig *cfg,
                                   const char *file_path, int media_type,
                                   const char *to_user_id,
                                   UploadedMedia **result) {
    if (!cdn || !cfg || !file_path || !to_user_id || !result) {
        return ILINK_ERR_PARAM;
    }

    FILE *f = fopen(file_path, "rb");
    if (!f) return ILINK_ERR_PARAM;
    fseek(f, 0, SEEK_END);
    long raw_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (raw_size <= 0) {
        fclose(f);
        return ILINK_ERR_PARAM;
    }

    unsigned char *plaintext = malloc(raw_size);
    if (!plaintext) {
        fclose(f);
        return ILINK_ERR_PARAM;
    }

    fread(plaintext, 1, raw_size, f);
    fclose(f);

    char *raw_md5 = ilink_md5_string((const char *)plaintext);
    fprintf(stderr, "[CDN] raw_md5=%s\n", raw_md5);
    if (!raw_md5) {
        free(plaintext);
        return ILINK_ERR_CRYPTO;
    }

    char *filekey = generate_hex_random(16);
    fprintf(stderr, "[CDN] filekey=%s at %p\n", filekey, (void*)filekey);
    if (!filekey) {
        free(plaintext);
        free(raw_md5);
        return ILINK_ERR_CRYPTO;
    }

    unsigned char *aes_key = malloc(16);
    fprintf(stderr, "[CDN] aes_key at %p\n", (void*)aes_key);
    if (!aes_key) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        return ILINK_ERR_CRYPTO;
    }

    for (int i = 0; i < 16; i++) {
        aes_key[i] = rand() & 0xFF;
    }

    size_t padded_size = aes_ecb_padded_size(raw_size);
    fprintf(stderr, "[CDN] padded_size=%zu\n", padded_size);
    unsigned char *ciphertext = malloc(padded_size);
    fprintf(stderr, "[CDN] ciphertext at %p\n", (void*)ciphertext);
    if (!ciphertext) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        return ILINK_ERR_CRYPTO;
    }

    size_t cipher_len = 0;
    fprintf(stderr, "[CDN] calling aes_ecb_encrypt\n");
    if (aes_ecb_encrypt(plaintext, raw_size, aes_key, ciphertext, &cipher_len) != 0) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        free(ciphertext);
        return ILINK_ERR_CRYPTO;
    }
    fprintf(stderr, "[CDN] cipher_len=%zu\n", cipher_len);

    GetUploadUrlRequest req;
    memset(&req, 0, sizeof(req));
    req.filekey = filekey;
    req.media_type = media_type;
    req.to_user_id = (char *)to_user_id;
    req.rawsize = raw_size;
    req.rawfilemd5 = raw_md5;
    req.filesize = (int64_t)cipher_len;
    req.no_need_thumb = 1;
    req.aeskey = ilink_hex_encode(aes_key, 16);

    GetUploadUrlResponse *url_resp = NULL;
    fprintf(stderr, "[CDN] cdn->http = %p, calling ilink_get_upload_url\n", (void*)cdn->http);
    IlinkError err = ilink_get_upload_url(cdn->http, cfg, &req, &url_resp);

    fprintf(stderr, "[CDN] ilink_get_upload_url returned, err=%d, url_resp=%p\n", err, (void*)url_resp);

    free(req.aeskey);
    if (url_resp) {
        DEBUG_printf("upload_param=%s, upload_full_url=%s, encrypt_query_param=%s\n",
                     url_resp->upload_param ? url_resp->upload_param : "null",
                     url_resp->upload_full_url ? url_resp->upload_full_url : "null",
                     url_resp->encrypt_query_param ? url_resp->encrypt_query_param : "null");
    }

    if (err != ILINK_ERR_OK) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        free(ciphertext);
        return err;
    }

    char *upload_url = NULL;

    if (!url_resp->upload_full_url && url_resp->upload_param) {
        err = ilink_cdn_build_upload_url(CDN_UPLOAD_URL, url_resp->upload_param,
                                          filekey, &upload_url);
        if (err != ILINK_ERR_OK) {
            free(plaintext);
            free(raw_md5);
            free(filekey);
            free(aes_key);
            free(ciphertext);
            ilink_get_upload_url_resp_free(url_resp);
            return err;
        }
    }

    IlinkHttpResponse *http_resp = NULL;
    const char *upload_url_to_use = url_resp->upload_full_url ? url_resp->upload_full_url : upload_url;
    if (!upload_url_to_use) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        free(ciphertext);
        ilink_get_upload_url_resp_free(url_resp);
        return ILINK_ERR_UPLOAD;
    }
    DEBUG_printf("Uploading to CDN: %s (%zu bytes)\n", upload_url_to_use, cipher_len);

    struct curl_slist *upload_headers = NULL;
    upload_headers = curl_slist_append(upload_headers, "Content-Type: application/octet-stream");

    int ret = ilink_http_post_binary_with_headers(cdn->http, upload_url_to_use, ciphertext, cipher_len,
                                                  upload_headers, &http_resp);
    curl_slist_free_all(upload_headers);

    free(upload_url);
    free(ciphertext);

    if (ret != 0 || !http_resp) {
        fprintf(stderr, "[CDN] CDN upload HTTP request failed (ret=%d, resp=%p)\n", ret, (void*)http_resp);
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        ilink_get_upload_url_resp_free(url_resp);
        return ILINK_ERR_UPLOAD;
    }

    fprintf(stderr, "[CDN] CDN upload HTTP response: status=%d\n", http_resp->status_code);
    fprintf(stderr, "[CDN] CDN x-encrypted-param header: %s\n",
            http_resp->header_x_encrypted_param ? http_resp->header_x_encrypted_param : "(missing)");

    UploadedMedia *media = calloc(1, sizeof(UploadedMedia));
    if (!media) {
        free(plaintext);
        free(raw_md5);
        free(filekey);
        free(aes_key);
        ilink_http_response_free(http_resp);
        ilink_get_upload_url_resp_free(url_resp);
        return ILINK_ERR_UPLOAD;
    }

    media->filekey = filekey;
    media->file_size = raw_size;
    media->file_size_ciphertext = cipher_len;
    media->aeskey_hex = ilink_hex_encode(aes_key, 16);
    // Use CDN header's x-encrypted-param as download_encrypted_query_param
    // This is the authoritative value returned by CDN after successful upload
    if (http_resp->header_x_encrypted_param && strlen(http_resp->header_x_encrypted_param) > 0) {
        media->download_encrypted_query_param = strdup(http_resp->header_x_encrypted_param);
        fprintf(stderr, "[CDN] download_encrypted_query_param captured (%zu chars)\n",
                strlen(media->download_encrypted_query_param));
    } else {
        media->download_encrypted_query_param = NULL;
        fprintf(stderr, "[CDN] WARNING: no x-encrypted-param in CDN response!\n");
    }

    *result = media;

    free(plaintext);
    free(raw_md5);
    free(aes_key);
    ilink_http_response_free(http_resp);
    ilink_get_upload_url_resp_free(url_resp);

    return ILINK_ERR_OK;
}
