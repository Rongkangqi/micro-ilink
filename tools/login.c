/* Standalone QR login for WeChat iLink Bot.
 *
 * Fetches a QR code, polls for WeChat scan confirmation, and writes
 * the resulting credentials to config.json.
 *
 * Build:  make -C tools
 * Usage:  ./tools/login [config_path]
 *
 * Requires: libcurl
 * Optional: libqrencode (make QRENCODE=1 for terminal QR display)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>

#ifdef HAS_QRENCODE
#include <qrencode.h>
#endif

/* ── Constants ─────────────────────────────────────────────────────── */

#define BASE_URL      "https://ilinkai.weixin.qq.com"
#define GET_QRCODE    BASE_URL "/ilink/bot/get_bot_qrcode?bot_type=3"
#define POLL_STATUS   BASE_URL "/ilink/bot/get_qrcode_status?qrcode="

#define POLL_INTERVAL  2      /* seconds between status checks */
#define MAX_QR_REFRESH 3      /* max QR code refreshes */
#define TIMEOUT        480    /* total timeout seconds */

/* ── Simple growable buffer ────────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

static void buf_init(Buffer *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buf_free(Buffer *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static size_t buf_write(void *ptr, size_t sz, size_t nmemb, void *user) {
    Buffer *b = (Buffer *)user;
    size_t n = sz * nmemb;
    if (b->len + n > b->cap) {
        size_t newcap = b->cap ? b->cap * 2 : 4096;
        while (b->len + n > newcap) newcap *= 2;
        char *p = realloc(b->data, newcap + 1);
        if (!p) return 0;
        b->data = p;
        b->cap = newcap;
    }
    memcpy(b->data + b->len, ptr, n);
    b->len += n;
    b->data[b->len] = '\0';
    return n;
}

/* ── Minimal JSON string extractor ─────────────────────────────────── */

static char *json_str(const char *json, const char *key) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == ':') p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = p;
    while (*end && *end != '"') {
        if (*end == '\\') end++;
        end++;
    }
    size_t len = end - p;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\\' && i + 1 < len) out[j++] = p[++i];
        else out[j++] = p[i];
    }
    out[j] = '\0';
    return out;
}

/* ── HTTP GET ──────────────────────────────────────────────────────── */

static CURL *curl = NULL;

static int http_get(const char *url, Buffer *body, long timeout_s) {
    if (!curl) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl = curl_easy_init();
        if (!curl) return -1;
    }

    buf_init(body);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buf_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_s);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "micro-ilink/1.0");

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        buf_free(body);
        return -1;
    }
    return 0;
}

/* ── Terminal QR display (optional, needs libqrencode) ─────────────── */

static void show_qr(const char *url) {
#ifdef HAS_QRENCODE
    QRcode *qr = QRcode_encodeString(url, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!qr) return;

    /* top border */
    for (int i = 0; i < qr->width + 2; i++) printf("\033[47m  \033[0m");
    printf("\n");

    for (int y = 0; y < qr->width; y++) {
        printf("\033[47m  \033[0m");  /* left border */
        for (int x = 0; x < qr->width; x++) {
            int black = qr->data[y * qr->width + x] & 1;
            printf(black ? "\033[40m  \033[0m" : "\033[47m  \033[0m");
        }
        printf("\033[47m  \033[0m\n");  /* right border */
    }

    /* bottom border */
    for (int i = 0; i < qr->width + 2; i++) printf("\033[47m  \033[0m");
    printf("\n");

    QRcode_free(qr);
#else
    (void)url;
#endif
}

/* ── Login flow ────────────────────────────────────────────────────── */

static int do_login(const char *out_path) {
    Buffer body;
    int qr_refresh = 0;
    time_t deadline = time(NULL) + TIMEOUT;
    const char *poll_base = BASE_URL;

    while (time(NULL) < deadline) {
        /* fetch QR code */
        if (http_get(GET_QRCODE, &body, 5) != 0) {
            fprintf(stderr, "Failed to fetch QR code\n");
            return -1;
        }

        char *key = json_str(body.data, "qrcode");
        char *url = json_str(body.data, "qrcode_img_content");
        buf_free(&body);

        if (!key || !*key) {
            fprintf(stderr, "Server returned empty QR code key\n");
            free(key); free(url);
            return -1;
        }

        /* show QR */
        if (url) {
            show_qr(url);
            printf("\n>>> Scan the QR code with WeChat, or open this URL:\n%s\n\n", url);
        }
        printf("Waiting for scan...\n");

        int scanned_shown = 0;

        /* poll status */
        while (time(NULL) < deadline) {
            char poll_url[1024];
            snprintf(poll_url, sizeof(poll_url), "%s/ilink/bot/get_qrcode_status?qrcode=%s",
                     poll_base, key);

            if (http_get(poll_url, &body, 35) != 0) {
                fprintf(stderr, "  Network error, retrying...\n");
                buf_free(&body);
                sleep(POLL_INTERVAL);
                continue;
            }

            char *status = json_str(body.data, "status");

            if (status && strcmp(status, "scaned") == 0) {
                if (!scanned_shown) {
                    printf("Scanned - confirm login in WeChat...\n");
                    scanned_shown = 1;
                }
            } else if (status && strcmp(status, "scaned_but_redirect") == 0) {
                char *host = json_str(body.data, "redirect_host");
                if (host && *host) {
                    char new_base[512];
                    snprintf(new_base, sizeof(new_base), "https://%s", host);
                    poll_base = strdup(new_base);
                    printf("  Redirected to: %s\n", poll_base);
                }
                free(host);
            } else if (status && strcmp(status, "confirmed") == 0) {
                char *token     = json_str(body.data, "bot_token");
                char *acct      = json_str(body.data, "ilink_bot_id");
                char *base      = json_str(body.data, "baseurl");
                char *uid       = json_str(body.data, "ilink_user_id");

                free(status);
                buf_free(&body);
                free(key);
                free(url);

                if (!acct) {
                    fprintf(stderr, "Login confirmed but missing account ID\n");
                    free(token); free(base); free(uid);
                    return -1;
                }
                if (!token) token = strdup("");

                /* write config */
                FILE *f = fopen(out_path, "w");
                if (!f) {
                    fprintf(stderr, "Cannot open %s for writing\n", out_path);
                    free(acct); free(token); free(base); free(uid);
                    return -1;
                }
                fprintf(f, "account_id = \"%s\"\n", acct);
                fprintf(f, "user_id    = \"%s\"\n", uid ? uid : "");
                fprintf(f, "base_url   = \"%s\"\n", base ? base : BASE_URL);
                fprintf(f, "token      = \"%s\"\n", token);
                fclose(f);

                printf("\n=== Login Successful ===\n\n");
                printf("account_id = \"%s\"\n", acct);
                printf("user_id    = \"%s\"\n", uid ? uid : "");
                printf("base_url   = \"%s\"\n", base ? base : BASE_URL);
                printf("token      = \"%s\"\n", token);
                printf("\nConfig saved to: %s\n", out_path);

                free(acct); free(token); free(base); free(uid);
                return 0;
            } else if (status && strcmp(status, "expired") == 0) {
                free(status);
                buf_free(&body);
                qr_refresh++;
                if (qr_refresh >= MAX_QR_REFRESH) {
                    fprintf(stderr, "QR code expired, refresh limit reached\n");
                    free(key); free(url);
                    return -1;
                }
                printf("QR code expired, refreshing (%d/%d)...\n",
                       qr_refresh, MAX_QR_REFRESH);
                break;  /* fetch new QR */
            }

            free(status);
            buf_free(&body);
            sleep(POLL_INTERVAL);
        }

        free(key);
        free(url);

        if (time(NULL) >= deadline) {
            fprintf(stderr, "Login timed out (%ds)\n", TIMEOUT);
            return -1;
        }
    }

    fprintf(stderr, "Login timed out (%ds)\n", TIMEOUT);
    return -1;
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *out = "config.json";

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("Usage: %s [config_path]\n", argv[0]);
            printf("Fetches a QR code, polls for WeChat confirmation,\n"
                   "and writes credentials to config_path (default: config.json).\n");
            return 0;
        }
        out = argv[1];
    }

    setbuf(stdout, NULL);
    printf("=== WeChat iLink QR Login ===\n\n");
    return do_login(out);
}
