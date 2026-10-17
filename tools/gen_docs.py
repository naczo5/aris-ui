#!/usr/bin/env python3
"""aris-ui documentation site generator.

Converts docs-src/*.md into docs/*.html (shared template, dark theme) and
emits docs/llms.txt + docs/llms-full.txt for LLM-friendly fetching.

Stdlib only — no markdown package required. Supported Markdown: headings,
fenced code blocks, bullet/numbered lists, pipe tables, inline code/bold/
italic/links. Anything else falls back to plain paragraphs.

Usage: python3 tools/gen_docs.py
"""
import html
import os
import re
import sys
from datetime import date

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs-src")
OUT = os.path.join(ROOT, "docs")

REPO = "https://github.com/naczo5/aris-ui"

# Order defines site navigation.
PAGES = [
    ("getting-started.md", "Getting Started"),
    ("core-concepts.md",   "Core Concepts"),
    ("widgets.md",         "Widgets"),
    ("layout.md",          "Layout"),
    ("rendering.md",       "Rendering"),
    ("fonts.md",           "Fonts"),
    ("agent-tooling.md",   "Agent Tooling"),
]

CSS = """/* aris-ui docs — modern glassy dark theme, vibrant neon/steel accents */
@import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap');

:root {
  --bg: #070a10;          /* deep midnight obsidian */
  --bg2: #0d121c;         /* raised surface */
  --panel: #111824;       /* card / panel surface */
  --panel-hover: #162030;
  --line: rgba(255, 255, 255, 0.08); /* subtle borders */
  --line-strong: rgba(255, 255, 255, 0.16);
  --text: #f1f5f9;        /* high-legibility primary text */
  --dim: #94a3b8;         /* secondary slate text */
  --dimmer: #64748b;
  --accent: #38bdf8;      /* electric sky cyan */
  --accent-soft: #7dd3fc;
  --accent-glow: rgba(56, 189, 248, 0.25);
  --emerald: #10b981;
  --purple: #a855f7;
  --amber: #f59e0b;
  --mono: "JetBrains Mono", ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  --sans: "Inter", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.68 var(--sans);
  -webkit-font-smoothing: antialiased;
  background-image:
    radial-gradient(ellipse 80% 50% at 50% -20%, rgba(56, 189, 248, 0.12), transparent 70%),
    radial-gradient(circle at 100% 100%, rgba(168, 85, 247, 0.06), transparent 50%);
  background-attachment: fixed;
}
a { color: var(--accent); text-decoration: none; transition: color 0.18s ease; }
a:hover { color: var(--accent-soft); text-decoration: none; }
code {
  font-family: var(--mono); font-size: 0.86em;
  background: rgba(255, 255, 255, 0.05); border: 1px solid var(--line);
  border-radius: 6px; padding: 0.12em 0.45em; color: #bae6fd;
}
pre {
  background: var(--bg2); border: 1px solid var(--line);
  border-radius: 12px; padding: 18px 22px; overflow-x: auto;
  box-shadow: 0 10px 30px rgba(0,0,0,0.35);
}
pre code { background: none; border: none; padding: 0; font-size: 0.85em; line-height: 1.6; color: #e2e8f0; }
h1, h2, h3, h4 { font-weight: 700; letter-spacing: -0.02em; color: #ffffff; }
h1 { font-size: 2.1em; margin: 0.3em 0 0.5em; line-height: 1.2; }
h2 {
  font-size: 1.38em; margin: 2.4em 0 0.7em; padding-top: 1.1em;
  border-top: 1px solid var(--line);
}
h3 { font-size: 1.12em; margin: 1.8em 0 0.5em; }
p { margin: 0.8em 0; color: #cbd5e1; }
ul, ol { padding-left: 1.4em; color: #cbd5e1; }
li { margin: 0.35em 0; }
table {
  border-collapse: separate; border-spacing: 0;
  margin: 1.4em 0; width: 100%; font-size: 0.92em;
  border: 1px solid var(--line); border-radius: 10px; overflow: hidden;
}
th, td { border-bottom: 1px solid var(--line); padding: 10px 14px; text-align: left; }
tr:last-child td { border-bottom: none; }
th { background: var(--bg2); color: #f8fafc; font-weight: 600; font-size: 0.88em; text-transform: uppercase; letter-spacing: 0.04em; }
tr:hover td { background: rgba(255, 255, 255, 0.02); }
hr { border: none; border-top: 1px solid var(--line); margin: 2.4em 0; }

/* layout */
.site { max-width: 1040px; margin: 0 auto; padding: 0 24px; }
header.site {
  display: flex; align-items: center; gap: 28px;
  padding: 16px 28px; border-bottom: 1px solid var(--line);
  position: sticky; top: 0; background: rgba(7, 10, 16, 0.82);
  backdrop-filter: blur(18px); -webkit-backdrop-filter: blur(18px); z-index: 100;
}
.logo {
  font-weight: 700; font-size: 18px; letter-spacing: -0.02em; color: #ffffff;
  display: flex; align-items: center; gap: 4px;
}
.logo:hover { text-decoration: none; }
.logo .dot { color: var(--accent); text-shadow: 0 0 10px var(--accent); }
header.site nav { display: flex; gap: 20px; flex-wrap: wrap; margin-left: auto; align-items: center; }
header.site nav a {
  color: var(--dim); font-size: 14.5px; font-weight: 500;
  padding: 6px 10px; border-radius: 6px; transition: all 0.18s;
}
header.site nav a:hover { color: #ffffff; background: rgba(255, 255, 255, 0.06); text-decoration: none; }
main.site { padding: 48px 24px 32px; }
footer.site {
  border-top: 1px solid var(--line); color: var(--dimmer);
  font-size: 13.5px; padding: 28px 24px 48px; margin-top: 80px;
  display: flex; justify-content: space-between; flex-wrap: wrap; gap: 12px;
}
.toc {
  background: rgba(255, 255, 255, 0.02); border: 1px solid var(--line);
  border-radius: 10px; padding: 12px 18px; color: var(--dim);
  font-size: 13.5px; margin-bottom: 2.2em; display: flex; flex-wrap: wrap; gap: 14px; align-items: center;
}
.toc a { color: var(--dim); white-space: nowrap; }
.toc a:hover { color: var(--accent); }
"""

HEADER = """<header class="site">
  <a class="logo" href="index.html">aris-ui<span class="dot">.</span></a>
  <nav>
    {nav}
    <a href="{repo}" target="_blank" rel="noopener">GitHub ↗</a>
  </nav>
</header>"""

FOOTER = """<footer class="site">
  <span><b>aris-ui</b> — MIT licensed · C++20 · OpenGL 3.3 · High-Performance Overlays</span>
  <span>Updated {date} · <a href="{repo}">source code</a></span>
</footer>"""


# --------------------------------------------------------------------------
# minimal Markdown → HTML
# --------------------------------------------------------------------------
def inline(text: str) -> str:
    """Inline code, bold, italic, links — on already-escaped text."""
    # `code` first so later rules don't touch code contents
    parts = re.split(r"(`[^`]+`)", text)
    out = []
    for i, part in enumerate(parts):
        if i % 2 == 1:
            out.append(f"<code>{part[1:-1]}</code>")
            continue
        part = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", part)
        part = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", part)
        part = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', part)
        out.append(part)
    return "".join(out)


def md_to_html(md: str) -> str:
    lines = md.splitlines()
    html_lines, i = [], 0
    list_stack = []          # 'ul' | 'ol'
    para: list = []          # accumulated plain-text lines

    def close_list():
        while list_stack:
            html_lines.append(f"</{list_stack.pop()}>")

    def flush_para():
        if para:
            html_lines.append(f"<p>{inline(html.escape(' '.join(para)))}</p>")
            para.clear()

    while i < len(lines):
        line = lines[i]

        if line.startswith("```"):
            flush_para(); close_list()
            html_lines.append("<pre><code>")
            i += 1
            while i < len(lines) and not lines[i].startswith("```"):
                html_lines.append(html.escape(lines[i]))
                i += 1
            html_lines.append("</code></pre>")
            i += 1
            continue

        m = re.match(r"^(#{1,4})\s+(.*)", line)
        if m:
            flush_para(); close_list()
            level = len(m.group(1))
            html_lines.append(f"<h{level}>{inline(html.escape(m.group(2)))}</h{level}>")
            i += 1
            continue

        if re.match(r"^\s*\|.*\|\s*$", line):
            flush_para(); close_list()
            rows = []
            while i < len(lines) and re.match(r"^\s*\|.*\|\s*$", lines[i]):
                cells = [c.strip() for c in lines[i].strip().strip("|").split("|")]
                if not re.match(r"^\s*\|?[\s:|-]+\|?\s*$", lines[i]):
                    rows.append(cells)
                i += 1
            if rows:
                t = ["<table>", "<tr>" + "".join(f"<th>{inline(html.escape(c))}</th>" for c in rows[0]) + "</tr>"]
                for r in rows[1:]:
                    t.append("<tr>" + "".join(f"<td>{inline(html.escape(c))}</td>" for c in r) + "</tr>")
                t.append("</table>")
                html_lines.extend(t)
            continue

        if re.match(r"^\s*[-*]\s+", line):
            flush_para()
            want = "ul"
            if not list_stack or list_stack[-1] != want:
                close_list()
                list_stack.append(want)
                html_lines.append("<ul>")
            html_lines.append(f"<li>{inline(html.escape(re.sub(r'^\s*[-*]\\s+', '', line)))}</li>")
            i += 1
            continue

        if re.match(r"^\s*\d+[.)]\s+", line):
            flush_para()
            if not list_stack or list_stack[-1] != "ol":
                close_list()
                list_stack.append("ol")
                html_lines.append("<ol>")
            html_lines.append(f"<li>{inline(html.escape(re.sub(r'^\\s*\\d+[.)]\\s+', '', line)))}</li>")
            i += 1
            continue

        if line.strip() == "":
            flush_para(); close_list()
            i += 1
            continue

        if line.strip() == "---":
            flush_para(); close_list()
            html_lines.append("<hr>")
            i += 1
            continue

        para.append(line.strip())
        i += 1

    flush_para()
    close_list()
    return "\n".join(html_lines)


# --------------------------------------------------------------------------
# page assembly
# --------------------------------------------------------------------------
def slugify(text: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


def add_heading_ids(page_html: str) -> str:
    return re.sub(r"<h2>(.*?)</h2>",
                  lambda m: f'<h2 id="{slugify(re.sub("<[^>]+>", "", m.group(1)))}">{m.group(1)}</h2>',
                  page_html)


def toc_of(page_html: str) -> str:
    items = re.findall(r"<h2 id=\"([^\"]+)\">(.*?)</h2>", page_html)
    if not items:
        return ""
    links = " ".join(f'<a href="#{sid}">{re.sub("<[^>]+>", "", txt)}</a>' for sid, txt in items)
    return f'<div class="toc">On this page — {links}</div>'


def render_page(title: str, body_html: str) -> str:
    nav = "".join(f'<a href="{f}.html">{t}</a>' for f, t in [(p[0][:-3], p[1]) for p in PAGES])
    body_html = add_heading_ids(body_html)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} — aris-ui</title>
<meta name="description" content="aris-ui documentation — {title}">
<link rel="stylesheet" href="style.css">
</head>
<body>
{HEADER.format(nav=nav, repo=REPO)}
<main class="site">
{toc_of(body_html)}
{body_html}
</main>
{FOOTER.format(date=date.today().isoformat(), repo=REPO)}
</body>
</html>"""


def write_llms(pages_md: dict) -> None:
    lines = [
        "# aris-ui",
        "",
        "> A modern C++20 immediate-mode UI toolkit for OpenGL overlay injection:",
        "> SDF-rendered widgets, MSDF font atlases, Yoga flexbox layout, and a",
        "> headless preview/test toolchain designed for AI coding agents.",
        "",
    ]
    lines.append("## Docs")
    lines.append("")
    for fname, title in PAGES:
        lines.append(f"- [{title}](./{fname[:-3]}.html): {title} documentation for aris-ui")
    lines.append("")
    lines.append("## Machine-readable")
    lines.append("")
    lines.append("- [llms-full.txt](llms-full.txt): this entire documentation set in one file")
    lines.append("- Markdown sources live in `docs-src/` inside the repository")
    lines.append("")
    with open(os.path.join(OUT, "llms.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    with open(os.path.join(OUT, "llms-full.txt"), "w", encoding="utf-8", newline="\n") as f:
        for fname, title in PAGES:
            f.write(f"# {title}\n\n")
            f.write(pages_md[fname].strip())
            f.write("\n\n---\n\n")


def main() -> int:
    os.makedirs(OUT, exist_ok=True)
    pages_md = {}
    for fname, title in PAGES:
        path = os.path.join(SRC, fname)
        if not os.path.exists(path):
            print(f"missing docs-src/{fname}", file=sys.stderr)
            return 1
        md = open(path, encoding="utf-8").read()
        pages_md[fname] = md
        out_name = fname[:-3] + ".html"
        with open(os.path.join(OUT, out_name), "w", encoding="utf-8", newline="\n") as f:
            f.write(render_page(title, md_to_html(md)))
        print(f"wrote docs/{out_name}")

    with open(os.path.join(OUT, "style.css"), "w", encoding="utf-8", newline="\n") as f:
        f.write(CSS)
    print("wrote docs/style.css")

    write_llms(pages_md)
    print("wrote docs/llms.txt, docs/llms-full.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())