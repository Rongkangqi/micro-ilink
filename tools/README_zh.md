# tools/

微信 iLink Bot 独立扫码登录工具，获取二维码、轮询扫码状态、将凭证写入配置文件。

## 文件结构

```
tools/
├── login.c       扫码登录程序（C + libcurl）
├── Makefile      构建脚本
├── README.md     英文说明
└── README_zh.md  本文档
```

## 依赖

| 依赖              | 必需   | 说明                                          |
|-------------------|--------|-----------------------------------------------|
| libcurl-dev       | 是     | `sudo apt install libcurl4-openssl-dev`       |
| libqrencode-dev   | 否     | `sudo apt install libqrencode-dev`，终端显示二维码 |

## 编译

```bash
# 基础编译（仅输出 URL）
make -C tools

# 编译并启用终端二维码显示
make -C tools QRENCODE=1

# 清理
make -C tools clean
```

编译产物为 `tools/login`，与 micro-ilink 主构建完全独立，顶层 `Makefile` 不会触碰 `tools/`。

## 使用方式

```bash
./tools/login                  # 交互模式，写入 ./config.json
./tools/login path/to/cfg      # 写入指定路径
./tools/login -h               # 查看帮助
```

**操作流程：**

1. 运行 `./tools/login`
2. 终端会打印二维码 URL，在浏览器中打开或用微信扫码。
   若编译时启用了 `QRENCODE=1`，终端会直接绘制二维码图案。
3. 扫码后在微信中确认登录。
4. 登录成功后，凭证自动写入配置文件。

**输出示例：**

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

生成的配置文件可直接用于 micro-ilink 的各个示例程序。

## 交叉编译

```bash
make -C tools CC=arm-linux-gnueabihf-gcc
```
