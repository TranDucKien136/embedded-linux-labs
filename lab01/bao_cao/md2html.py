#!/usr/bin/env python3
"""Chuyen bao cao Markdown (tap con don gian) sang HTML de LibreOffice xuat PDF.
Ho tro: heading, bang, code fence, blockquote, list, hr, anh, **dam**, `code`.
Dung: python3 md2html.py input.md output.html
"""
import html
import os
import re
import struct
import sys

src, dst = sys.argv[1], sys.argv[2]
base = os.path.dirname(os.path.abspath(src))
lines = open(src, encoding="utf-8").read().split("\n")

CSS = """
body { font-family: 'DejaVu Sans', sans-serif; font-size: 10pt; line-height: 1.35; }
h1 { font-family: 'DejaVu Sans', sans-serif; font-size: 17pt; color: #1F3864; margin-bottom: 2pt; }
h2 { font-family: 'DejaVu Sans', sans-serif; font-size: 13.5pt; color: #1F3864; border-bottom: 1px solid #8EAADB; margin-top: 14pt; }
h3 { font-family: 'DejaVu Sans', sans-serif; font-size: 11.5pt; color: #2E75B6; margin-top: 10pt; }
table { border-collapse: collapse; width: 100%; margin: 4pt 0; }
th, td { border: 1px solid #999; padding: 2pt 4pt; font-size: 9pt; vertical-align: top; margin: 0; }
p { margin: 3pt 0; }
th { background: #D9E2F3; }
pre { font-family: 'DejaVu Sans Mono', monospace; font-size: 7.5pt; background: #F2F2F2;
      border: 1px solid #ccc; padding: 4pt; white-space: pre-wrap; }
code { font-family: 'DejaVu Sans Mono', monospace; font-size: 8.5pt; }
blockquote { border-left: 3px solid #2E75B6; background: #F3F7FC; margin: 4pt 0; padding: 2pt 6pt; }
img { max-width: 100%; }
"""


def inline(t):
    # Tach `code` ra placeholder truoc de **dam** co the bao quanh `code`
    codes = []

    def keep(m):
        codes.append("<code>" + html.escape(m.group(1)) + "</code>")
        return f"\x00{len(codes) - 1}\x00"

    t = re.sub(r"`([^`]+)`", keep, t)
    t = html.escape(t)
    t = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", t)
    t = re.sub(r"(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])", r"<i>\1</i>", t)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], t)


def cells(row):
    return [c.strip() for c in row.strip().strip("|").split("|")]


out = []
i = 0
para = []


def flush():
    global para
    if para:
        out.append("<p>" + inline(" ".join(para)) + "</p>")
        para = []


while i < len(lines):
    ln = lines[i]
    s = ln.strip()
    if s.startswith("```"):
        flush()
        i += 1
        buf = []
        while i < len(lines) and not lines[i].strip().startswith("```"):
            buf.append(lines[i])
            i += 1
        out.append("<pre>" + html.escape("\n".join(buf)) + "</pre>")
        i += 1
        continue
    m = re.match(r"^(#{1,6})\s+(.*)", s)
    if m:
        flush()
        n = len(m.group(1))
        out.append(f"<h{n}>{inline(m.group(2))}</h{n}>")
        i += 1
        continue
    if s == "---":
        flush()
        i += 1
        continue
    m = re.match(r"^!\[(.*?)\]\((.*?)\)$", s)
    if m:
        flush()
        path = os.path.abspath(os.path.join(base, m.group(2)))
        # PNG header: width/height o byte 16..24 -> giu dung ti le, rong 16 cm
        with open(path, "rb") as im:
            w, h = struct.unpack(">II", im.read(24)[16:24])
        wcm = 16.0
        hcm = wcm * h / w
        out.append(f'<p align="center"><img src="file://{path}" alt="{html.escape(m.group(1))}" '
                   f'style="width:{wcm:.1f}cm; height:{hcm:.1f}cm"/>'
                   f'<br/><i>{html.escape(m.group(1))}</i></p>')
        i += 1
        continue
    if s.startswith("|") and i + 1 < len(lines) and re.match(r"^\|[\s:|-]+\|$", lines[i + 1].strip()):
        flush()
        head = cells(s)
        i += 2
        rows = []
        while i < len(lines) and lines[i].strip().startswith("|"):
            rows.append(cells(lines[i]))
            i += 1
        t = ['<table border="1" cellspacing="0" cellpadding="3" width="100%"><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr>"]
        for r in rows:
            t.append("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>")
        out.append("".join(t) + "</table>")
        continue
    if s.startswith(">"):
        flush()
        buf = []
        while i < len(lines) and lines[i].strip().startswith(">"):
            buf.append(lines[i].strip()[1:].strip())
            i += 1
        out.append("<blockquote>" + inline(" ".join(buf)) + "</blockquote>")
        continue
    m = re.match(r"^(\s*)([-*]|\d+\.)\s+(.*)", ln)
    if m:
        flush()
        tag = "ol" if m.group(2)[0].isdigit() else "ul"
        items = []
        while i < len(lines):
            mm = re.match(r"^\s*([-*]|\d+\.)\s+(.*)", lines[i])
            if mm:
                items.append(mm.group(2))
            elif lines[i].startswith("   ") and lines[i].strip() and items:
                items[-1] += " " + lines[i].strip()
            else:
                break
            i += 1
        out.append(f"<{tag}>" + "".join(f"<li>{inline(x)}</li>" for x in items) + f"</{tag}>")
        continue
    if not s:
        flush()
        i += 1
        continue
    para.append(s)
    i += 1
flush()

with open(dst, "w", encoding="utf-8") as f:
    f.write('<!DOCTYPE html><html><head><meta charset="utf-8"><title>LAB-03</title>'
            f"<style>{CSS}</style></head><body>\n" + "\n".join(out) + "\n</body></html>\n")
print("wrote", dst)
