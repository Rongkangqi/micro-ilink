#ifndef ILINK_UTILS_H
#define ILINK_UTILS_H

#include <stddef.h>

#ifdef ILINK_DEBUG
#define DEBUG_printf(...) fprintf(stderr, "[DEBUG] " __VA_ARGS__)
#define DEBUG_hexdump(label, data, len) \
    do { \
        fprintf(stderr, "[DEBUG] %s (%d bytes): ", label, (int)len); \
        for (int i = 0; i < (len) && i < 64; i++) fprintf(stderr, "%02x ", (unsigned char)(data)[i]); \
        fprintf(stderr, "\n"); \
    } while(0)
#else
#define DEBUG_printf(...) ((void)0)
#define DEBUG_hexdump(...) ((void)0)
#endif

char *ilink_generate_uuid(void);
char *ilink_generate_client_id(void);
char *ilink_base64_encode(const unsigned char *data, size_t len);
char *ilink_md5_file(const char *file_path);
char *ilink_md5_string(const char *str);
char *ilink_hex_encode(const unsigned char *data, size_t len);

#endif /* ILINK_UTILS_H */
