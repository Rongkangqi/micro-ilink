#ifndef ILINK_PROTOCOL_H
#define ILINK_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>

#include "config.h"

typedef struct IlinkHttpClient IlinkHttpClient;

#define ILINK_APP_ID "wx_bot_c"
#define ILINK_API_VERSION "1.0.0"

#define MESSAGE_TYPE_TEXT 1
#define MESSAGE_TYPE_IMAGE 2
#define MESSAGE_TYPE_VOICE 3
#define MESSAGE_TYPE_FILE 4
#define MESSAGE_TYPE_VIDEO 5

#define MESSAGE_TYPE_USER 1
#define MESSAGE_TYPE_BOT 2

#define MESSAGE_STATE_FINISH 2

#define MEDIA_TYPE_IMAGE 1
#define MEDIA_TYPE_VIDEO 2
#define MEDIA_TYPE_FILE 3
#define MEDIA_TYPE_VOICE 4

typedef enum {
    FILTER_ALL = 0,
    FILTER_TEXT = 1,
    FILTER_IMAGE = 2,
    FILTER_FILE = 4,
    FILTER_VIDEO = 5,
    FILTER_VOICE = 3
} IlinkFilterType;

typedef enum {
    ILINK_ERR_OK = 0,
    ILINK_ERR_NETWORK = -1,
    ILINK_ERR_PROTOCOL = -2,
    ILINK_ERR_CRYPTO = -3,
    ILINK_ERR_PARSE = -4,
    ILINK_ERR_PARAM = -5,
    ILINK_ERR_UPLOAD = -6,
    ILINK_ERR_SEND = -7,
    ILINK_ERR_TIMEOUT = -8
} IlinkError;

typedef struct TextItem {
    char *text;
} TextItem;

typedef struct MediaItem {
    char *encrypt_query_param;
    char *aes_key;
    int64_t mid_size;
    int64_t video_size;
    char *file_name;
    int64_t file_size;
} MediaItem;

typedef struct MessageItem {
    int type;
    union {
        TextItem text_item;
        MediaItem media_item;
    } content;
} MessageItem;

typedef struct WeixinMessage {
    int64_t message_id;
    char from_user_id[256];
    char to_user_id[256];
    int message_type;
    int message_state;
    MessageItem items[16];
    int item_count;
    char context_token[512];
    char client_id[64];
} WeixinMessage;

typedef struct UploadedMedia {
    char *filekey;
    char *download_encrypted_query_param;
    char *aeskey_hex;
    int64_t file_size;
    int64_t file_size_ciphertext;
} UploadedMedia;

typedef struct GetUploadUrlRequest {
    char *filekey;
    int media_type;
    char *to_user_id;
    int64_t rawsize;
    char *rawfilemd5;
    int64_t filesize;
    int no_need_thumb;
    char *aeskey;
} GetUploadUrlRequest;

typedef struct GetUploadUrlResponse {
    char *upload_param;
    char *upload_full_url;
    char *encrypt_query_param;
} GetUploadUrlResponse;

typedef struct GetUpdatesResponse {
    WeixinMessage *msgs;
    int msg_count;
    char *get_updates_buf;
} GetUpdatesResponse;

void ilink_message_init(WeixinMessage *msg);
void ilink_message_free(WeixinMessage *msg);
void ilink_uploaded_media_free(UploadedMedia *media);
void ilink_get_upload_url_resp_free(GetUploadUrlResponse *resp);
void ilink_get_updates_resp_free(GetUpdatesResponse *resp);

IlinkError ilink_get_updates(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *buf, GetUpdatesResponse **resp);

IlinkError ilink_send_message(IlinkHttpClient *client, const BotConfig *cfg,
                               const WeixinMessage *msg);

IlinkError ilink_get_upload_url(IlinkHttpClient *client, const BotConfig *cfg,
                                  const GetUploadUrlRequest *req,
                                  GetUploadUrlResponse **resp);

IlinkError ilink_send_typing(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *ilink_user_id, const char *typing_ticket,
                              int status);

IlinkError ilink_get_config(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *ilink_user_id, char **typing_ticket);

char *ilink_build_send_message_json(const WeixinMessage *msg);
char *ilink_build_get_updates_json(const char *buf);
char *ilink_build_get_upload_url_json(const GetUploadUrlRequest *req);
char *ilink_build_send_typing_json(const char *ilink_user_id,
                                     const char *typing_ticket, int status);
char *ilink_build_get_config_json(const char *ilink_user_id);

WeixinMessage *ilink_parse_get_updates_response(const char *json, int *count,
                                                  char **next_buf);
GetUploadUrlResponse *ilink_parse_get_upload_url_response(const char *json);

#endif /* ILINK_PROTOCOL_H */
