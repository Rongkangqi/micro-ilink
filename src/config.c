#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *extract_value(const char *line) {
    const char *start = NULL;
    const char *p = line;

    while (*p) {
        if (*p == '"') {
            start = p + 1;
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
        p++;
    }
    return NULL;
}

static char *get_value_from_line(const char *line, const char *key) {
    while (*line == ' ') line++;

    size_t key_len = strlen(key);
    if (strncmp(line, key, key_len) != 0) return NULL;

    line += key_len;
    while (*line == ' ') line++;

    if (*line != '=') return NULL;
    line++;

    while (*line == ' ') line++;

    return extract_value(line);
}

int ilink_config_load(const char *path, BotConfig *cfg) {
    if (!path || !cfg) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    memset(cfg, 0, sizeof(BotConfig));

    while (fgets(line, sizeof(line), f)) {
        char *val;
        if ((val = get_value_from_line(line, "account_id"))) {
            strncpy(cfg->account_id, val, ILINK_CONFIG_MAX_LEN - 1);
            free(val);
        } else if ((val = get_value_from_line(line, "user_id"))) {
            strncpy(cfg->user_id, val, ILINK_CONFIG_MAX_LEN - 1);
            free(val);
        } else if ((val = get_value_from_line(line, "base_url"))) {
            strncpy(cfg->base_url, val, ILINK_CONFIG_MAX_LEN - 1);
            free(val);
        } else if ((val = get_value_from_line(line, "token"))) {
            strncpy(cfg->token, val, ILINK_CONFIG_MAX_LEN - 1);
            free(val);
        }
    }

    fclose(f);

    if (cfg->account_id[0] == '\0') return -1;
    return 0;
}

void ilink_config_init(BotConfig *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(BotConfig));
}

void ilink_config_free(BotConfig *cfg) {
    (void)cfg;
}
