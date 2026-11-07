# Native mouse debug

An Immersive Relay build accepts `--tholus-mouse-debug` to start an ordinary
windowed race on Snow Tux Peak with one local driver. This mode does not enable
the relay, its API, phone roles or immersive video transport.

The host must supply `THOLUS_MOUSE_DEBUG_DATA_ROOT`: an existing canonical,
user-owned writable directory with private permissions (`0700`). The flag is
parsed before user configuration is loaded. Config, players/progress, scores,
logs, addons, screenshots, replays, texture caches and Grand Prix files use this
session directory through the existing FileManager. A missing or invalid root
stops startup; there is no fallback to normal game data. Normal launches ignore
this debug environment variable. The host removes the session directory only
after the game process has stopped.

- Move the cursor horizontally to steer; the window centre drives straight.
- Hold the left mouse button to accelerate.
- Hold the right mouse button to brake; braking overrides acceleration.
- Press the middle mouse button to use an item.

The mouse must remain over the focused game window. Losing focus, opening a
menu or losing the window dimensions releases every driving input. Normal
launches keep the existing input path.

macOS bundles advertise `TholusMouseDebugProtocolVersion = 1` when built with
`ENABLE_IMMERSIVE_RELAY`. Vanilla bundles advertise `0`. Hosts must check the
installed bundle before passing the flag. Existing releases need a rebuilt,
repackaged executable; editing their plist alone does not add this capability.

The pure `tholus_mouse_debug.controls` CTest regression covers steering limits,
button handling, braking precedence and neutral input after focus loss.
