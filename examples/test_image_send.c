#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *config_path = "config.json";
    const char *file_path = "test.jpg";

    if (argc > 1) config_path = argv[1];
    if (argc > 2) file_path = argv[2];

    BotConfig cfg;
    if (ilink_config_load(config_path, &cfg) != 0) {
        fprintf(stderr, "Failed to load config from %s\n", config_path);
        return 1;
    }

    printf("Config loaded: account_id=%s, base_url=%s\n", cfg.account_id, cfg.base_url);

    const char *to_user = cfg.user_id;
    if (argc > 3) to_user = argv[3];

    if (!to_user || strlen(to_user) == 0) {
        fprintf(stderr, "Usage: %s <config> <file_path> [to_user_id]\n", argv[0]);
        fprintf(stderr, "  to_user_id defaults to user_id from config (%s)\n",
                cfg.user_id[0] ? cfg.user_id : "not set");
        return 1;
    }

    IlinkBot *bot = ilink_bot_new(&cfg);
    if (!bot) {
        fprintf(stderr, "Failed to create bot\n");
        return 1;
    }

    printf("Sending image %s to %s\n", file_path, to_user);
    IlinkError err = ilink_bot_send_image(bot, to_user, file_path);
    printf("Image send result: %d (%s)\n", err, ilink_error_str(err));

    ilink_bot_free(bot);
    return err == ILINK_ERR_OK ? 0 : 1;
}
