#ifndef ILINK_BOT_H
#define ILINK_BOT_H

#include "ilink.h"
#include "http.h"
#include "cdn.h"
#include "config.h"

#define ILINK_BOT_MAX_HANDLERS 16

typedef struct IlinkBot IlinkBot;

typedef void (*IlinkMessageHandler)(IlinkBot *bot, const WeixinMessage *msg, void *user_data);

typedef struct IlinkHandlerEntry {
    IlinkFilterType filter;
    IlinkMessageHandler handler;
    void *user_data;
} IlinkHandlerEntry;

struct IlinkBot {
    BotConfig config;
    IlinkHttpClient *http;
    IlinkCdnClient *cdn;
    IlinkHandlerEntry handlers[ILINK_BOT_MAX_HANDLERS];
    int handler_count;
    volatile int running;
    char *get_updates_buf;
};

IlinkBot *ilink_bot_new(const BotConfig *cfg);
void ilink_bot_free(IlinkBot *bot);

int ilink_bot_on_message(IlinkBot *bot, IlinkFilterType filter,
                         IlinkMessageHandler handler, void *user_data);

void ilink_bot_run(IlinkBot *bot);
void ilink_bot_run_once(IlinkBot *bot);
void ilink_bot_stop(IlinkBot *bot);

IlinkError ilink_bot_send_text(IlinkBot *bot, const char *to_user, const char *text);
IlinkError ilink_bot_send_image(IlinkBot *bot, const char *to_user, const char *file_path);
IlinkError ilink_bot_send_video(IlinkBot *bot, const char *to_user, const char *file_path);
IlinkError ilink_bot_send_file(IlinkBot *bot, const char *to_user, const char *file_path);

IlinkError ilink_bot_reply_text(IlinkBot *bot, const WeixinMessage *msg, const char *text);
IlinkError ilink_bot_reply_image(IlinkBot *bot, const WeixinMessage *msg, const char *file_path);
IlinkError ilink_bot_reply_video(IlinkBot *bot, const WeixinMessage *msg, const char *file_path);
IlinkError ilink_bot_reply_file(IlinkBot *bot, const WeixinMessage *msg, const char *file_path);

const char *ilink_error_str(IlinkError err);

#endif /* ILINK_BOT_H */
