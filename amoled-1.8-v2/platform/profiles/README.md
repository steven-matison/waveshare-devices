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
| `brand.desktopBg` | `resource/shell/styles/shell.json` → `shell.desktop.bgColor` (the launcher home background) |
| `brand.accent` | reserved for wordmark/accent styling (not yet wired) |
| `splash` | `resource/startup/images/background.png` (the file named here, from this profile dir) |
| `launcher.*` | `resource/shell/constants/portrait.json` → the `launcher*` grid constants |
| `launcher.hideFiles` | generated `main/board_profile.h` → `BOARD_HIDE_FILES` (read by `shell_app_launcher.cpp`) |
| `apps` | `main/CMakeLists.txt` → `TUNASTREET_APP_PACKAGES` (which runtime tiles get staged) |
| `wifi.evictAp` | generated `board_profile.h` → `BOARD_WIFI_EVICT_AP` (read by `main.cpp`); the SSID to join stays in gitignored `sdkconfig.local` |
| `c2.baseUrl` | generated `board_profile.h` → `BOARD_C2_BASE_URL` + the `sdkconfig` C2 heartbeat/ack fragment |

Design note: the base overlay files keep whatever generic defaults; `apply_profile.py`
overwrites the profile-owned values on top, so the overlay's own values are
don't-cares for the patched fields. A board that needs a value the schema does
not yet cover is the signal to add a field here — never to hardcode it back into
a shared file (that is exactly the divergence #260 exists to end).

## Profiles

- **tuna-street** — the original board (WindowsDesktop / COM8). 3-col launcher, all
  four `tunastreet.*` apps, ATT LAN, direct EFM C2, tuna splash.
- **tuna-starlink** — StarlinkAI board (#252 / PR #1). 2×2, `tunastarlink.*` apps,
  open STARLINK WiFi, StarlinkAI C2 relay, amber-on-black splash.
- **cloudera** — Cloudera-branded board (#258, COM10). 2×2, Agent + RACING, orange
  desktop, Cloudera-cloud splash, ATT LAN, direct EFM C2.
