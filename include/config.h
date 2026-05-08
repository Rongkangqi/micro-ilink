#ifndef ILINK_CONFIG_H
#define ILINK_CONFIG_H

#define ILINK_CONFIG_MAX_LEN 512

typedef struct BotConfig {
    char account_id[ILINK_CONFIG_MAX_LEN];
    char user_id[ILINK_CONFIG_MAX_LEN];
    char base_url[ILINK_CONFIG_MAX_LEN];
    char token[ILINK_CONFIG_MAX_LEN];
} BotConfig;

int ilink_config_load(const char *path, BotConfig *cfg);
void ilink_config_init(BotConfig *cfg);
void ilink_config_free(BotConfig *cfg);

#endif /* ILINK_CONFIG_H */
