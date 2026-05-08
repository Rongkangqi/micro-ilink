#include "bot.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

IlinkBot *ilink_bot_new(const BotConfig *cfg) {
    if (!cfg) return NULL;

    IlinkBot *bot = calloc(1, sizeof(IlinkBot));
    if (!bot) return NULL;

    memcpy(&bot->config, cfg, sizeof(BotConfig));
    bot->running = 0;
    bot->handler_count = 0;
    bot->get_updates_buf = NULL;

    bot->http = ilink_http_client_new();
    if (!bot->http) {
        free(bot);
        return NULL;
    }

    bot->cdn = ilink_cdn_client_new(bot->http);
    if (!bot->cdn) {
        ilink_http_client_free(bot->http);
        free(bot);
        return NULL;
    }

    srand((unsigned int)time(NULL) ^ (unsigned long)bot);

    return bot;
}

void ilink_bot_free(IlinkBot *bot) {
    if (!bot) return;

    if (bot->cdn) ilink_cdn_client_free(bot->cdn);
    if (bot->http) ilink_http_client_free(bot->http);
    free(bot->get_updates_buf);
    free(bot);
}

int ilink_bot_on_message(IlinkBot *bot, IlinkFilterType filter,
                         IlinkMessageHandler handler, void *user_data) {
    if (!bot || !handler) return -1;
    if (bot->handler_count >= ILINK_BOT_MAX_HANDLERS) return -1;

    bot->handlers[bot->handler_count].filter = filter;
    bot->handlers[bot->handler_count].handler = handler;
    bot->handlers[bot->handler_count].user_data = user_data;
    bot->handler_count++;

    return 0;
}

static void dispatch_message(IlinkBot *bot, const WeixinMessage *msg) {
    for (int i = 0; i < bot->handler_count; i++) {
        IlinkHandlerEntry *entry = &bot->handlers[i];

        if (entry->filter == FILTER_ALL) {
            entry->handler(bot, msg, entry->user_data);
            continue;
        }

        if (msg->item_count > 0) {
            int msg_type = msg->items[0].type;
            if ((int)entry->filter == msg_type) {
                entry->handler(bot, msg, entry->user_data);
            }
        }
    }
}

void ilink_bot_run_once(IlinkBot *bot) {
    if (!bot) return;

    DEBUG_printf("Calling get_updates...\n");

    GetUpdatesResponse *resp = NULL;
    IlinkError err = ilink_get_updates(bot->http, &bot->config,
                                        bot->get_updates_buf, &resp);

    if (err != ILINK_ERR_OK) {
        DEBUG_printf("get_updates error: %d\n", err);
        if (err == ILINK_ERR_NETWORK) {
            usleep(100000);
        }
        return;
    }

    DEBUG_printf("get_updates returned %d messages\n", resp->msg_count);

    free(bot->get_updates_buf);
    bot->get_updates_buf = resp->get_updates_buf;
    resp->get_updates_buf = NULL;

    for (int i = 0; i < resp->msg_count; i++) {
        DEBUG_printf("Dispatching message %d: from=%s, item_count=%d\n",
                     i, resp->msgs[i].from_user_id, resp->msgs[i].item_count);
        dispatch_message(bot, &resp->msgs[i]);
    }

    ilink_get_updates_resp_free(resp);
}

void ilink_bot_run(IlinkBot *bot) {
    if (!bot) return;
    bot->running = 1;

    while (bot->running) {
        ilink_bot_run_once(bot);
    }
}

void ilink_bot_stop(IlinkBot *bot) {
    if (!bot) return;
    bot->running = 0;
}

static IlinkError send_media_message(IlinkBot *bot, const char *to_user,
                                      const char *file_path, int media_type) {
    if (!bot || !to_user || !file_path) return ILINK_ERR_PARAM;

    int media_type_for_api;
    if (media_type == MESSAGE_TYPE_IMAGE) media_type_for_api = MEDIA_TYPE_IMAGE;
    else if (media_type == MESSAGE_TYPE_VIDEO) media_type_for_api = MEDIA_TYPE_VIDEO;
    else if (media_type == MESSAGE_TYPE_FILE) media_type_for_api = MEDIA_TYPE_FILE;
    else if (media_type == MESSAGE_TYPE_VOICE) media_type_for_api = MEDIA_TYPE_VOICE;
    else media_type_for_api = media_type;

    UploadedMedia *media = NULL;
    IlinkError err = ilink_cdn_upload_media(bot->cdn, &bot->config,
                                             file_path, media_type_for_api, to_user, &media);
    if (err != ILINK_ERR_OK || !media) return err;

    WeixinMessage msg;
    ilink_message_init(&msg);

    strncpy(msg.to_user_id, to_user, sizeof(msg.to_user_id) - 1);
    msg.message_type = MESSAGE_TYPE_BOT;
    msg.message_state = MESSAGE_STATE_FINISH;

    char *client_id = ilink_generate_client_id();
    if (client_id) {
        strncpy(msg.client_id, client_id, sizeof(msg.client_id) - 1);
        free(client_id);
    }

    msg.items[0].type = media_type;
    msg.items[0].content.media_item.encrypt_query_param = media->download_encrypted_query_param ? strdup(media->download_encrypted_query_param) : NULL;
    // Base64-encode the hex string directly (matching Python's behavior)
    if (media->aeskey_hex) {
        msg.items[0].content.media_item.aes_key =
            ilink_base64_encode((const unsigned char *)media->aeskey_hex, strlen(media->aeskey_hex));
    } else {
        msg.items[0].content.media_item.aes_key = NULL;
    }
    // Use file_size_ciphertext (encrypted size) for mid_size, not file_size (plaintext size)
    msg.items[0].content.media_item.mid_size = media->file_size_ciphertext;
    msg.items[0].content.media_item.file_size = media->file_size;
    if (media_type == MESSAGE_TYPE_VIDEO) {
        msg.items[0].content.media_item.video_size = media->file_size_ciphertext;
    }
    // Extract basename for file type
    if (media_type == MESSAGE_TYPE_FILE && file_path) {
        const char *basename = strrchr(file_path, '/');
        msg.items[0].content.media_item.file_name = strdup(basename ? basename + 1 : file_path);
    } else {
        msg.items[0].content.media_item.file_name = NULL;
    }
    msg.item_count = 1;

    err = ilink_send_message(bot->http, &bot->config, &msg);

    free(msg.items[0].content.media_item.encrypt_query_param);
    free(msg.items[0].content.media_item.aes_key);
    free(msg.items[0].content.media_item.file_name);
    free(media);
    return err;
}

IlinkError ilink_bot_send_text(IlinkBot *bot, const char *to_user, const char *text) {
    if (!bot || !to_user || !text) return ILINK_ERR_PARAM;

    WeixinMessage msg;
    ilink_message_init(&msg);

    strncpy(msg.to_user_id, to_user, sizeof(msg.to_user_id) - 1);
    msg.message_type = MESSAGE_TYPE_BOT;
    msg.message_state = MESSAGE_STATE_FINISH;

    char *client_id = ilink_generate_client_id();
    if (client_id) {
        strncpy(msg.client_id, client_id, sizeof(msg.client_id) - 1);
        free(client_id);
    }

    msg.items[0].type = MESSAGE_TYPE_TEXT;
    msg.items[0].content.text_item.text = strdup(text);
    msg.item_count = 1;

    IlinkError err = ilink_send_message(bot->http, &bot->config, &msg);

    free(msg.items[0].content.text_item.text);
    return err;
}

IlinkError ilink_bot_send_image(IlinkBot *bot, const char *to_user, const char *file_path) {
    return send_media_message(bot, to_user, file_path, MESSAGE_TYPE_IMAGE);
}

IlinkError ilink_bot_send_video(IlinkBot *bot, const char *to_user, const char *file_path) {
    return send_media_message(bot, to_user, file_path, MESSAGE_TYPE_VIDEO);
}

IlinkError ilink_bot_send_file(IlinkBot *bot, const char *to_user, const char *file_path) {
    return send_media_message(bot, to_user, file_path, MESSAGE_TYPE_FILE);
}

IlinkError ilink_bot_reply_text(IlinkBot *bot, const WeixinMessage *msg, const char *text) {
    if (!bot || !msg || !text) return ILINK_ERR_PARAM;

    WeixinMessage reply;
    ilink_message_init(&reply);

    strncpy(reply.to_user_id, msg->from_user_id, sizeof(reply.to_user_id) - 1);
    strncpy(reply.context_token, msg->context_token, sizeof(reply.context_token) - 1);
    reply.message_type = MESSAGE_TYPE_BOT;
    reply.message_state = MESSAGE_STATE_FINISH;

    char *client_id = ilink_generate_client_id();
    if (client_id) {
        strncpy(reply.client_id, client_id, sizeof(reply.client_id) - 1);
        free(client_id);
    }

    reply.items[0].type = MESSAGE_TYPE_TEXT;
    reply.items[0].content.text_item.text = strdup(text);
    reply.item_count = 1;

    IlinkError err = ilink_send_message(bot->http, &bot->config, &reply);

    free(reply.items[0].content.text_item.text);
    return err;
}

static IlinkError reply_media_message(IlinkBot *bot, const WeixinMessage *msg,
                                       const char *file_path, int media_type) {
    if (!bot || !msg || !file_path) return ILINK_ERR_PARAM;

    UploadedMedia *media = NULL;
    IlinkError err = ilink_cdn_upload_media(bot->cdn, &bot->config,
                                             file_path, media_type,
                                             msg->from_user_id, &media);
    if (err != ILINK_ERR_OK || !media) return err;

    WeixinMessage reply;
    ilink_message_init(&reply);

    strncpy(reply.to_user_id, msg->from_user_id, sizeof(reply.to_user_id) - 1);
    strncpy(reply.context_token, msg->context_token, sizeof(reply.context_token) - 1);
    reply.message_type = MESSAGE_TYPE_BOT;
    reply.message_state = MESSAGE_STATE_FINISH;

    char *client_id = ilink_generate_client_id();
    if (client_id) {
        strncpy(reply.client_id, client_id, sizeof(reply.client_id) - 1);
        free(client_id);
    }

    reply.items[0].type = media_type;
    reply.items[0].content.media_item.encrypt_query_param = media->download_encrypted_query_param ? strdup(media->download_encrypted_query_param) : NULL;
    // Base64-encode the hex string directly (matching Python's behavior)
    if (media->aeskey_hex) {
        reply.items[0].content.media_item.aes_key =
            ilink_base64_encode((const unsigned char *)media->aeskey_hex, strlen(media->aeskey_hex));
    } else {
        reply.items[0].content.media_item.aes_key = NULL;
    }
    // Use file_size_ciphertext (encrypted size) for mid_size, not file_size (plaintext size)
    reply.items[0].content.media_item.mid_size = media->file_size_ciphertext;
    reply.items[0].content.media_item.file_size = media->file_size;
    if (media_type == MESSAGE_TYPE_VIDEO) {
        reply.items[0].content.media_item.video_size = media->file_size_ciphertext;
    }
    // Extract basename for file type
    if (media_type == MESSAGE_TYPE_FILE && file_path) {
        const char *basename = strrchr(file_path, '/');
        reply.items[0].content.media_item.file_name = strdup(basename ? basename + 1 : file_path);
    } else {
        reply.items[0].content.media_item.file_name = NULL;
    }
    reply.item_count = 1;

    err = ilink_send_message(bot->http, &bot->config, &reply);

    free(reply.items[0].content.media_item.encrypt_query_param);
    free(reply.items[0].content.media_item.aes_key);
    free(reply.items[0].content.media_item.file_name);
    free(media);
    return err;
}

IlinkError ilink_bot_reply_image(IlinkBot *bot, const WeixinMessage *msg, const char *file_path) {
    return reply_media_message(bot, msg, file_path, MESSAGE_TYPE_IMAGE);
}

IlinkError ilink_bot_reply_video(IlinkBot *bot, const WeixinMessage *msg, const char *file_path) {
    return reply_media_message(bot, msg, file_path, MESSAGE_TYPE_VIDEO);
}

IlinkError ilink_bot_reply_file(IlinkBot *bot, const WeixinMessage *msg, const char *file_path) {
    return reply_media_message(bot, msg, file_path, MESSAGE_TYPE_FILE);
}

const char *ilink_error_str(IlinkError err) {
    switch (err) {
        case ILINK_ERR_OK: return "OK";
        case ILINK_ERR_NETWORK: return "Network error";
        case ILINK_ERR_PROTOCOL: return "Protocol error";
        case ILINK_ERR_CRYPTO: return "Crypto error";
        case ILINK_ERR_PARSE: return "Parse error";
        case ILINK_ERR_PARAM: return "Invalid parameter";
        case ILINK_ERR_UPLOAD: return "Upload error";
        case ILINK_ERR_SEND: return "Send error";
        case ILINK_ERR_TIMEOUT: return "Timeout";
        default: return "Unknown error";
    }
}
