#include "http.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

struct IlinkHttpClient {
    CURL *curl;
    char error_buf[CURL_ERROR_SIZE];
};

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} WriteBuffer;

typedef struct {
    const char *data;
    size_t size;
    size_t pos;
} ReadBuffer;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    WriteBuffer *buf = (WriteBuffer *)userp;

    if (buf->size + realsize > buf->capacity) {
        size_t new_cap = buf->capacity == 0 ? 4096 : buf->capacity * 2;
        while (buf->size + realsize > new_cap) new_cap *= 2;
        char *new_data = realloc(buf->data, new_cap);
        if (!new_data) return 0;
        buf->data = new_data;
        buf->capacity = new_cap;
    }

    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    return realsize;
}

static size_t header_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char *header = (char *)contents;

    if (realsize >= 18) {
        size_t header_len = realsize;
        while (header_len > 0 && (header[header_len - 1] == '\r' || header[header_len - 1] == '\n')) {
            header_len--;
        }

        char save = header[header_len];
        header[header_len] = '\0';

        char *colon = strchr(header, ':');
        if (colon) {
            *colon = '\0';
            char *header_name = header;
            while (*header_name && (*header_name == ' ' || *header_name == '\t')) header_name++;

            if (strcasecmp(header_name, "x-encrypted-param") == 0 ||
                strcasecmp(header_name, "x-encryptedparam") == 0) {
                IlinkHttpResponse *resp = (IlinkHttpResponse *)userp;
                char *value = colon + 1;
                while (*value == ' ' || *value == '\t') value++;
                size_t value_len = header_len - (value - header);
                if (value_len > 0 && !resp->header_x_encrypted_param) {
                    resp->header_x_encrypted_param = malloc(value_len + 1);
                    if (resp->header_x_encrypted_param) {
                        memcpy(resp->header_x_encrypted_param, value, value_len);
                        resp->header_x_encrypted_param[value_len] = '\0';
                    }
                }
            }
        }
        header[header_len] = save;
    }
    return realsize;
}

static size_t read_callback(void *dest, size_t size, size_t nmemb, void *userp) {
    ReadBuffer *buf = (ReadBuffer *)userp;
    size_t to_copy = (buf->size - buf->pos < size * nmemb) ?
                     (buf->size - buf->pos) : (size * nmemb);
    if (to_copy > 0) {
        memcpy(dest, buf->data + buf->pos, to_copy);
        buf->pos += to_copy;
    }
    return to_copy;
}

IlinkHttpClient *ilink_http_client_new(void) {
    static int global_init_done = 0;
    if (!global_init_done) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        global_init_done = 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    IlinkHttpClient *client = calloc(1, sizeof(IlinkHttpClient));
    if (!client) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    client->curl = curl;
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, client->error_buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    return client;
}

void ilink_http_client_free(IlinkHttpClient *client) {
    if (!client) return;
    if (client->curl) curl_easy_cleanup(client->curl);
    free(client);
}

static void free_headers(struct curl_slist *headers) {
    curl_slist_free_all(headers);
}

int ilink_http_post_json(IlinkHttpClient *client, const char *url,
                         const char *body, const char **headers_in,
                         IlinkHttpResponse **response) {
    if (!client || !url || !body || !response) return -1;

    WriteBuffer write_buf = {0};
    IlinkHttpResponse *resp = calloc(1, sizeof(IlinkHttpResponse));
    if (!resp) return -1;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (headers_in) {
        for (int i = 0; headers_in[i]; i += 2) {
            char header_line[1024];
            snprintf(header_line, sizeof(header_line), "%s: %s", headers_in[i], headers_in[i + 1]);
            headers = curl_slist_append(headers, header_line);
        }
    }

    curl_easy_reset(client->curl);
    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &write_buf);
    curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, resp);
    curl_easy_setopt(client->curl, CURLOPT_ERRORBUFFER, client->error_buf);

    CURLcode code = curl_easy_perform(client->curl);

    free_headers(headers);

    if (code != CURLE_OK) {
        free(write_buf.data);
        free(resp);
        return -1;
    }

    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &resp->status_code);

    resp->body = write_buf.data;
    resp->body_len = write_buf.size;
    resp->body = realloc(resp->body, write_buf.size + 1);
    if (resp->body) resp->body[write_buf.size] = '\0';
    *response = resp;

    return 0;
}

int ilink_http_post_binary(IlinkHttpClient *client, const char *url,
                           const uint8_t *data, size_t data_len,
                           const char **headers_in, IlinkHttpResponse **response) {
    if (!client || !url || !data || !response) return -1;

    WriteBuffer write_buf = {0};
    IlinkHttpResponse *resp = calloc(1, sizeof(IlinkHttpResponse));
    if (!resp) return -1;

    ReadBuffer read_buf = {(const char *)data, data_len, 0};

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    if (headers_in) {
        for (int i = 0; headers_in[i]; i += 2) {
            headers = curl_slist_append(headers, headers_in[i]);
            headers = curl_slist_append(headers, headers_in[i + 1]);
        }
    }

    curl_easy_reset(client->curl);

    curl_easy_setopt(client->curl, CURLOPT_URL, url);
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE, data_len);
    curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(client->curl, CURLOPT_READDATA, &read_buf);
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &write_buf);
    curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, resp);
    curl_easy_setopt(client->curl, CURLOPT_ERRORBUFFER, client->error_buf);

    CURLcode code = curl_easy_perform(client->curl);

    free_headers(headers);

    if (code != CURLE_OK) {
        free(write_buf.data);
        free(resp);
        return -1;
    }

    curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &resp->status_code);

    resp->body = write_buf.data;
    resp->body_len = write_buf.size;
    resp->body = realloc(resp->body, write_buf.size + 1);
    if (resp->body) resp->body[write_buf.size] = '\0';
    *response = resp;

    return 0;
}

int ilink_http_post_binary_with_headers(IlinkHttpClient *client, const char *url,
                                         const uint8_t *data, size_t data_len,
                                         struct curl_slist *headers,
                                         IlinkHttpResponse **response) {
    if (!client || !url || !data || !response) return -1;

    WriteBuffer write_buf = {0};
    IlinkHttpResponse *resp = calloc(1, sizeof(IlinkHttpResponse));
    if (!resp) return -1;

    CURL *curl = curl_easy_init();
    if (!curl) {
        free(resp);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data_len);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    CURLcode code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->status_code);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        free(write_buf.data);
        free(resp);
        return -1;
    }

    resp->body = write_buf.data;
    resp->body_len = write_buf.size;
    resp->body = realloc(resp->body, write_buf.size + 1);
    if (resp->body) resp->body[write_buf.size] = '\0';
    *response = resp;

    return 0;
}

void ilink_http_response_free(IlinkHttpResponse *resp) {
    if (!resp) return;
    free(resp->body);
    free(resp->header_x_encrypted_param);
    free(resp);
}
