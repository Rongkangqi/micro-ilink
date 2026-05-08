# micro-ilink

微信 iLink Bot 的 C 语言实现 —— 面向嵌入式 Linux 设备的轻量级库。

## 快速开始

### 1. 安装依赖

```bash
sudo apt install libcurl4-openssl-dev libssl-dev
```

登录工具可选（终端显示二维码）：

```bash
sudo apt install libqrencode-dev
```

### 2. 获取凭证

```bash
make -C tools          # 编译登录工具（可追加 QRENCODE=1）
./tools/login          # 用微信扫码 → 自动写入 config.json
```

或手动创建并编辑 `config.json`：

```
account_id = "your_bot_id@im.bot"
user_id    = "target_user@im.wechat"
base_url   = "https://ilinkai.weixin.qq.com"
token      = "your_token_here"
```

### 3. 编译

```bash
make          # release 编译 (-O2)
make debug    # debug 编译 (-g -O0，输出详细调试信息)
make clean    # 删除 build/
```

输出目录：

```
build/
├── bin/               # 示例程序
│   ├── echo_bot
│   ├── test_image_send
│   ├── test_video_send
│   └── test_file_send
├── *.o *.d            # 目标文件和依赖文件
└── libilink.a         # 静态库
```

### 4. 运行示例

```bash
./build/bin/test_image_send                 # 发送 test.jpg，目标用户取自 config
./build/bin/test_image_send config.json ./photo.jpg
./build/bin/test_video_send config.json ./clip.mp4
./build/bin/test_file_send config.json ./doc.pdf

./build/bin/echo_bot                        # 回复机器人（原样回复文本）
```

### 5. 交叉编译

```bash
make CC=arm-linux-gnueabihf-gcc AR=arm-linux-gnueabihf-ar
```

## API 概览

```c
// 生命周期
IlinkBot *bot = ilink_bot_new(&cfg);
ilink_bot_free(bot);

// 主动发送
ilink_bot_send_text(bot, to_user, "hello");
ilink_bot_send_image(bot, to_user, "photo.jpg");
ilink_bot_send_video(bot, to_user, "clip.mp4");
ilink_bot_send_file(bot, to_user, "doc.pdf");

// 回复（保留收到消息中的 context_token）
ilink_bot_reply_text(bot, msg, "got it");
ilink_bot_reply_image(bot, msg, "photo.jpg");
ilink_bot_reply_video(bot, msg, "clip.mp4");
ilink_bot_reply_file(bot, msg, "doc.pdf");

// 事件循环
ilink_bot_on_message(bot, FILTER_TEXT, my_handler, NULL);
while (running) {
    ilink_bot_run_once(bot);   // 单次轮询，非阻塞
}
// 或: ilink_bot_run(bot);     // 阻塞循环
```

### 消息过滤器

| 常量           | 值 | 匹配         |
|----------------|----|--------------|
| `FILTER_ALL`   | 0  | 所有消息     |
| `FILTER_TEXT`  | 1  | 文本         |
| `FILTER_IMAGE` | 2  | 图片         |
| `FILTER_VOICE` | 3  | 语音         |
| `FILTER_FILE`  | 4  | 文件         |
| `FILTER_VIDEO` | 5  | 视频         |

### 消息类型

| 常量                 | 值 | 发送 / 回复函数                  |
|----------------------|----|----------------------------------|
| `MESSAGE_TYPE_TEXT`  | 1  | `send_text` / `reply_text`       |
| `MESSAGE_TYPE_IMAGE` | 2  | `send_image` / `reply_image`     |
| `MESSAGE_TYPE_VOICE` | 3  | —                                |
| `MESSAGE_TYPE_FILE`  | 4  | `send_file` / `reply_file`       |
| `MESSAGE_TYPE_VIDEO` | 5  | `send_video` / `reply_video`     |

## 目录结构

```
micro-ilink/
├── include/               # 公共头文件
│   ├── bot.h              #   高层 Bot API
│   ├── ilink.h            #   协议类型、枚举、结构体
│   ├── http.h             #   HTTP 客户端（libcurl 封装）
│   ├── cdn.h              #   CDN 上传客户端
│   ├── crypto.h           #   AES-128-ECB 加密
│   ├── config.h           #   配置文件解析
│   └── utils.h            #   base64、MD5、UUID、hex 工具
├── src/                   # 实现
│   ├── bot.c              #   消息分发、发送/回复逻辑
│   ├── ilink.c            #   iLink 协议：JSON 构建、API 调用、解析
│   ├── cdn.c              #   CDN 上传：读文件 → 加密 → 上传 → 响应
│   ├── http.c             #   HTTP GET/POST，含响应头捕获
│   ├── crypto.c           #   AES-128-ECB + PKCS#7 填充
│   ├── config.c           #   key=value 配置文件加载
│   └── utils.c            #   base64（OpenSSL BIO）、MD5、hex、UUID
├── examples/              # 示例程序
│   ├── echo_bot.c         #   文本回显机器人
│   ├── test_image_send.c  #   单次发送图片
│   ├── test_video_send.c  #   单次发送视频
│   └── test_file_send.c   #   单次发送文件
├── tools/                 # 独立工具
│   ├── login.c            #   扫码登录 → 获取凭证
│   ├── Makefile           #   tools/ 的独立构建
│   └── README.md
├── config.json            # Bot 凭证
├── Makefile               # 主构建（release / debug / clean）
├── .gitignore
├── README.md              # 英文说明
└── README_zh.md           # 本文档
```

## 代码架构

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
│  bot.h / bot.c            机器人层                   │
│                                                      │
│  • 消息分发（filter → handler）                      │
│  • send_text / send_image / send_video / send_file  │
│  • reply_text / reply_image / reply_video / reply_file│
│  • 长轮询事件循环                                     │
└──────┬──────────────────┬───────────────────────────┘
       │                  │
       ▼                  ▼
┌──────────────┐  ┌──────────────────────────────────┐
│  cdn.h/cdn.c │  │  ilink.h / ilink.c               │
│               │  │                                  │
│  • 读文件     │  │  • getuploadurl 请求/响应        │
│  • AES 加密   │  │  • sendmessage JSON 构建         │
│  • CDN 上传   │  │  • getupdates 解析               │
│  • 捕获       │  │  • getconfig / sendtyping        │
│    x-encrypt- │  │                                  │
│    ed-param   │  └──────────────┬───────────────────┘
└──────┬───────┘                 │
       │                         │
       ▼                         ▼
┌──────────────┐  ┌──────────────────────────────────┐
│ crypto.h/.c  │  │  http.h / http.c                  │
│               │  │                                   │
│  • AES-128-  │  │  • ilink_http_post_json()          │
│    ECB 加密   │  │  • ilink_http_post_binary()       │
│  • PKCS#7    │  │  • 响应头回调（捕获 x-encrypted-   │
│    填充      │  │    param）                          │
│               │  │  • 读写缓冲区                       │
└──────────────┘  └───────────────────────────────────┘
       │                         │
       ▼                         ▼
┌──────────────┐  ┌──────────────────────────────────┐
│ utils.h/.c   │  │  config.h / config.c              │
│               │  │                                   │
│  • base64    │  │  • key = "value" 解析器           │
│  • MD5       │  │  • account_id、token、user_id、  │
│  • hex       │  │    base_url                        │
│  • UUID      │  │                                   │
└──────────────┘  └───────────────────────────────────┘
```

### 模块说明

| 模块 | 文件 | 职责 |
|------|------|------|
| **bot** | `bot.c` | 顶层 API。管理处理器注册、消息分发和发送/回复编排。`send_media_message()` 是图片/视频/文件的共用路径 —— 调用 CDN 层上传，构建 `WeixinMessage`，再调用协议层发送。 |
| **ilink** | `ilink.c` | 协议层。为所有 API 端点构建 JSON 请求体（`getuploadurl`、`sendmessage`、`getupdates`、`sendtyping`、`getconfig`）。用最简手写提取器解析 JSON 响应（无外部 JSON 库依赖）。 |
| **cdn** | `cdn.c` | CDN 上传管线。打开本地文件，计算 MD5，生成随机 AES 密钥和 filekey，用 AES-128-ECB 加密，调用 `getuploadurl`，构建 CDN 上传 URL，POST 密文，捕获 `x-encrypted-param` 响应头。 |
| **http** | `http.c` | libcurl 封装。提供 POST（JSON 和二进制），含响应头捕获。响应头回调拦截 CDN 响应中的 `x-encrypted-param` 并存入响应结构体。 |
| **crypto** | `crypto.c` | AES-128-ECB 加密 + PKCS#7 填充。输入填充至 16 字节边界（1–16 字节填充值），然后用 OpenSSL 逐块加密。 |
| **utils** | `utils.c` | Base64（OpenSSL BIO）、MD5 哈希（文件和字符串）、hex 编码、UUID 生成。Base64 使用 BIO 链：`base64_filter → memstream_sink`。 |
| **config** | `config.c` | 简单 `key = "value"` 解析器。将 `account_id`、`user_id`、`base_url`、`token` 读入 `BotConfig` 结构体。 |
| **login** | `tools/login.c` | 独立扫码登录。从 iLink API 获取二维码，轮询扫码状态，确认后将凭证写入 `config.json`。仅依赖 libcurl。可选 libqrencode 实现终端二维码显示。 |

## 媒体发送流程

发送图片、视频或文件遵循同一管线：

```
   ilink_bot_send_image(to, path)
          │
          ▼
   send_media_message()                     bot.c
          │
          ├─ 1. 打开文件，读取字节
          ├─ 2. 计算原文 MD5
          ├─ 3. 生成随机 16 字节 AES 密钥 + 32 位 hex filekey
          ├─ 4. PKCS#7 填充 → AES-128-ECB 加密              crypto.c
          │
          ├─ 5. POST /ilink/bot/getuploadurl               ilink.c → http.c
          │      请求体: {filekey, media_type, rawsize,
          │              rawfilemd5, filesize, aeskey}
          │      响应: {upload_param}
          │
          ├─ 6. 构建 CDN URL:
          │      https://novac2c.cdn.weixin.qq.com/c2c/upload
          │      ?encrypted_query_param=<upload_param>
          │      &filekey=<filekey>
          │
          ├─ 7. POST 密文到 CDN URL                          cdn.c → http.c
          │      Content-Type: application/octet-stream
          │      响应头: x-encrypted-param
          │
          ├─ 8. 构建 WeixinMessage item_list JSON           ilink.c
          │      { type: 2|4|5, image_item|file_item|video_item: {
          │          media: {encrypt_query_param, aes_key, encrypt_type:1},
          │          mid_size, file_name (仅文件)
          │      }}
          │
          └─ 9. POST /ilink/bot/sendmessage                ilink.c → http.c
                 响应: {} (成功)
```

**关键细节：**

- sendmessage 中的 `aes_key` 为 `base64(hex_string)` —— 将 16 字节 AES 密钥的 32 字符 hex 字符串按 ASCII 进行 base64 编码。与 Python SDK 行为一致。
- `mid_size` 是**密文**大小（PKCS#7 填充后），而非原始文件大小。
- 发送文件时额外包含 `file_name`（文件名）和 `len`（原始文件大小的字符串），使微信能正确显示文件名。
- CDN 上传响应中的 `x-encrypted-param` 会成为 sendmessage 中的 `encrypt_query_param` —— 接收方客户端用它来下载和解密媒体文件。
- `context_token` 为空时不写入 JSON（与 Python 的 `exclude_none=True` 行为一致）。

## 消息接收流程

```
   ilink_bot_run_once()
          │
          ├─ 1. POST /ilink/bot/getupdates
          │      请求体: {get_updates_buf, base_info}
          │
          ├─ 2. 解析响应 JSON
          │      提取 msgs[] → WeixinMessage 结构体
          │      保存 get_updates_buf 供下次轮询使用
          │
          └─ 3. 将每条消息分发给匹配的处理器
                 按 msg.items[0].type 进行过滤
```

## 依赖

| 库 | 用途 |
|----|------|
| libcurl | HTTP 客户端（所有 API 调用、CDN 上传） |
| libssl  | OpenSSL — AES 加密、MD5、base64 BIO |
| libm    | 数学库（OpenSSL 间接依赖） |
| libqrencode | 可选 — `tools/login` 中的终端二维码显示 |

## 许可证

MIT
