# micro-ilink

WeChat iLink Bot in C — a lightweight library for embedded Linux devices.

## Quick start

### 1. Install dependencies

```bash
sudo apt install libcurl4-openssl-dev libssl-dev
```

Optional for terminal QR display in the login tool:

```bash
sudo apt install libqrencode-dev
```

### 2. Get credentials

```bash
make -C tools          # build the login tool (may add QRENCODE=1)
./tools/login          # scan QR code with WeChat → writes config.json
```

Or copy and edit `config.json` by hand:

```
account_id = "your_bot_id@im.bot"
user_id    = "target_user@im.wechat"
base_url   = "https://ilinkai.weixin.qq.com"
token      = "your_token_here"
```

### 3. Build

```bash
make          # release build (-O2)
make debug    # debug build (-g -O0, verbose logging)
make clean    # remove build/
```

Output:

```
build/
├── bin/               # example programs
│   ├── echo_bot
│   ├── test_image_send
│   ├── test_video_send
│   └── test_file_send
├── *.o *.d            # object and dependency files
└── libilink.a         # static library
```

### 4. Run examples

```bash
./build/bin/test_image_send                 # send test.jpg to user_id from config
./build/bin/test_image_send config.json ./photo.jpg
./build/bin/test_video_send config.json ./clip.mp4
./build/bin/test_file_send config.json ./doc.pdf

./build/bin/echo_bot                        # reply bot (echoes text back)
```

### 5. Cross-compile

```bash
make CC=arm-linux-gnueabihf-gcc AR=arm-linux-gnueabihf-ar
```

## API overview

```c
// Lifecycle
IlinkBot *bot = ilink_bot_new(&cfg);
ilink_bot_free(bot);

// Send (proactive)
ilink_bot_send_text(bot, to_user, "hello");
ilink_bot_send_image(bot, to_user, "photo.jpg");
ilink_bot_send_video(bot, to_user, "clip.mp4");
ilink_bot_send_file(bot, to_user, "doc.pdf");

// Reply (preserves context_token from incoming message)
ilink_bot_reply_text(bot, msg, "got it");
ilink_bot_reply_image(bot, msg, "photo.jpg");
ilink_bot_reply_video(bot, msg, "clip.mp4");
ilink_bot_reply_file(bot, msg, "doc.pdf");

// Event loop
ilink_bot_on_message(bot, FILTER_TEXT, my_handler, NULL);
while (running) {
    ilink_bot_run_once(bot);   // single poll, non-blocking
}
// or: ilink_bot_run(bot);     // blocking loop
```

### Message filters

| Constant      | Value | Matches   |
|---------------|-------|-----------|
| `FILTER_ALL`  | 0     | all       |
| `FILTER_TEXT` | 1     | text      |
| `FILTER_IMAGE`| 2     | images    |
| `FILTER_VOICE`| 3     | voice     |
| `FILTER_FILE` | 4     | files     |
| `FILTER_VIDEO`| 5     | videos    |

### Message types

| Constant             | Value | Send / Reply function  |
|----------------------|-------|------------------------|
| `MESSAGE_TYPE_TEXT`  | 1     | `send_text` / `reply_text`     |
| `MESSAGE_TYPE_IMAGE` | 2     | `send_image` / `reply_image`   |
| `MESSAGE_TYPE_VOICE` | 3     | —                              |
| `MESSAGE_TYPE_FILE`  | 4     | `send_file` / `reply_file`     |
| `MESSAGE_TYPE_VIDEO` | 5     | `send_video` / `reply_video`   |

## Directory structure

```
micro-ilink/
├── include/               # Public headers
│   ├── bot.h              #   high-level bot API
│   ├── ilink.h            #   protocol types, enums, structs
│   ├── http.h             #   HTTP client (libcurl wrapper)
│   ├── cdn.h              #   CDN upload client
│   ├── crypto.h           #   AES-128-ECB encryption
│   ├── config.h           #   config file parser
│   └── utils.h            #   base64, MD5, UUID, hex helpers
├── src/                   # Implementation
│   ├── bot.c              #   message dispatch, send/reply logic
│   ├── ilink.c            #   iLink protocol: JSON builder, API calls, parser
│   ├── cdn.c              #   CDN upload: file read → encrypt → upload → response
│   ├── http.c             #   HTTP GET/POST with header capture
│   ├── crypto.c           #   AES-128-ECB + PKCS#7 padding
│   ├── config.c           #   key=value config file loader
│   └── utils.c            #   base64 (OpenSSL BIO), MD5, hex, UUID
├── examples/              # Example programs
│   ├── echo_bot.c         #   text echo reply bot
│   ├── test_image_send.c  #   one-shot image send
│   ├── test_video_send.c  #   one-shot video send
│   └── test_file_send.c   #   one-shot file send
├── tools/                 # Standalone utilities
│   ├── login.c            #   QR login → obtain credentials
│   ├── Makefile           #   independent build for tools/
│   └── README.md
├── config.json            # Bot credentials
├── Makefile               # Main build (release / debug / clean)
├── .gitignore
└── README.md
```

## Code architecture

```
┌─────────────────────────────────────────────────────┐
│  examples/                                          │
│  echo_bot.c  test_image_send.c  test_video_send.c  │
│  test_file_send.c                                   │
└──────────────────────┬──────────────────────────────┘
                       │  ilink_bot_send_*()
                       │  ilink_bot_reply_*()
                       │  ilink_bot_run_once()
                       ▼
┌─────────────────────────────────────────────────────┐
│  bot.h / bot.c            Bot layer                 │
│                                                      │
│  • message dispatch (filter → handler)               │
│  • send_text / send_image / send_video / send_file  │
│  • reply_text / reply_image / reply_video / reply_file│
│  • long-poll event loop                              │
└──────┬──────────────────┬───────────────────────────┘
       │                  │
       ▼                  ▼
┌──────────────┐  ┌──────────────────────────────────┐
│  cdn.h/cdn.c │  │  ilink.h / ilink.c               │
│               │  │                                  │
│  • file read  │  │  • getuploadurl request/response │
│  • AES encrypt│  │  • sendmessage JSON builder      │
│  • CDN upload │  │  • getupdates parse              │
│  • capture    │  │  • getconfig / sendtyping        │
│    x-encrypt- │  │                                  │
│    ed-param   │  └──────────────┬───────────────────┘
└──────┬───────┘                 │
       │                         │
       ▼                         ▼
┌──────────────┐  ┌──────────────────────────────────┐
│ crypto.h/.c  │  │  http.h / http.c                  │
│               │  │                                   │
│  • AES-128-  │  │  • ilink_http_post_json()          │
│    ECB encrypt│  │  • ilink_http_post_binary()       │
│  • PKCS#7    │  │  • header callback (x-encrypted-   │
│    padding   │  │    param capture)                   │
│               │  │  • write/read buffers              │
└──────────────┘  └───────────────────────────────────┘
       │                         │
       ▼                         ▼
┌──────────────┐  ┌──────────────────────────────────┐
│ utils.h/.c   │  │  config.h / config.c              │
│               │  │                                   │
│  • base64    │  │  • key = "value" parser           │
│  • MD5       │  │  • account_id, token, user_id,   │
│  • hex       │  │    base_url                        │
│  • UUID      │  │                                   │
└──────────────┘  └───────────────────────────────────┘
```

### Module descriptions

| Module | File | Role |
|--------|------|------|
| **bot** | `bot.c` | Top-level API. Manages handler registration, message dispatch, and the send/reply orchestration. `send_media_message()` is the shared path for image/video/file — it calls the CDN layer to upload, builds the `WeixinMessage`, then calls the protocol layer to send. |
| **ilink** | `ilink.c` | Wire-protocol layer. Builds JSON request bodies for all API endpoints (`getuploadurl`, `sendmessage`, `getupdates`, `sendtyping`, `getconfig`). Parses JSON responses with a minimal hand-rolled extractor (no external JSON library). |
| **cdn** | `cdn.c` | CDN upload pipeline. Opens the local file, computes MD5, generates random AES key + filekey, encrypts with AES-128-ECB, calls `getuploadurl`, builds the CDN upload URL, POSTs ciphertext, and captures the `x-encrypted-param` response header. |
| **http** | `http.c` | libcurl wrapper. Provides `POST` (JSON and binary) with header capture. The header callback intercepts `x-encrypted-param` from CDN responses and stores it on the response struct. |
| **crypto** | `crypto.c` | AES-128-ECB encryption with PKCS#7 padding. Input is padded to the next 16-byte boundary (1–16 bytes of padding), then encrypted block-by-block using OpenSSL. |
| **utils** | `utils.c` | Base64 (OpenSSL BIO), MD5 hash (file and string), hex encoding, and UUID generation. Base64 uses a BIO chain: `base64_filter → memstream_sink`. |
| **config** | `config.c` | Simple `key = "value"` parser. Reads `account_id`, `user_id`, `base_url`, `token` into a `BotConfig` struct. |
| **login** | `tools/login.c` | Standalone QR login. Fetches a QR code from the iLink API, polls for scan status, and writes `config.json` on confirmation. Depends only on libcurl. Optional libqrencode integration for terminal QR display. |

## Media send flow

Sending an image, video, or file follows the same pipeline:

```
   ilink_bot_send_image(to, path)
          │
          ▼
   send_media_message()                     bot.c
          │
          ├─ 1. Open file, read bytes
          ├─ 2. MD5 plaintext
          ├─ 3. Generate random 16-byte AES key + 32-hex filekey
          ├─ 4. PKCS#7 pad → AES-128-ECB encrypt           crypto.c
          │
          ├─ 5. POST /ilink/bot/getuploadurl               ilink.c → http.c
          │      Body: {filekey, media_type, rawsize,
          │             rawfilemd5, filesize, aeskey}
          │      Response: {upload_param}
          │
          ├─ 6. Build CDN URL:
          │      https://novac2c.cdn.weixin.qq.com/c2c/upload
          │      ?encrypted_query_param=<upload_param>
          │      &filekey=<filekey>
          │
          ├─ 7. POST ciphertext to CDN URL                  cdn.c → http.c
          │      Content-Type: application/octet-stream
          │      Response header: x-encrypted-param
          │
          ├─ 8. Build WeixinMessage item_list JSON          ilink.c
          │      { type: 2|4|5, image_item|file_item|video_item: {
          │          media: {encrypt_query_param, aes_key, encrypt_type:1},
          │          mid_size, file_name (file only)
          │      }}
          │
          └─ 9. POST /ilink/bot/sendmessage                ilink.c → http.c
                 Response: {} (success)
```

**Key details:**

- `aes_key` in the sendmessage payload is `base64(hex_string)` — the 32-char hex string of the 16-byte AES key is base64-encoded as ASCII. This matches the Python SDK behavior.
- `mid_size` is the **ciphertext** size (after PKCS#7 padding), not the original file size.
- For files, `file_name` (basename) and `len` (raw file size as string) are included so WeChat displays the correct filename.
- The `x-encrypted-param` from the CDN upload response becomes `encrypt_query_param` in the sendmessage payload — this is what the receiving client uses to download and decrypt the media.
- `context_token` is omitted from the JSON when empty (matching Python's `exclude_none=True`).

## Message receive flow

```
   ilink_bot_run_once()
          │
          ├─ 1. POST /ilink/bot/getupdates
          │      Body: {get_updates_buf, base_info}
          │
          ├─ 2. Parse response JSON
          │      Extract msgs[] → WeixinMessage structs
          │      Save get_updates_buf for next poll
          │
          └─ 3. Dispatch each message to matching handlers
                 Filter by msg.items[0].type
```

## Dependencies

| Library | Purpose |
|---------|---------|
| libcurl | HTTP client (all API calls, CDN upload) |
| libssl  | OpenSSL — AES encryption, MD5, base64 BIO |
| libm    | math (linked by OpenSSL) |
| libqrencode | optional — terminal QR display in `tools/login` |

## License

MIT
