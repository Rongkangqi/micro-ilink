#ifndef ILINK_HTTP_H
#define ILINK_HTTP_H

#include <stddef.h>
#include <stdint.h>

struct curl_slist;

typedef struct IlinkHttpResponse {
    int status_code;
    char *body;
    size_t body_len;
    char *header_x_encrypted_param;
} IlinkHttpResponse;

typedef struct IlinkHttpHeader {
    char *key;
    char *value;
    struct IlinkHttpHeader *next;
} IlinkHttpHeader;

typedef struct IlinkHttpClient IlinkHttpClient;

IlinkHttpClient *ilink_http_client_new(void);
void ilink_http_client_free(IlinkHttpClient *client);

int ilink_http_post_json(IlinkHttpClient *client, const char *url,
                         const char *body, const char **headers,
                         IlinkHttpResponse **response);

int ilink_http_post_binary(IlinkHttpClient *client, const char *url,
                           const uint8_t *data, size_t data_len,
                           const char **headers, IlinkHttpResponse **response);

int ilink_http_post_binary_with_headers(IlinkHttpClient *client, const char *url,
                                        const uint8_t *data, size_t data_len,
                                        struct curl_slist *headers,
                                        IlinkHttpResponse **response);

void ilink_http_response_free(IlinkHttpResponse *resp);

#endif /* ILINK_HTTP_H */
