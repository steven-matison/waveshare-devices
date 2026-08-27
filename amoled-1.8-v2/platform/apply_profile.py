#!/usr/bin/env python3
"""Apply a board profile to the Brookesia work tree (#260).

setup.sh copies the generic overlay into the work tree, then calls this to patch
the profile-owned values on top. Base overlay values for the patched fields are
don't-cares -- the profile wins. See profiles/README.md for the field map.

    python3 apply_profile.py <profile-name> [--work <brookesia-dir>]

Default work dir: $BROOKESIA_DIR or ~/esp/esp-brookesia. Patches are idempotent.
"""
import argparse
import json
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# Generic launcher-desktop background = upstream esp-brookesia's own default
# token (shell.json shell.desktop.bgColor at the pinned upstream). A profile
# with brand.desktopBg=null resets to this deterministically, so a board never
# inherits a previous profile's colour from a reused work tree.
DEFAULT_DESKTOP_BG = "${color.bg.base}"
# Generic boot-splash frame colour = upstream startup.json's own default (black).
DEFAULT_SPLASH_BG = "#000000"


def die(msg):
    print(f"apply_profile: ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def _sub_shell_style(t, block, key, value, path):
    """Set "shell.<block>": { ... "<key>": "..." } to value and return the text.
    Anchored on the unique "shell.<block>" key, a non-greedy .*? reaches that
    block's own <key> (every field we patch is an immediate member, so the first
    hit after the block key is the right one). We deliberately do NOT bound the
    span with [^}]: a reset value can be a "${color.x.y}" token whose own "}"
    would otherwise halt the scan before a later field in the same block. The
    closing quote in the block pattern keeps "statusTiny" from matching
    "statusTinyMuted"."""
    pat = re.compile(
        rf'("shell\.{re.escape(block)}"\s*:\s*\{{.*?"{re.escape(key)}"\s*:\s*)"[^"]*"',
        re.S)
    new, n = pat.subn(rf'\g<1>"{value}"', t)
    if n != 1:
        die(f"could not patch shell.{block}.{key} (matched {n} times) in {path}")
    return new


def patch_shell_styles(work, brand):
    """resource/shell/styles/shell.json -> desktop background + the status-bar
    palette (#260, reconciling #258's Cloudera look). brand.statusBar is a dict
    of four semantic colours or null; a null/absent value (any field) resets to
    the generic upstream token, so a board never inherits a previous profile's
    colour from a reused work tree."""
    p = os.path.join(work, "system/brookesia_system_super/resource/shell/styles/shell.json")
    if not os.path.isfile(p):
        die(f"shell.json not found at {p}")
    t = open(p).read()
    desktop = brand.get("desktopBg") or DEFAULT_DESKTOP_BG
    t = _sub_shell_style(t, "desktop", "bgColor", desktop, p)
    sb = brand.get("statusBar") or {}
    bg     = sb.get("bgColor")        or "${color.surface.base}"
    border = sb.get("borderColor")    or "${color.border.default}"
    text   = sb.get("textColor")      or "${color.text.default}"
    muted  = sb.get("mutedTextColor") or "${color.text.muted}"
    # bgColor + border live on shell.statusBar; the strip's text splits across
    # brand/tiny (primary) and title/tinyMuted (muted).
    t = _sub_shell_style(t, "statusBar",       "bgColor",     bg,     p)
    t = _sub_shell_style(t, "statusBar",       "borderColor", border, p)
    t = _sub_shell_style(t, "statusBrand",     "textColor",   text,   p)
    t = _sub_shell_style(t, "statusTiny",      "textColor",   text,   p)
    t = _sub_shell_style(t, "statusTitle",     "textColor",   muted,  p)
    t = _sub_shell_style(t, "statusTinyMuted", "textColor",   muted,  p)
    open(p, "w").write(t)
    print(f"  shell styles -> desktop {desktop}; statusBar bg {bg} text {text}/{muted}")


def patch_startup_bg(work, color):
    """resource/startup/screens/startup.json -> style.bgColor (the frame colour
    behind the boot splash image). Generic default is black; Cloudera fills the
    frame with brand orange."""
    p = os.path.join(work, "system/brookesia_system_super/resource/startup/screens/startup.json")
    if not os.path.isfile(p):
        die(f"startup.json not found at {p}")
    color = color or DEFAULT_SPLASH_BG
    t = open(p).read()
    pat = re.compile(r'("style"\s*:\s*\{[^}]*?"bgColor"\s*:\s*)"[^"]*"', re.S)
    new, n = pat.subn(rf'\g<1>"{color}"', t)
    if n != 1:
        die(f"could not patch startup style.bgColor (matched {n} times) in {p}")
    open(p, "w").write(new)
    print(f"  startup.bgColor -> {color}")


def patch_launcher(work, launcher):
    """resource/shell/constants/portrait.json -> launcher* geometry constants."""
    p = os.path.join(work, "system/brookesia_system_super/resource/shell/constants/portrait.json")
    if not os.path.isfile(p):
        die(f"portrait.json not found at {p}")
    t = open(p).read()
    mapping = {
        "launcherColumns": ("columns", int),
        "launcherGridWidth": ("gridWidth", str),
        "launcherItemWidth": ("itemWidth", str),
        "launcherItemHeight": ("itemHeight", str),
        "launcherItemGap": ("itemGap", str),
        "launcherIconTileSize": ("iconTileSize", str),
        "launcherIconSize": ("iconSize", str),
    }
    for key, (pkey, typ) in mapping.items():
        if pkey not in launcher:
            continue
        val = launcher[pkey]
        if typ is int:
            repl = str(int(val))
            pat = re.compile(rf'("{key}"\s*:\s*)\d+')
        else:
            repl = f'"{val}"'
            pat = re.compile(rf'("{key}"\s*:\s*)"[^"]*"')
        t, n = pat.subn(rf'\g<1>{repl}', t)
        if n < 1:
            die(f"could not patch {key} in {p}")
    open(p, "w").write(t)
    print(f"  launcher grid -> {launcher.get('columns')} cols "
          f"({launcher.get('itemWidth')} x {launcher.get('itemHeight')})")


def patch_apps(work, apps):
    """main/CMakeLists.txt -> TUNASTREET_APP_PACKAGES list."""
    p = os.path.join(work, "examples/system/super/main/CMakeLists.txt")
    if not os.path.isfile(p):
        die(f"CMakeLists.txt not found at {p}")
    t = open(p).read()
    block = "set(TUNASTREET_APP_PACKAGES\n" + "".join(f"    {a}\n" for a in apps) + ")"
    pat = re.compile(r"set\(TUNASTREET_APP_PACKAGES.*?\)", re.S)
    new, n = pat.subn(lambda _m: block, t, count=1)
    if n != 1:
        die(f"could not patch TUNASTREET_APP_PACKAGES in {p}")
    open(p, "w").write(new)
    print(f"  apps -> {', '.join(apps)}")


def copy_splash(work, prof_dir, splash):
    dst = os.path.join(work, "system/brookesia_system_super/resource/startup/images/background.png")
    src = os.path.join(prof_dir, splash)
    if not os.path.isfile(src):
        print(f"  splash: {src} not present -> leaving overlay background.png "
              f"(fill this profile's splash to override)")
        return
    shutil.copyfile(src, dst)
    print(f"  splash -> {os.path.relpath(src, HERE)}")


def write_header(work, prof):
    """Generated main/board_profile.h -- string macros consumed by main.cpp,
    which sits in the same dir so a plain #include resolves. BOARD_HIDE_FILES is
    deliberately NOT here: the launcher that reads it lives in the
    brookesia_system_super component, which has no include path back to main/,
    so it travels as a build-wide compile definition (write_launcher_cmake). C2
    is not here either -- the agent reads it as Kconfig (write_c2_sdkconfig)."""
    p = os.path.join(work, "examples/system/super/main/board_profile.h")
    evict = prof.get("wifi", {}).get("evictAp", "")
    body = (
        "// GENERATED by apply_profile.py -- do not edit. Board profile: "
        f"{prof['name']}\n"
        "#pragma once\n"
        f"#define BOARD_PROFILE_NAME \"{prof['name']}\"\n"
        f"#define BOARD_WIFI_EVICT_AP \"{evict}\"\n"
    )
    open(p, "w").write(body)
    print(f"  board_profile.h -> PROFILE={prof['name']} EVICT_AP='{evict}'")


def write_launcher_cmake(work, prof):
    """Generated main/board_profile.cmake -> per-board flags as plain CMake
    variables. examples/system/super/CMakeLists.txt includes it BEFORE project()
    (so component CMakeLists -- microfi_agent, agent_status_tile -- see the
    variables while they register) and then, after project(), turns each one
    into a build-wide compile definition for the C++ consumers that live in
    components with no include path back to main/ (the launcher, the status
    bar, main.cpp). Always writes a definite 0/1 so every consuming #if / if()
    is deterministic.

      BOARD_HIDE_FILES   -> shell_app_launcher.cpp (hide the stock Files tile)
      BOARD_HAS_BATTERY  -> the status-bar battery gauge (#261): boards with a
                            LiPo fitted read the AXP2101 and show charge; a
                            USB-only board (tuna-street) compiles it out.
      BOARD_HAS_AGENT    -> the MicroFi EFM agent + its native status tile
                            (#263). 0 builds both components as empty stubs
                            and main.cpp never starts the agent task -- a
                            battery board deployed off the LAN carries no
                            agent at all. Default (absent) is 1."""
    p = os.path.join(work, "examples/system/super/main/board_profile.cmake")
    hide = 1 if prof.get("launcher", {}).get("hideFiles") else 0
    battery = 1 if prof.get("hasBattery") else 0
    agent = 1 if prof.get("hasAgent", True) else 0
    body = (
        f"# GENERATED by apply_profile.py -- do not edit. Board profile: {prof['name']}\n"
        f"set(BOARD_HIDE_FILES {hide})\n"
        f"set(BOARD_HAS_BATTERY {battery})\n"
        f"set(BOARD_HAS_AGENT {agent})\n"
    )
    open(p, "w").write(body)
    print(f"  board_profile.cmake -> BOARD_HIDE_FILES={hide} BOARD_HAS_BATTERY={battery} "
          f"BOARD_HAS_AGENT={agent}")


def write_c2_sdkconfig(work, prof):
    """Generated examples/system/super/sdkconfig.profile -> per-board C2 URLs as
    Kconfig overrides. The MicroFi agent reads CONFIG_MICROFI_C2_*_URL (Kconfig),
    not a macro, so C2 must ride in sdkconfig. setup.sh cats this AFTER
    sdkconfig.microfi, so these win; sdkconfig.microfi keeps the generic
    (tuna-street) default as a fallback. A profile with no c2.baseUrl clears any
    stale file so it can't leak from a previous profile on a reused work tree."""
    base = prof.get("c2", {}).get("baseUrl", "").rstrip("/")
    p = os.path.join(work, "examples/system/super/sdkconfig.profile")
    if not base:
        if os.path.isfile(p):
            os.remove(p)
        print("  sdkconfig.profile -> (no c2.baseUrl; none written)")
        return
    body = (
        f"# GENERATED by apply_profile.py -- do not edit. Board profile: {prof['name']}\n"
        f"CONFIG_MICROFI_C2_HEARTBEAT_URL=\"{base}/efm/api/c2-protocol/heartbeat\"\n"
        f"CONFIG_MICROFI_C2_ACK_URL=\"{base}/efm/api/c2-protocol/acknowledge\"\n"
    )
    open(p, "w").write(body)
    print(f"  sdkconfig.profile -> C2 base {base}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("profile")
    ap.add_argument("--work", default=os.environ.get("BROOKESIA_DIR",
                    os.path.expanduser("~/esp/esp-brookesia")))
    args = ap.parse_args()

    prof_dir = os.path.join(HERE, "profiles", args.profile)
    prof_path = os.path.join(prof_dir, "profile.json")
    if not os.path.isfile(prof_path):
        die(f"no such profile: {prof_path}")
    prof = json.load(open(prof_path))
    work = args.work
    if not os.path.isdir(work):
        die(f"work tree not found: {work}")

    print(f"apply_profile: '{prof['name']}' -> {work}")
    patch_shell_styles(work, prof.get("brand", {}))
    patch_startup_bg(work, prof.get("brand", {}).get("splashBg"))
    copy_splash(work, prof_dir, prof.get("splash", "background.png"))
    patch_launcher(work, prof.get("launcher", {}))
    patch_apps(work, prof.get("apps", []))
    write_header(work, prof)
    write_launcher_cmake(work, prof)
    write_c2_sdkconfig(work, prof)
    print("apply_profile: done")


if __name__ == "__main__":
    main()
