# Native mouse debug

An Immersive Relay build accepts `--tholus-mouse-debug` to start an ordinary
windowed race on Snow Tux Peak with one local driver. This mode does not enable
the relay, its API, phone roles or immersive video transport.

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
