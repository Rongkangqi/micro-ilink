# tools/

Standalone QR login utility for WeChat iLink Bot.  Fetches a QR code,
polls for WeChat scan confirmation, and writes credentials to a config file.

## Files

```
tools/
├── login.c       QR login program (C + libcurl)
├── Makefile      Build script
└── README.md     This file
```

## Requirements

| Dependency        | Required | Notes                                      |
|-------------------|----------|--------------------------------------------|
| libcurl-dev       | yes      | `sudo apt install libcurl4-openssl-dev`     |
| libqrencode-dev   | no       | `sudo apt install libqrencode-dev` for terminal QR |

## Build

```bash
# Basic build (URL-only output)
make -C tools

# With terminal QR display
make -C tools QRENCODE=1

# Clean
make -C tools clean
```

The binary is written to `tools/login`.  It is fully independent from the
main micro-ilink build — the top-level `Makefile` does not touch `tools/`.

## Usage

```bash
./tools/login                  # interactive, writes ./config.json
./tools/login path/to/cfg      # write to custom path
./tools/login -h               # show help
```

**Workflow:**

1. Run `./tools/login`
2. A QR code URL is printed.  Open it in a browser or scan it with WeChat.
   With `make QRENCODE=1` the QR is also drawn directly in the terminal.
3. After scanning, confirm the login inside WeChat.
4. On success, credentials are written to the config file.

**Example output:**

```
=== WeChat iLink QR Login ===

>>> Scan the QR code with WeChat, or open this URL:
https://liteapp.weixin.qq.com/q/7GiQu1?qrcode=...

Waiting for scan...
Scanned - confirm login in WeChat...

=== Login Successful ===

account_id = "xxxxxxxxxxxx@im.bot"
user_id    = "xxxxxxxxxxxx3xxxxxxxxxxxx@im.wechat"
base_url   = "https://ilinkai.weixin.qq.com"
token      = "xxxxxxxxxxxx@im.bot:060000..."
```

The generated config file is ready for use with micro-ilink
(`./build/bin/echo_bot`, `./build/bin/test_image_send`, etc.).

## Cross-compilation

```bash
make -C tools CC=arm-linux-gnueabihf-gcc
```
