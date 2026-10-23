# Firmware architecture

## Startup and runtime

`src/main.cpp` initializes the HAL, NVS, LVGL, application objects, and the tasks that service the display and motor. The motor update loop runs independently from LVGL so that display work does not replace the motor's high-frequency control loop.

```text
main.cpp
├── HAL: motor / encoder / LCD / power / NVS / MQTT
├── Port: LVGL display and input adapters
├── App: page manager, accounts, resources, and screens
└── Web: Wi-Fi/MQTT configuration and OTA upload
```

## Hardware layer (`src/hal/`)

- `motor.cpp` initializes SimpleFOC and runs the FOC and detent-position loop. It computes the active detent center, detects a crossed detent, applies a small dead zone near the center, and increases the boundary force at the ends of bounded ranges.
- `encoder.cpp` and `knob.cpp` read the angle sensor and button events.
- `lcd.cpp` controls the backlight and its timeout. A brightness of zero intentionally turns the backlight off; the application can still be running while the screen appears blank.
- `nvs.cpp` persists settings in ESP32 Preferences. Values entered through the local setup page replace the first-boot defaults.
- `mqtt.cpp` owns the Wi-Fi/MQTT worker, subscribes to the gateway state topic, and queues commands from the UI. The UI reads a snapshot through `HAL::smartled_state`.

The pin map and task-core assignments are in `src/config.h`. Treat motor parameters and GPIO assignments as board-specific settings; change them only when the hardware variant requires it.

## Application (`src/app/`)

Each screen is a page managed by `PageManager`. `AppFactory.cpp` maps page names to page classes. Pages build their widgets in `onViewLoad`, respond to LVGL events, and release timers/widgets in their unload hooks.

The menu launches pages such as Startup, Settings, Playground, System Info, Surface Dial, Home Assistant, and Smart LED. The Smart LED page in `Pages/Smartled/` displays a local preview, sends a command after a short rotation pause, and waits for the gateway's state update before marking the value confirmed. Short press changes between brightness, temperature, and power; long press exits.

The account system under `Accounts/` and `Utils/AccountSystem/` provides publish/notify/pull messaging for shared data such as motor and system configuration. Prefer using these established interfaces where existing pages depend on them.

## Display and input (`src/port/`)

`display.cpp` configures the LCD hardware. `lv_port/` translates LVGL flush and input callbacks to the display and rotary encoder. Keep hardware-specific drawing and touch/input adaptation in this layer rather than in page logic.

## Network and persistence

The local web setup handlers in `src/web/WiFiAsyncWebServer.cpp` save network values through the NVS API. Never log submitted passwords. MQTT is compiled behind `XK_MQTT`; when disabled, its public HAL methods return safe defaults so the UI can still build.

The knob expects the gateway to provide the topic and command contract documented in [MQTT.md](MQTT.md). A screen preview is not a confirmed device state: only the gateway's reported state marks an operation as synchronized.

## Adding a page

1. Add the page class under `src/app/Pages/<PageName>/`.
2. Register it in `src/app/Pages/AppFactory.cpp`.
3. Add a menu entry/event if users should open it from the menu.
4. Release timers, LVGL groups, and dynamically allocated objects in the matching page lifecycle hooks.
5. Run `pio run` and inspect both compiler output and the UI lifecycle on target hardware.
