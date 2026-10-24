import AppKit
import Security
import Foundation
import LocalAuthentication

let appID = "local.xknob.companion.app"
let configDirectory = ProcessInfo.processInfo.environment["XKNOB_CONFIG_DIR"].map { URL(fileURLWithPath: $0) }
    ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".config/xknob-companion")
let settingsURL = configDirectory.appendingPathComponent("settings.json")
let ingressPort = Int(ProcessInfo.processInfo.environment["XKNOB_INGRESS_PORT"] ?? "17345") ?? 17345

struct Settings: Codable {
    var host = "127.0.0.1"
    var port = 1883
    var username = ""
    var hostID = "example-host"
    var sshTarget = ""
    var localPort = 18830
    var tls = false
    var accountPoll = false

    func validate() throws {
        func matches(_ value: String, _ pattern: String) -> Bool { value.range(of: pattern, options: .regularExpression) != nil }
        guard !host.hasPrefix("-"), matches(host, "^[A-Za-z0-9_.:\\[\\]-]+$") else { throw AppError.message("请输入有效的 MQTT 服务器地址") }
        guard (1...65535).contains(port), (1...65535).contains(localPort) else { throw AppError.message("端口应在 1–65535 之间") }
        guard matches(hostID, "^[A-Za-z0-9_-]{1,48}$") else { throw AppError.message("配对标识只能含字母、数字、短横线和下划线，最长 48 字符") }
        guard sshTarget.isEmpty || (!sshTarget.hasPrefix("-") && matches(sshTarget, "^[A-Za-z0-9_.@:\\[\\]-]+$")) else { throw AppError.message("SSH 连接名称无效") }
    }
    func save() throws {
        try validate()
        try FileManager.default.createDirectory(at: configDirectory, withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted, .sortedKeys]
        try encoder.encode(self).write(to: settingsURL, options: .atomic)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: settingsURL.path)
    }
}

enum AppError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let text) = self { return text }; return nil }
}

// No shell interpolation. Passwords travel only via Keychain or a child stdin pipe.
func runProcess(_ executable: URL, _ arguments: [String], timeout: TimeInterval = 15) throws -> (Int32, Data) {
    let process = Process(); process.executableURL = executable; process.arguments = arguments
    let output = Pipe(); process.standardOutput = output; process.standardError = FileHandle.nullDevice
    let ended = DispatchSemaphore(value: 0)
    process.terminationHandler = { _ in ended.signal() }
    try process.run()
    if ended.wait(timeout: .now() + timeout) == .timedOut {
        if process.isRunning { process.terminate() }
        if ended.wait(timeout: .now() + 2) == .timedOut { kill(process.processIdentifier, SIGKILL) }
        throw AppError.message("操作超时，请重试")
    }
    return (process.terminationStatus, output.fileHandleForReading.readDataToEndOfFile())
}

enum PasswordStore {
    static let service = appID + ".credentials"
    static func read(username: String) -> String {
        let context = LAContext(); context.interactionNotAllowed = true
        let query: [String: Any] = [kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service, kSecAttrAccount as String: "mqtt",
            kSecReturnData as String: true, kSecUseAuthenticationContext as String: context]
        var result: CFTypeRef?
        if SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
           let data = result as? Data, let password = String(data: data, encoding: .utf8) { return password }
        // Migrate the existing trusted CLI access without writing a plaintext secret.
        guard !username.isEmpty, let (code, data) = try? runProcess(URL(fileURLWithPath: "/usr/bin/security"),
            ["find-generic-password", "-s", "local.xknob.mqtt", "-a", username, "-w"], timeout: 5), code == 0 else { return "" }
        var text = String(data: data, encoding: .utf8) ?? ""
        if text.hasSuffix("\n") { text.removeLast() }
        return text
    }
    static func save(_ password: String) throws {
        let query: [String: Any] = [kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service, kSecAttrAccount as String: "mqtt"]
        let attributes = [kSecValueData as String: Data(password.utf8)]
        var status = SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        if status == errSecItemNotFound {
            status = SecItemAdd(query.merging(attributes) { _, new in new } as CFDictionary, nil)
        }
        guard status == errSecSuccess else { throw AppError.message("无法保存到 macOS 钥匙串，请检查钥匙串是否已解锁") }
    }
}

enum HookForwarder {
    static func run() {
        // Always fail open. Never open the GUI, start MQTT, or forward prompt text.
        var data = Data()
        while data.count <= 1_048_576 {
            guard let chunk = try? FileHandle.standardInput.read(upToCount: 65536), !chunk.isEmpty else { break }
            data.append(chunk)
        }
        guard data.count <= 1_048_576,
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        let stop = object["hook_event_name"] as? String == "Stop"
        defer { if stop { print("{}") } }
        let allowed = ["hook_event_name", "session_id", "turn_id", "source", "tool_name"]
        let safe = object.filter { allowed.contains($0.key) && $0.value is String }
        guard let body = try? JSONSerialization.data(withJSONObject: safe), body.count <= 4096,
              let token = try? String(contentsOf: configDirectory.appendingPathComponent("bridge.key"), encoding: .ascii) else { return }
        var request = URLRequest(url: URL(string: "http://127.0.0.1:\(ingressPort)/v1/hook")!)
        request.httpMethod = "POST"; request.httpBody = body; request.timeoutInterval = 0.35
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.setValue(token.trimmingCharacters(in: .whitespacesAndNewlines), forHTTPHeaderField: "X-Bridge-Key")
        let done = DispatchSemaphore(value: 0)
        let task = URLSession.shared.dataTask(with: request) { _, _, _ in done.signal() }
        task.resume()
        if done.wait(timeout: .now() + 0.6) == .timedOut { task.cancel() }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate {
    var statusItem: NSStatusItem!
    var statusLine = NSMenuItem(title: "正在启动…", action: nil, keyEquivalent: "")
    var hooksLine = NSMenuItem(title: "Hooks：检查中", action: nil, keyEquivalent: "")
    var startItem: NSMenuItem!
    var stopItem: NSMenuItem!
    var window: NSWindow?
    var hostField = NSTextField(string: "")
    var portField = NSTextField(string: "")
    var userField = NSTextField(string: "")
    var passwordField = NSSecureTextField(string: "")
    var hostIDField = NSTextField(string: "")
    var sshField = NSTextField(string: "")
    var tlsButton = NSButton(checkboxWithTitle: "使用 TLS 加密连接", target: nil, action: nil)
    var advancedButton: NSButton!
    var advancedStack: NSStackView!
    var saveButton: NSButton!
    var statusLabel = NSTextField(labelWithString: "正在启动…")
    var settings = Settings()
    var backend: Process?
    var timer: Timer?
    var quitting = false
    var busy = false
    var checkInFlight = false
    let queue = DispatchQueue(label: "local.xknob.app.operations")
    var backendURL: URL { Bundle.main.resourceURL!.appendingPathComponent("backend/XKnobBridge") }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        let mainMenu = NSMenu(); let appMenuItem = NSMenuItem(); mainMenu.addItem(appMenuItem)
        let appMenu = NSMenu(); appMenu.addItem(item("MQTT 设置…", #selector(showSettings), ","))
        appMenu.addItem(.separator()); appMenu.addItem(item("退出 X-Knob Companion", #selector(quit), "q"))
        appMenuItem.submenu = appMenu; NSApp.mainMenu = mainMenu
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        let icon = NSImage(systemSymbolName: "dial.medium.fill", accessibilityDescription: "X-Knob Companion")
            ?? NSImage(systemSymbolName: "circle.circle.fill", accessibilityDescription: "X-Knob Companion")
        icon?.isTemplate = true; statusItem.button?.image = icon
        let menu = NSMenu(); menu.addItem(NSMenuItem(title: "X-Knob Companion", action: nil, keyEquivalent: ""))
        menu.addItem(statusLine); menu.addItem(hooksLine); menu.addItem(.separator())
        menu.addItem(item("MQTT 设置…", #selector(showSettings), ","))
        startItem = item("重新连接", #selector(reconnect)); menu.addItem(startItem)
        stopItem = item("停止连接", #selector(stopConnection)); menu.addItem(stopItem)
        menu.addItem(.separator()); menu.addItem(item("安装 Hooks…", #selector(installHooks)))
        menu.addItem(item("卸载 Hooks", #selector(uninstallHooks)))
        menu.addItem(.separator()); menu.addItem(item("退出 X-Knob Companion", #selector(quit), "q"))
        statusItem.menu = menu
        timer = Timer.scheduledTimer(withTimeInterval: 3, repeats: true) { [weak self] _ in self?.pollHealth() }
        setBusy(true)
        queue.async {
            do {
                _ = try self.backendCommand(["migrate"])
                let loaded = try JSONDecoder().decode(Settings.self, from: Data(contentsOf: settingsURL))
                try loaded.validate()
                DispatchQueue.main.async {
                    self.settings = loaded; self.setBusy(false); self.reconnect(); self.showSettings()
                }
            } catch { DispatchQueue.main.async { self.setBusy(false); self.showSettings(); self.showError(error) } }
        }
    }
    func item(_ title: String, _ action: Selector, _ key: String = "") -> NSMenuItem {
        let item = NSMenuItem(title: title, action: action, keyEquivalent: key); item.target = self; return item
    }
    func backendCommand(_ arguments: [String]) throws -> [String: Any] {
        let (code, data) = try runProcess(backendURL, arguments)
        let result = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any]
        guard code == 0 else { throw AppError.message(result?["error"] as? String ?? "操作未完成，请检查配置后重试") }
        return result ?? [:]
    }
    func setBusy(_ value: Bool) {
        busy = value; startItem?.isEnabled = !value; stopItem?.isEnabled = !value && backend != nil
        saveButton?.isEnabled = !value
    }
    func setStatus(_ text: String, connected: Bool = false) {
        statusLine.title = text; statusLabel.stringValue = text
        statusItem.button?.contentTintColor = connected ? .systemGreen : .secondaryLabelColor
    }
    @objc func reconnect() {
        guard !busy else { return }; setBusy(true); setStatus("正在连接 MQTT…")
        let current = settings
        queue.async { [self] in
            self.stopBackend()
            do {
                let password = PasswordStore.read(username: current.username)
                let process = Process(); process.executableURL = self.backendURL
                process.arguments = ["serve", "--parent-pid", String(getpid())]
                var environment = ProcessInfo.processInfo.environment
                environment.removeValue(forKey: "XKNOB_MQTT_PASSWORD")
                environment["PATH"] = "/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"
                if environment["XKNOB_CODEX_BIN"] == nil,
                   FileManager.default.isExecutableFile(atPath: "/Applications/Codex.app/Contents/Resources/codex") {
                    environment["XKNOB_CODEX_BIN"] = "/Applications/Codex.app/Contents/Resources/codex"
                }
                process.environment = environment
                let input = Pipe(); process.standardInput = input
                process.standardOutput = FileHandle.nullDevice; process.standardError = FileHandle.nullDevice
                process.terminationHandler = { [weak self] p in DispatchQueue.main.async {
                    guard let self = self, self.backend === p else { return }
                    self.backend = nil
                    if !self.quitting { self.setStatus("连接已停止，请检查设置后重新连接"); self.stopItem.isEnabled = false }
                }}
                try process.run()
                input.fileHandleForWriting.write(try JSONSerialization.data(withJSONObject: ["password": password]))
                try input.fileHandleForWriting.close()
                DispatchQueue.main.async {
                    self.backend = process.isRunning ? process : nil; self.setBusy(false)
                    if process.isRunning { self.pollHealth() }
                    else { self.setStatus("连接未启动，请检查设置或端口占用") }
                }
            } catch { DispatchQueue.main.async { self.setBusy(false); self.showError(error) } }
        }
    }
    func stopBackend() {
        let process = backend
        DispatchQueue.main.sync { self.backend = nil }
        if let process = process, process.isRunning {
            process.terminate()
            for _ in 0..<100 { if !process.isRunning { break }; Thread.sleep(forTimeInterval: 0.1) }
            if process.isRunning { kill(process.processIdentifier, SIGKILL) }
            process.waitUntilExit()
        }
    }
    @objc func stopConnection() {
        guard !busy else { return }; setBusy(true)
        queue.async { self.stopBackend(); DispatchQueue.main.async { self.setBusy(false); self.setStatus("连接已停止"); self.stopItem.isEnabled = false } }
    }
    func pollHealth() {
        guard !busy, !checkInFlight else { return }; checkInFlight = true
        var request = URLRequest(url: URL(string: "http://127.0.0.1:\(ingressPort)/healthz")!); request.timeoutInterval = 1
        URLSession.shared.dataTask(with: request) { [weak self] data, _, _ in DispatchQueue.main.async {
            guard let self = self else { return }; self.checkInFlight = false
            guard let process = self.backend else { return }
            if let data = data, let health = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
               (health["pid"] as? Int) == Int(process.processIdentifier) {
                let connected = health["mqtt_connected"] as? Bool == true
                let message = connected ? "MQTT 已连接" : (health["mqtt_error"] as? String).flatMap { $0.isEmpty ? nil : $0 } ?? "MQTT 连接中…"
                self.setStatus(message, connected: connected)
            } else { self.setStatus("正在启动连接…") }
        }}.resume()
        queue.async {
            // Reading a tiny local JSON file avoids launching Python every poll.
            let path = ProcessInfo.processInfo.environment["XKNOB_HOOKS_CONFIG"].map { URL(fileURLWithPath: $0) }
                ?? FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".codex/hooks.json")
            let object = (try? Data(contentsOf: path)).flatMap { try? JSONSerialization.jsonObject(with: $0) as? [String:Any] }
            let groups = object?["hooks"] as? [String:Any] ?? [:]
            let count = groups.values.compactMap { $0 as? [[String:Any]] }.flatMap { $0 }
                .flatMap { $0["hooks"] as? [[String:Any]] ?? [] }.filter {
                    let command = $0["command"] as? String ?? ""
                    return (command.contains("XKnobCompanion") && command.hasSuffix(" --hook")) ||
                        (command.contains("/X-Knob/") && command.contains("/bridge/hook_sender.py"))
                }.count
            DispatchQueue.main.async { self.hooksLine.title = count == 0 ? "Hooks：未安装" : "Hooks：已安装（\(count) 条）" }
        }
    }
    @objc func installHooks() {
        guard !busy else { return }
        let alert = NSAlert(); alert.messageText = "安装 Codex Hooks？"
        alert.informativeText = "仅安装会话开始、发送请求、等待审批、本轮结束、中断和会话结束六类事件。会替换本项目旧注册，保留其他 Hooks。原配置会自动备份。"
        alert.addButton(withTitle: "安装"); alert.addButton(withTitle: "取消")
        if alert.runModal() == .alertFirstButtonReturn { changeHooks(true) }
    }
    @objc func uninstallHooks() {
        guard !busy else { return }
        let alert = NSAlert(); alert.messageText = "卸载本项目 Hooks？"
        alert.informativeText = "仅移除 X-Knob 注册，保留其他 Hooks 和 MQTT 设置，原配置会自动备份。"
        alert.addButton(withTitle: "卸载"); alert.addButton(withTitle: "取消")
        if alert.runModal() == .alertFirstButtonReturn { changeHooks(false) }
    }
    func changeHooks(_ install: Bool) {
        setBusy(true)
        queue.async {
            do {
                var args = [install ? "install-hooks" : "uninstall-hooks"]
                if install { args += ["--executable", Bundle.main.executableURL!.path] }
                let result = try self.backendCommand(args)
                DispatchQueue.main.async {
                    self.setBusy(false); self.hooksLine.title = "Hooks：\(install ? "已安装" : "未安装")"
                    let alert = NSAlert(); alert.messageText = install ? "Hooks 已安装" : "Hooks 已卸载"
                    alert.informativeText = install ? "Codex 可能需要重新加载配置并确认信任。当前已保存 \(result["after"] as? Int ?? 0) 条注册。" : "MQTT 设置与其他项目的 Hooks 均已保留。"
                    alert.runModal()
                }
            } catch { DispatchQueue.main.async { self.setBusy(false); self.showError(error) } }
        }
    }
    func row(_ title: String, _ control: NSView) -> NSView {
        let label = NSTextField(labelWithString: title); label.alignment = .right
        label.widthAnchor.constraint(equalToConstant: 104).isActive = true
        let stack = NSStackView(views: [label, control]); stack.orientation = .horizontal
        stack.spacing = 14; stack.alignment = .centerY
        control.widthAnchor.constraint(equalToConstant: 300).isActive = true
        return stack
    }
    @objc func showSettings() {
        if window == nil {
            let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 510, height: 515),
                styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
            window.title = "X-Knob · MQTT 设置"; window.isReleasedWhenClosed = false; window.delegate = self
            self.window = window
            let heading = NSTextField(labelWithString: "连接你的 MQTT")
            heading.font = .systemFont(ofSize: 21, weight: .semibold)
            let note = NSTextField(labelWithString: "关闭设置窗口后继续在菜单栏运行。")
            note.textColor = .secondaryLabelColor
            passwordField.placeholderString = "留空保留已保存的密码"
            advancedButton = NSButton(checkboxWithTitle: "高级连接设置", target: self, action: #selector(toggleAdvanced))
            let explanation = NSTextField(wrappingLabelWithString: "配对标识与旋钮保持一致。使用 SSH 时，地址和端口指远端 MQTT；软件负责建立隧道。")
            explanation.textColor = .secondaryLabelColor; explanation.font = .systemFont(ofSize: 11)
            advancedStack = NSStackView(views: [row("配对标识",hostIDField),row("SSH 连接",sshField),explanation])
            advancedStack.orientation = .vertical; advancedStack.alignment = .leading; advancedStack.spacing = 10
            saveButton = NSButton(title: "保存并连接", target: self, action: #selector(saveSettings)); saveButton.bezelStyle = .rounded; saveButton.keyEquivalent = "\r"
            let close = NSButton(title: "关闭", target: window, action: #selector(NSWindow.performClose(_:))); close.bezelStyle = .rounded
            let quitButton = NSButton(title: "退出软件", target: self, action: #selector(quit)); quitButton.bezelStyle = .rounded
            let buttons = NSStackView(views: [quitButton,close,saveButton]); buttons.orientation = .horizontal; buttons.spacing = 10
            statusLabel.textColor = .secondaryLabelColor; statusLabel.font = .systemFont(ofSize: 12)
            let content = NSStackView(views: [heading,note,row("服务器地址",hostField),row("端口",portField),row("用户名",userField),row("密码",passwordField),row("",tlsButton),advancedButton,advancedStack,statusLabel,buttons])
            content.orientation = .vertical; content.alignment = .leading; content.spacing = 13
            content.translatesAutoresizingMaskIntoConstraints = false
            window.contentView!.addSubview(content)
            NSLayoutConstraint.activate([content.leadingAnchor.constraint(equalTo: window.contentView!.leadingAnchor,constant: 26),content.topAnchor.constraint(equalTo: window.contentView!.topAnchor,constant: 25),content.trailingAnchor.constraint(equalTo: window.contentView!.trailingAnchor,constant: -26)])
            window.center()
        }
        hostField.stringValue = settings.host; portField.stringValue = String(settings.port)
        userField.stringValue = settings.username; passwordField.stringValue = ""
        hostIDField.stringValue = settings.hostID; sshField.stringValue = settings.sshTarget
        tlsButton.state = settings.tls ? .on : .off
        advancedButton.state = settings.sshTarget.isEmpty ? .off : .on; toggleAdvanced()
        saveButton.isEnabled = !busy
        window!.makeKeyAndOrderFront(nil); NSApp.activate(ignoringOtherApps: true)
    }
    @objc func toggleAdvanced() {
        advancedStack.isHidden = advancedButton.state != .on
        if let stack = window?.contentView?.subviews.first as? NSStackView {
            stack.layoutSubtreeIfNeeded()
            window?.setContentSize(NSSize(width: 510, height: max(460,stack.fittingSize.height+52)))
        }
    }
    @objc func saveSettings() {
        guard !busy else { return }
        var updated = settings
        updated.host = hostField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let port = Int(portField.stringValue) else { showError(AppError.message("端口必须为数字")); return }
        updated.port = port; updated.username = userField.stringValue
        updated.hostID = hostIDField.stringValue; updated.sshTarget = sshField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        updated.tls = tlsButton.state == .on
        do {
            try updated.validate()
            if !passwordField.stringValue.isEmpty { try PasswordStore.save(passwordField.stringValue) }
            try updated.save(); settings = updated; passwordField.stringValue = ""; reconnect()
        } catch { showError(error) }
    }
    func showError(_ error: Error) {
        setStatus("操作未完成")
        let alert = NSAlert(); alert.messageText = "暂时无法完成操作"; alert.informativeText = error.localizedDescription; alert.runModal()
    }
    @objc func quit() {
        guard !quitting else { return }; quitting = true; timer?.invalidate(); setBusy(true)
        queue.async { self.stopBackend(); DispatchQueue.main.async { NSApp.terminate(nil) } }
    }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool { showSettings(); return true }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationWillTerminate(_ notification: Notification) { if let p = backend, p.isRunning { p.terminate() } }
}

if CommandLine.arguments.contains("--hook") {
    HookForwarder.run()
} else if CommandLine.arguments.contains("--self-test") {
    var config = Settings(); try config.validate()
    config.port = 0
    do { try config.validate(); fatalError("Invalid port accepted") } catch {}
    config.port = 1883; config.sshTarget = "-ProxyCommand=bad"
    do { try config.validate(); fatalError("Invalid SSH target accepted") } catch {}
    precondition(Bundle.main.object(forInfoDictionaryKey: "LSUIElement") as? Bool == true)
    print("PASS: app configuration validation; LSUIElement enabled; native hook helper available")
} else {
    let app = NSApplication.shared
    let delegate = AppDelegate(); app.delegate = delegate; app.run()
}
