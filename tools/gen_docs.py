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

REPO = "https://github.com/OWNER/aris-ui"   # update when the repo is pushed

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

CSS = """/* aris-ui docs — night forest: dark blue-black, pine hints, no gradients */
:root {
  --bg: #0a0e14;          /* near-black blue */
  --bg2: #0d1219;         /* raised surface */
  --panel: #10161f;       /* card / panel */
  --line: #1d2836;        /* hairline borders */
  --text: #e6edf3;        /* primary */
  --dim: #8b949e;         /* secondary */
  --accent: #7c5cff;      /* violet, sparing */
  --accent-soft: #a394ff;
  --pine: #4e6e5e;        /* muted forest green for small details */
  --mono: ui-monospace, "JetBrains Mono", "SFMono-Regular", Menlo, Consolas, monospace;
  --sans: Inter, "Segoe UI", system-ui, -apple-system, sans-serif;
}
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
body {
  margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.65 var(--sans);
  -webkit-font-smoothing: antialiased;
}
a { color: var(--accent-soft); text-decoration: none; }
a:hover { text-decoration: underline; }
code {
  font-family: var(--mono); font-size: 0.86em;
  background: var(--panel); border: 1px solid var(--line);
  border-radius: 5px; padding: 0.08em 0.4em; color: #d2e3dc;
}
pre {
  background: var(--panel); border: 1px solid var(--line);
  border-radius: 10px; padding: 16px 18px; overflow-x: auto;
}
pre code { background: none; border: none; padding: 0; font-size: 0.84em; line-height: 1.55; }
h1, h2, h3 { font-weight: 650; letter-spacing: -0.01em; }
h1 { font-size: 1.9em; margin: 0.2em 0 0.5em; }
h2 { font-size: 1.3em; margin: 2.2em 0 0.6em; padding-top: 0.9em; border-top: 1px solid var(--line); }
h3 { font-size: 1.06em; margin: 1.6em 0 0.4em; }
p { margin: 0.7em 0; }
ul { padding-left: 1.3em; }
li { margin: 0.3em 0; }
table { border-collapse: collapse; margin: 1em 0; width: 100%; font-size: 0.92em; }
th, td { border: 1px solid var(--line); padding: 7px 11px; text-align: left; }
th { background: var(--bg2); color: var(--dim); font-weight: 600; }
hr { border: none; border-top: 1px solid var(--line); margin: 2em 0; }

/* layout */
.site { max-width: 980px; margin: 0 auto; padding: 0 24px; }
header.site {
  display: flex; align-items: center; gap: 26px;
  padding: 18px 24px; border-bottom: 1px solid var(--line);
  position: sticky; top: 0; background: var(--bg); z-index: 10;
}
.logo { font-weight: 700; font-size: 17px; letter-spacing: -0.02em; }
.logo .dot { color: var(--accent); }
header.site nav { display: flex; gap: 18px; flex-wrap: wrap; margin-left: auto; }
header.site nav a { color: var(--dim); font-size: 14.5px; }
header.site nav a:hover { color: var(--text); text-decoration: none; }
main.site { padding: 40px 24px 24px; }
footer.site {
  border-top: 1px solid var(--line); color: var(--dim);
  font-size: 13.5px; padding: 22px 24px 40px; margin-top: 60px;
  display: flex; justify-content: space-between; flex-wrap: wrap; gap: 10px;
}
.toc { color: var(--dim); font-size: 14px; margin-bottom: 2em; }
.toc a { margin-right: 14px; white-space: nowrap; }
"""

HEADER = """<header class="site">
  <a class="logo" href="index.html">aris-ui<span class="dot">.</span></a>
  <nav>
    {nav}
    <a href="{repo}">GitHub</a>
  </nav>
</header>"""

FOOTER = """<footer class="site">
  <span>aris-ui — MIT licensed. Dark pixels, calm forestry, zero gradients.</span>
  <span>Generated {date} · <a href="{repo}">source</a></span>
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
    with open(os.path.join(OUT, "llms.txt"), "w") as f:
        f.write("\n".join(lines))

    with open(os.path.join(OUT, "llms-full.txt"), "w") as f:
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
        md = open(path).read()
        pages_md[fname] = md
        out_name = fname[:-3] + ".html"
        with open(os.path.join(OUT, out_name), "w") as f:
            f.write(render_page(title, md_to_html(md)))
        print(f"wrote docs/{out_name}")

    with open(os.path.join(OUT, "style.css"), "w") as f:
        f.write(CSS)
    print("wrote docs/style.css")

    write_llms(pages_md)
    print("wrote docs/llms.txt, docs/llms-full.txt")
    return 0


if __name__ == "__main__":
    sys.exit(main())