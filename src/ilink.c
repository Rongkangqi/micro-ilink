#include "ilink.h"
#include "http.h"
#include "config.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *build_base_info_json(void) {
    return strdup("{\"channel_version\":\"1.0.0\"}");
}

static char *get_wechat_uin(void) {
    uint32_t raw = ((uint32_t)rand() << 16) | ((uint32_t)rand() & 0xFFFF);
    char num[16];
    snprintf(num, sizeof(num), "%u", raw);
    return ilink_base64_encode((unsigned char *)num, strlen(num));
}

static const char **build_common_headers(const BotConfig *cfg, int *count) {
    static const char *headers[16];
    static char auth_header[1024];
    *count = 0;

    char *uin = get_wechat_uin();

    headers[(*count)++] = "iLink-App-Id";
    headers[(*count)++] = ILINK_APP_ID;
    headers[(*count)++] = "iLink-App-ClientVersion";
    headers[(*count)++] = "256";
    headers[(*count)++] = "AuthorizationType";
    headers[(*count)++] = "ilink_bot_token";
    headers[(*count)++] = "X-WECHAT-UIN";
    headers[(*count)++] = uin;

    snprintf(auth_header, sizeof(auth_header), "Bearer %s", cfg->token);
    headers[(*count)++] = "Authorization";
    headers[(*count)++] = auth_header;

    return headers;
}

static IlinkHttpResponse *do_post(IlinkHttpClient *client, const BotConfig *cfg,
                                   const char *endpoint, const char *body) {
    char url[512];
    int base_len = strlen(cfg->base_url);
    if (base_len > 0 && cfg->base_url[base_len - 1] == '/') {
        snprintf(url, sizeof(url), "%s%s", cfg->base_url, endpoint);
    } else {
        snprintf(url, sizeof(url), "%s/%s", cfg->base_url, endpoint);
    }

    DEBUG_printf("POST %s\n", url);
    DEBUG_printf("Body: %s\n", body);

    int header_count;
    const char **headers = build_common_headers(cfg, &header_count);

    DEBUG_printf("Calling ilink_http_post_json...\n");

    IlinkHttpResponse *resp = NULL;
    int ret = ilink_http_post_json(client, url, body, headers, &resp);

    DEBUG_printf("ilink_http_post_json returned, ret=%d, resp=%p\n", ret, (void*)resp);

    DEBUG_printf("HTTP Response status: %d\n", resp ? resp->status_code : -1);
    if (resp && resp->body) {
        DEBUG_printf("Response body: %s\n", resp->body);
    }

    free((void *)headers[7]);

    if (ret != 0 || !resp) return NULL;
    return resp;
}

char *ilink_build_send_message_json(const WeixinMessage *msg) {
    char *items_json = malloc(1);
    items_json[0] = '\0';

    for (int i = 0; i < msg->item_count && i < 16; i++) {
        char item[2048];  // Increased from 512 to handle larger JSON
        MessageItem *item_ptr = &msg->items[i];

        if (item_ptr->type == MESSAGE_TYPE_TEXT) {
            snprintf(item, sizeof(item),
                     "{\"type\":1,\"text_item\":{\"text\":\"%s\"}}",
                     item_ptr->content.text_item.text);
        } else if (item_ptr->type == MESSAGE_TYPE_IMAGE) {
            snprintf(item, sizeof(item),
                     "{\"type\":2,\"image_item\":{\"media\":{\"encrypt_query_param\":\"%s\",\"aes_key\":\"%s\",\"encrypt_type\":1},\"mid_size\":%ld}}",
                     item_ptr->content.media_item.encrypt_query_param ? item_ptr->content.media_item.encrypt_query_param : "",
                     item_ptr->content.media_item.aes_key ? item_ptr->content.media_item.aes_key : "",
                     (long)item_ptr->content.media_item.mid_size);
        } else if (item_ptr->type == MESSAGE_TYPE_VIDEO) {
            snprintf(item, sizeof(item),
                     "{\"type\":5,\"video_item\":{\"media\":{\"encrypt_query_param\":\"%s\",\"aes_key\":\"%s\",\"encrypt_type\":1},\"mid_size\":%ld,\"video_size\":%ld}}",
                     item_ptr->content.media_item.encrypt_query_param ? item_ptr->content.media_item.encrypt_query_param : "",
                     item_ptr->content.media_item.aes_key ? item_ptr->content.media_item.aes_key : "",
                     (long)item_ptr->content.media_item.mid_size,
                     (long)item_ptr->content.media_item.video_size);
        } else if (item_ptr->type == MESSAGE_TYPE_FILE) {
            snprintf(item, sizeof(item),
                     "{\"type\":4,\"file_item\":{\"media\":{\"encrypt_query_param\":\"%s\",\"aes_key\":\"%s\",\"encrypt_type\":1},\"mid_size\":%ld,\"file_name\":\"%s\",\"len\":\"%ld\"}}",
                     item_ptr->content.media_item.encrypt_query_param ? item_ptr->content.media_item.encrypt_query_param : "",
                     item_ptr->content.media_item.aes_key ? item_ptr->content.media_item.aes_key : "",
                     (long)item_ptr->content.media_item.mid_size,
                     item_ptr->content.media_item.file_name ? item_ptr->content.media_item.file_name : "",
                     (long)item_ptr->content.media_item.file_size);
        }

        size_t new_len = strlen(items_json) + strlen(item) + 2;
        char *new_json = realloc(items_json, new_len);
        if (!new_json) { free(items_json); return NULL; }
        items_json = new_json;

        if (strlen(items_json) > 1) strcat(items_json, ",");
        strcat(items_json, item);
    }

    char *base_info = build_base_info_json();
    int has_context = msg->context_token && strlen(msg->context_token) > 0;

    size_t result_size = 1024 + strlen(items_json) + strlen(msg->client_id) +
                         strlen(msg->to_user_id);
    if (has_context) result_size += strlen(msg->context_token);
    char *result = malloc(result_size);
    if (!result) { free(items_json); free(base_info); return NULL; }

    if (has_context) {
        snprintf(result, result_size,
                 "{\"msg\":{\"from_user_id\":\"\",\"to_user_id\":\"%s\",\"client_id\":\"%s\","
                 "\"message_type\":%d,\"message_state\":%d,\"item_list\":[%s],"
                 "\"context_token\":\"%s\"},\"base_info\":%s}",
                 msg->to_user_id, msg->client_id, msg->message_type, msg->message_state,
                 items_json, msg->context_token, base_info);
    } else {
        snprintf(result, result_size,
                 "{\"msg\":{\"from_user_id\":\"\",\"to_user_id\":\"%s\",\"client_id\":\"%s\","
                 "\"message_type\":%d,\"message_state\":%d,\"item_list\":[%s]"
                 "},\"base_info\":%s}",
                 msg->to_user_id, msg->client_id, msg->message_type, msg->message_state,
                 items_json, base_info);
    }

    free(items_json);
    free(base_info);
    return result;
}

char *ilink_build_get_updates_json(const char *buf) {
    char *result;

    if (buf && strlen(buf) > 0) {
        result = malloc(256 + strlen(buf));
        snprintf(result, 512,
                 "{\"get_updates_buf\":\"%s\",\"base_info\":{\"channel_version\":\"1.0.0\"}}",
                 buf);
    } else {
        result = malloc(256);
        snprintf(result, 256,
                 "{\"get_updates_buf\":\"\",\"base_info\":{\"channel_version\":\"1.0.0\"}}");
    }

    return result;
}

char *ilink_build_get_upload_url_json(const GetUploadUrlRequest *req) {
    char *base_info = build_base_info_json();
    char *result = malloc(1024);

    snprintf(result, 1024,
             "{\"filekey\":\"%s\",\"media_type\":%d,\"to_user_id\":\"%s\","
             "\"rawsize\":%ld,\"rawfilemd5\":\"%s\",\"filesize\":%ld,"
             "\"no_need_thumb\":%s,\"aeskey\":\"%s\",\"base_info\":%s}",
             req->filekey, req->media_type, req->to_user_id,
             (long)req->rawsize, req->rawfilemd5, (long)req->filesize,
             req->no_need_thumb ? "true" : "false", req->aeskey, base_info);

    free(base_info);
    return result;
}

char *ilink_build_send_typing_json(const char *ilink_user_id,
                                     const char *typing_ticket, int status) {
    char *base_info = build_base_info_json();
    char *result = malloc(512);

    snprintf(result, 512,
             "{\"ilink_user_id\":\"%s\",\"typing_ticket\":\"%s\",\"status\":%d,\"base_info\":%s}",
             ilink_user_id, typing_ticket, status, base_info);

    free(base_info);
    return result;
}

char *ilink_build_get_config_json(const char *ilink_user_id) {
    char *base_info = build_base_info_json();
    char *result = malloc(512);

    snprintf(result, 512,
             "{\"ilink_user_id\":\"%s\",\"base_info\":%s}",
             ilink_user_id, base_info);

    free(base_info);
    return result;
}

static const char *json_get_string(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *start = strstr(json, pattern);
    if (!start) return NULL;
    start += strlen(pattern);

    while (*start == ' ' || *start == '\t' || *start == ':') start++;
    if (*start != '"') return NULL;
    start++;

    const char *end = start;
    while (*end && *end != '"') {
        if (*end == '\\') end++;
        end++;
    }

    size_t len = end - start;
    char *result = malloc(len + 1);
    if (!result) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (start[i] == '\\' && i + 1 < len) {
            result[j++] = start[++i];
        } else {
            result[j++] = start[i];
        }
    }
    result[j] = '\0';
    return result;
}

static long json_get_long(const char *json, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *start = strstr(json, pattern);
    if (!start) return 0;
    start += strlen(pattern);

    while (*start == ' ' || *start == '\t' || *start == ':') start++;
    while (*start == '"') start++;

    return strtol(start, NULL, 10);
}

GetUploadUrlResponse *ilink_parse_get_upload_url_response(const char *json) {
    if (!json) return NULL;

    GetUploadUrlResponse *resp = calloc(1, sizeof(GetUploadUrlResponse));
    if (!resp) return NULL;

    resp->upload_param = json_get_string(json, "upload_param");
    resp->upload_full_url = json_get_string(json, "upload_full_url");
    resp->encrypt_query_param = json_get_string(json, "encrypt_query_param");

    return resp;
}

void ilink_get_upload_url_resp_free(GetUploadUrlResponse *resp) {
    if (!resp) return;
    free(resp->upload_param);
    free(resp->upload_full_url);
    free(resp->encrypt_query_param);
    free(resp);
}

IlinkError ilink_get_updates(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *buf, GetUpdatesResponse **resp) {
    char *body = ilink_build_get_updates_json(buf ? buf : "");
    if (!body) return ILINK_ERR_PARSE;

    IlinkHttpResponse *http_resp = do_post(client, cfg, "ilink/bot/getupdates", body);
    free(body);

    if (!http_resp) return ILINK_ERR_NETWORK;

    if (http_resp->status_code != 200) {
        ilink_http_response_free(http_resp);
        return ILINK_ERR_PROTOCOL;
    }

    *resp = calloc(1, sizeof(GetUpdatesResponse));
    if (!*resp) {
        ilink_http_response_free(http_resp);
        return ILINK_ERR_PARSE;
    }

    (*resp)->get_updates_buf = json_get_string(http_resp->body, "get_updates_buf");

    DEBUG_printf("Parsing get_updates response, buf=%s\n",
                 (*resp)->get_updates_buf ? (*resp)->get_updates_buf : "null");

    const char *msgs_start = strstr(http_resp->body, "\"msgs\":[");
    DEBUG_printf("msgs_start found: %s\n", msgs_start ? "yes" : "no");
    if (msgs_start) {
        msgs_start += 8;
        if (*msgs_start == '[') msgs_start++;
        if (*msgs_start == ']') {
            (*resp)->msg_count = 0;
            (*resp)->msgs = NULL;
        } else {
            (*resp)->msgs = calloc(32, sizeof(WeixinMessage));
            int count = 0;
            const char *p = msgs_start;

            while (*p && *p != ']' && count < 32) {
                ilink_message_init(&(*resp)->msgs[count]);

                const char *msg_start = p;
                while (*msg_start == '{') msg_start++;

                char *from = json_get_string(msg_start, "from_user_id");
                if (from) { strncpy((*resp)->msgs[count].from_user_id, from, 255); free(from); }

                char *to = json_get_string(msg_start, "to_user_id");
                if (to) { strncpy((*resp)->msgs[count].to_user_id, to, 255); free(to); }

                char *client_id = json_get_string(msg_start, "client_id");
                if (client_id) { strncpy((*resp)->msgs[count].client_id, client_id, 63); free(client_id); }

                char *ctx = json_get_string(msg_start, "context_token");
                if (ctx) { strncpy((*resp)->msgs[count].context_token, ctx, 511); free(ctx); }

                (*resp)->msgs[count].message_id = json_get_long(msg_start, "message_id");
                (*resp)->msgs[count].message_type = (int)json_get_long(msg_start, "message_type");
                (*resp)->msgs[count].message_state = (int)json_get_long(msg_start, "message_state");

                const char *items_start = strstr(msg_start, "\"item_list\":[");
                if (items_start) {
                    items_start += 12;
                    int item_idx = 0;
                    const char *ip = items_start;

                    while (*ip && *ip != ']' && item_idx < 16) {
                        while (*ip == '{') ip++;
                        if (*ip == '}') { ip++; continue; }
                        if (*ip == ',') { ip++; continue; }
                        if (*ip == ']') break;

                        int type = (int)json_get_long(ip, "type");
                        (*resp)->msgs[count].items[item_idx].type = type;

                        if (type == MESSAGE_TYPE_TEXT) {
                            char *text = json_get_string(ip, "text");
                            if (text) {
                                (*resp)->msgs[count].items[item_idx].content.text_item.text = text;
                            }
                        } else {
                            char *enc = json_get_string(ip, "encrypt_query_param");
                            char *aes = json_get_string(ip, "aes_key");
                            long mid_size = json_get_long(ip, "mid_size");
                            long video_size = json_get_long(ip, "video_size");

                            if (enc) (*resp)->msgs[count].items[item_idx].content.media_item.encrypt_query_param = enc;
                            if (aes) (*resp)->msgs[count].items[item_idx].content.media_item.aes_key = aes;
                            (*resp)->msgs[count].items[item_idx].content.media_item.mid_size = mid_size;
                            (*resp)->msgs[count].items[item_idx].content.media_item.video_size = video_size;
                        }

                        item_idx++;
                        while (*ip && *ip != '}' && *ip != ']') ip++;
                        if (*ip == '}') ip++;
                    }
                    (*resp)->msgs[count].item_count = item_idx;
                }

                count++;
                while (*p && *p != '}') p++;
                if (*p == '}') p++;
                while (*p && (*p == ',' || *p == ' ')) p++;
            }
            (*resp)->msg_count = count;
        }
    }

    ilink_http_response_free(http_resp);
    return ILINK_ERR_OK;
}

IlinkError ilink_send_message(IlinkHttpClient *client, const BotConfig *cfg,
                               const WeixinMessage *msg) {
    char *body = ilink_build_send_message_json(msg);
    if (!body) return ILINK_ERR_PARSE;

    IlinkHttpResponse *http_resp = do_post(client, cfg, "ilink/bot/sendmessage", body);
    free(body);

    if (!http_resp) return ILINK_ERR_NETWORK;

    int status = http_resp->status_code;
    int ret_code = 0;
    if (http_resp->body) {
        char *ret_start = strstr(http_resp->body, "\"ret\":");
        if (ret_start) {
            ret_code = atoi(ret_start + 6);
        }
    }
    ilink_http_response_free(http_resp);

    if (status != 200 || ret_code != 0) return ILINK_ERR_SEND;
    return ILINK_ERR_OK;
}

IlinkError ilink_get_upload_url(IlinkHttpClient *client, const BotConfig *cfg,
                                  const GetUploadUrlRequest *req,
                                  GetUploadUrlResponse **resp) {
    char *body = ilink_build_get_upload_url_json(req);
    if (!body) return ILINK_ERR_PARSE;

    IlinkHttpResponse *http_resp = do_post(client, cfg, "ilink/bot/getuploadurl", body);
    free(body);

    if (!http_resp) return ILINK_ERR_NETWORK;

    if (http_resp->status_code != 200) {
        fprintf(stderr, "[DEBUG] getuploadurl status=%d, body=%s\n",
                http_resp->status_code, http_resp->body ? http_resp->body : "null");
        ilink_http_response_free(http_resp);
        return ILINK_ERR_UPLOAD;
    }

    fprintf(stderr, "[DEBUG] getuploadurl response body: %s\n", http_resp->body);

    *resp = ilink_parse_get_upload_url_response(http_resp->body);
    ilink_http_response_free(http_resp);

    if (!*resp) return ILINK_ERR_PARSE;
    return ILINK_ERR_OK;
}

IlinkError ilink_send_typing(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *ilink_user_id, const char *typing_ticket,
                              int status) {
    char *body = ilink_build_send_typing_json(ilink_user_id, typing_ticket, status);
    if (!body) return ILINK_ERR_PARSE;

    IlinkHttpResponse *http_resp = do_post(client, cfg, "ilink/bot/sendtyping", body);
    free(body);

    if (!http_resp) return ILINK_ERR_NETWORK;

    int resp_status = http_resp->status_code;
    ilink_http_response_free(http_resp);

    return resp_status == 200 ? ILINK_ERR_OK : ILINK_ERR_SEND;
}

IlinkError ilink_get_config(IlinkHttpClient *client, const BotConfig *cfg,
                              const char *ilink_user_id, char **typing_ticket) {
    char *body = ilink_build_get_config_json(ilink_user_id);
    if (!body) return ILINK_ERR_PARSE;

    IlinkHttpResponse *http_resp = do_post(client, cfg, "ilink/bot/getconfig", body);
    free(body);

    if (!http_resp) return ILINK_ERR_NETWORK;

    if (http_resp->status_code != 200) {
        ilink_http_response_free(http_resp);
        return ILINK_ERR_PROTOCOL;
    }

    *typing_ticket = json_get_string(http_resp->body, "typing_ticket");
    ilink_http_response_free(http_resp);

    return ILINK_ERR_OK;
}

void ilink_message_init(WeixinMessage *msg) {
    if (!msg) return;
    memset(msg, 0, sizeof(WeixinMessage));
    msg->message_type = MESSAGE_TYPE_BOT;
    msg->message_state = MESSAGE_STATE_FINISH;
}

void ilink_message_free(WeixinMessage *msg) {
    if (!msg) return;
    for (int i = 0; i < msg->item_count && i < 16; i++) {
        if (msg->items[i].type == MESSAGE_TYPE_TEXT) {
            free(msg->items[i].content.text_item.text);
        } else {
            free(msg->items[i].content.media_item.encrypt_query_param);
            free(msg->items[i].content.media_item.aes_key);
            free(msg->items[i].content.media_item.file_name);
        }
    }
}

void ilink_uploaded_media_free(UploadedMedia *media) {
    if (!media) return;
    free(media->filekey);
    free(media->download_encrypted_query_param);
    free(media->aeskey_hex);
    free(media);
}

void ilink_get_updates_resp_free(GetUpdatesResponse *resp) {
    if (!resp) return;
    if (resp->msgs) {
        for (int i = 0; i < resp->msg_count; i++) {
            ilink_message_free(&resp->msgs[i]);
        }
        free(resp->msgs);
    }
    free(resp->get_updates_buf);
    free(resp);
}
