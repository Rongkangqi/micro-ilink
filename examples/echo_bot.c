#include "../include/bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile int g_running = 1;

static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
}

static void echo_handler(IlinkBot *bot, const WeixinMessage *msg, void *user_data) {
    (void)user_data;

    if (msg->item_count == 0) return;

    MessageItem *item = &msg->items[0];

    if (item->type == MESSAGE_TYPE_TEXT && item->content.text_item.text) {
        printf("Received from %s: %s\n", msg->from_user_id, item->content.text_item.text);

        IlinkError err = ilink_bot_reply_text(bot, msg, item->content.text_item.text);
        if (err != ILINK_ERR_OK) {
            fprintf(stderr, "Failed to send reply: %s\n", ilink_error_str(err));
        } else {
            printf("Replied: %s\n", item->content.text_item.text);
        }
    }
}

int main(int argc, char *argv[]) {
    const char *config_path = "config.json";

    if (argc > 1) {
        config_path = argv[1];
    }

    BotConfig cfg;
    if (ilink_config_load(config_path, &cfg) != 0) {
        fprintf(stderr, "Failed to load config from %s\n", config_path);
        return 1;
    }

    printf("Config loaded: account_id=%s, base_url=%s\n", cfg.account_id, cfg.base_url);

    IlinkBot *bot = ilink_bot_new(&cfg);
    if (!bot) {
        fprintf(stderr, "Failed to create bot\n");
        return 1;
    }

    ilink_bot_on_message(bot, FILTER_TEXT, echo_handler, NULL);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Echo bot started. Press Ctrl+C to stop.\n");

    while (g_running) {
        ilink_bot_run_once(bot);
    }

    printf("\nShutting down...\n");
    ilink_bot_free(bot);

    return 0;
}
