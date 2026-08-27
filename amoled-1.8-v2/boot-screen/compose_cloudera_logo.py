#!/usr/bin/env python3
"""Cloudera boot splash v2 (#258): just the official CLOUDERA wordmark on the
brand orange, centered. No cloud badge, no RACING -- Steven's call. Uses the
logo Steven provided; samples its own orange so the canvas is a seamless match.
Run with /home/tunas/venv/bin/python."""
from PIL import Image

W, H = 368, 448
LOGO = "/home/tunas/.claude/image-cache/a6a415e2-33f4-4224-96c1-b40638cd4173/1.png"
OUT = "/tmp/claude-1000/-home-tunas-DesktopShare/a6a415e2-33f4-4224-96c1-b40638cd4173/scratchpad/cloudera_background.png"

logo = Image.open(LOGO).convert("RGB")
# sample the orange from a corner pixel (the logo's own background)
orange = logo.getpixel((2, 2))
print("sampled brand orange:", orange, "#%02X%02X%02X" % orange)

canvas = Image.new("RGB", (W, H), orange)
# scale the wordmark to ~86% of the panel width
target_w = int(W * 0.86)
scale = target_w / logo.width
target_h = int(logo.height * scale)
logo_r = logo.resize((target_w, target_h), Image.LANCZOS)
canvas.paste(logo_r, ((W - target_w) // 2, (H - target_h) // 2))
canvas.save(OUT)
print("wrote", OUT, canvas.size)
