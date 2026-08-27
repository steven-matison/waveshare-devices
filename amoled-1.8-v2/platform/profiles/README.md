# Board profiles

One `main`, many branded boards. A **profile** captures everything that differs
between physical AMOLED-1.8-V2 boards so the shared platform files stay generic
and the per-board specifics live here as data. Selected at build time:

```bash
BOARD_PROFILE=cloudera ./setup.sh      # (default: tuna-street)
```

`setup.sh` copies the generic overlay into the Brookesia work tree, then
`apply_profile.py <name>` patches that work tree from `profiles/<name>/`:

| Profile field | Patches (in the work tree, not the repo) |
|---|---|
| `brand.desktopBg` | `resource/shell/styles/shell.json` → `shell.desktop.bgColor` (launcher home background). `null` resets to `${color.bg.base}`, the generic upstream default |
| `brand.statusBar` | `shell.json` → the top status-strip palette: `bgColor`/`borderColor` on `shell.statusBar`, `textColor` on `shell.statusBrand`+`shell.statusTiny`, `mutedTextColor` on `shell.statusTitle`+`shell.statusTinyMuted`. `null`/absent → all reset to the generic upstream `${color.*}` tokens (the light-on-neutral default) |
| `brand.splashBg` | `resource/startup/screens/startup.json` → `style.bgColor`, the frame colour behind the boot splash. `null` → `#000000` (generic black) |
| `brand.accent` | reserved for wordmark/accent styling (not yet wired) |
| `splash` | `resource/startup/images/background.png` (the file named here, from this profile dir; absent → the overlay's generic tuna splash is kept) |
| `launcher.*` | `resource/shell/constants/portrait.json` → the `launcher*` grid constants |
| `launcher.hideFiles` | generated `main/board_profile.cmake` → `BOARD_HIDE_FILES` **compile definition** (read by `shell_app_launcher.cpp`, which lives in another component and can't include `main/` — so it rides as a build-wide define, not a header macro) |
| `apps` | `main/CMakeLists.txt` → `TUNASTREET_APP_PACKAGES` (which runtime tiles get staged) |
| `wifi.evictAp` | generated `main/board_profile.h` → `BOARD_WIFI_EVICT_AP` (read by `main.cpp`, same dir); the SSID to join stays in gitignored `sdkconfig.local` |
| `c2.baseUrl` | generated `examples/system/super/sdkconfig.profile` → `CONFIG_MICROFI_C2_HEARTBEAT_URL` / `_ACK_URL`; `setup.sh` cats it **after** `sdkconfig.microfi` so the board's URL wins. The agent reads these as Kconfig, not a macro |

Three generated artifacts, one per consumption mechanism: a **header** (`board_profile.h`, string macros for `main.cpp` which sits alongside it), a **CMake fragment** (`board_profile.cmake`, a compile definition for the launcher in a component `main/` can't reach), and a **Kconfig fragment** (`sdkconfig.profile`, for the agent which reads `CONFIG_*`). All three are re-generated on every `apply_profile.py` run and are `.gitignore`-worthy build output, never committed.

Design note: the base overlay files keep the generic (tuna-street) defaults; `apply_profile.py`
overwrites the profile-owned values on top, so the overlay's own values are the
default any board starts from. A board that needs a value the schema does
not yet cover is the signal to add a field here — never to hardcode it back into
a shared file (that is exactly the divergence #260 exists to end).

## Profiles

- **tuna-street** — the original board (WindowsDesktop / COM8). 3-col launcher, all
  four `tunastreet.*` apps, ATT LAN, direct EFM C2, tuna splash.
- **tuna-starlink** — StarlinkAI board (#252 / PR #1). 2×2, `tunastarlink.*` apps,
  open STARLINK WiFi, StarlinkAI C2 relay, amber-on-black splash.
- **cloudera** — Cloudera-branded board (#258, COM10). 2×2, Agent + RACING, orange
  desktop, Cloudera-cloud splash, ATT LAN, direct EFM C2.
