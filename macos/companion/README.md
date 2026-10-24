# X-Knob Companion · macOS 菜单栏软件

打开 `X-Knob Companion.app` 后只在菜单栏显示旋钮图标，不在 Dock 常驻。设置窗口关闭后继续连接；菜单中的“退出”会同时结束本应用的后台引擎和 SSH 转发。

## 使用

1. 打开软件，在菜单栏选择 **MQTT 设置**。
2. 填写服务器地址、端口、用户名和密码，按 **保存并连接**。窗口与菜单会显示连接状态；网络恢复后自动重连。
3. 在菜单中选择 **安装 Hooks** 或 **卸载 Hooks**。安装只启用 SessionStart、UserPromptSubmit、PermissionRequest、Stop、Interrupt、SessionEnd 六类重要事件。
4. 退出软件时使用菜单的 **退出 X-Knob Companion**，或设置窗口的 **退出软件**。退出后不会有旧 LaunchAgent 自动将服务重新拉起。

密码留空表示继续使用已保存的密码；新输入的密码保存在 macOS 钥匙串，不写入设置文件或日志。初次启动会读取旧的私有配置并继续使用原有钥匙串密码。高级设置中的“配对标识”须与旋钮私有 `firmware/src/secrets.h` 中的 `XK_CODEX_HOST_ID` 一致。

如果现有 broker 通过 SSH 访问，在高级设置中填写 SSH 配置别名或 `user@host`。此时地址和端口填写远端 MQTT 的地址与端口，软件会管理本地转发。SSH 的密钥与主机信任沿用系统配置；不在软件里存储 SSH 密码。直连 TLS 使用系统证书校验，不跳过证书错误。

## Hooks 清理与可逆安装

迁移会清理本项目历史路径（包括 `1.Firmware/mac_companion`）以及已删除的本项目应用留下的失效注册。安装会把本项目的现用注册统一替换为应用内原生 `--hook` 命令，卸载只移除本项目注册；其他项目的 Hooks 与配置保持不变。每次变更前都会创建私有、独立的备份，重复安装不会累积重复项。

[OpenAI 官方 Hooks 文档](https://learn.chatgpt.com/docs/hooks)说明：不同来源的 Hooks 会合并加载，非托管 Hooks 仍需由 Codex 客户端审核和信任。软件的安装按钮管理注册，不会修改信任数据库或自动接受客户端的信任提示。

旧的 `local.xknob.companion` LaunchAgent 会先备份，再停用和移除，由菜单栏软件管理唯一的服务实例。默认没有新增登录启动项。个人 MQTT 设置、钥匙串与旧配置备份会保留；删除应用前建议先在菜单中卸载 Hooks，再退出软件。

## 数据与状态边界

- 配置位于 `~/.config/xknob-companion/settings.json`，包含地址、用户名和配对设置，不包含密码；权限为 0600。
- 备份位于同一目录的 `backups/`；原有 `companion.env` 和钥匙串不会被删除。
- Hooks 只发送事件名称与会话标识等白名单元数据，不发送提示词、命令、目录或工具输出。原生转发器在软件关闭或输入损坏时快速返回，不阻止 Codex。
- Stop 只表示本轮结束，不能表示任务或测试已经成功。
- MQTT 仍使用 `xknob/v1/codex/<配对标识>/` 主题，语义变化发布与 120 秒保活保持不变。普通工具事件默认不安装；事件不控制房间灯带。
- 软件只管理自己启动的后台进程和隧道；退出后发布 offline 并清理子进程。异常退出时，后台也会检测父进程消失并退出。

## 构建与测试

当前构建为 Apple Silicon 原生应用，本机验证环境为 macOS 27。构建脚本根据包内 Python/OpenSSL 等实际依赖设置最低系统版本；当前本机产物需要 macOS 27，不声称兼容尚未验证的旧系统。应用已内置 Python 运行时与 MQTT 引擎，用户运行它不需要安装 Python、pip 或项目依赖。

开发构建需要系统 Swift 命令行工具及 Python：

```sh
cd macos/companion
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt -r app/requirements-build.txt
.venv/bin/python -m unittest discover -s tests -v
.venv/bin/python app/build_app.py
.venv/bin/python app/test_bundle.py
```

产物为 `dist/X-Knob Companion.app`，可以复制到用户的“应用程序”文件夹。构建脚本打包后端、编译 AppKit 界面、设置 `LSUIElement`，并进行本机签名与签名校验。`build/`、`dist/` 和凭据均不进入 Git。

验证覆盖旧路径清理、重复安装与卸载、保留其他 Hooks、私有文件权限、配置验证、打包后原生转发器的隐私过滤、离线快速返回、真实打包引擎的 HTTP 鉴权与正常退出。合成测试使用临时配置和独立端口，不向真实 MQTT 发布模拟生命周期事件。

## 代码目录

- `app/XKnobCompanion.swift`：菜单栏、MQTT 设置窗口、钥匙串、原生 Hooks 转发及子进程生命周期。
- `app/build_app.py` / `app/test_bundle.py`：构建、自检和独立打包测试。
- `bridge/app_config.py` / `app_runtime.py`：配置迁移、历史清理及应用后端入口。
- `bridge/service.py` / `reducer.py`：本地鉴权入口、状态归约与 MQTT 发布。
- `bridge/install_hooks.py`：本项目注册识别、安装、卸载与混合组保留。
- `bridge/tunnel.py` / `account_probe.py`：可选 SSH 转发及真实账号元数据适配器。

原先的命令行脚本保留供开发和排障；菜单栏软件已接管服务时，不要额外运行旧 LaunchAgent 安装器或再启动一个手动引擎。
