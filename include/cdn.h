#ifndef ILINK_CDN_H
#define ILINK_CDN_H

#include "ilink.h"
#include "http.h"
#include "config.h"

typedef struct IlinkCdnClient IlinkCdnClient;

IlinkCdnClient *ilink_cdn_client_new(IlinkHttpClient *http);
void ilink_cdn_client_free(IlinkCdnClient *client);

IlinkError ilink_cdn_upload_media(IlinkCdnClient *cdn, const BotConfig *cfg,
                                   const char *file_path, int media_type,
                                   const char *to_user_id,
                                   UploadedMedia **result);

IlinkError ilink_cdn_build_upload_url(const char *base_url, const char *upload_param,
                                       const char *filekey, char **url);

#endif /* ILINK_CDN_H */
