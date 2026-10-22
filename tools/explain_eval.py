"""Shows explanations of the sentences in tools/explain_fixtures.json side
by side, to compare models, prompts or grammar references.

  # Ask Claude, through the app's own request (build/*/src/claude/
  # explain_sentence). Paid: on Sonnet 5.5 about $0.30 for the first
  # request, which writes the cache, and $0.04 for each after it.
  ANTHROPIC_API_KEY=... python3 tools/explain_eval.py ask \\
      --grammar grammar/reference.md --model claude-haiku-5-5

  # One column per run (a saved run, or a database's stored explanations).
  python3 tools/explain_eval.py compare lexiCorp-pi.db \\
      explain-runs/claude-haiku-5-5.json

`ask` saves to explain-runs/<model>.json, `compare` writes
explain-runs/compare.html (both ignored by git). Standard library only.
"""

import argparse
import html
import json
import os
import sqlite3
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tools" / "explain_fixtures.json"
RUNS = ROOT / "explain-runs"


def sentences():
    return json.loads(FIXTURES.read_text())["sentences"]


def default_binary():
    for preset in ("release", "debug"):
        path = ROOT / "build" / preset / "src" / "claude" / "explain_sentence"
        if path.exists():
            return path
    return None


def ask(args):
    binary = args.binary or default_binary()
    if not binary:
        sys.exit("Build explain_sentence first (cmake --build --preset release)")
    env = dict(os.environ, LEXICORP_CLAUDE_EXPLAIN_MODEL=args.model)
    if args.grammar:
        env["LEXICORP_GRAMMAR_FILE"] = str(Path(args.grammar).resolve())

    process = subprocess.Popen([str(binary)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, text=True, env=env)
    answers = {}
    for s in sentences():
        query = {k: s[k] for k in ("sentence", "before", "after")}
        process.stdin.write(json.dumps(query, ensure_ascii=False) + "\n")
        process.stdin.flush()
        line = process.stdout.readline()
        if not line:
            sys.exit("explain_sentence stopped")
        answers[s["sentence"]] = json.loads(line)
        print(f"{len(answers)}/{len(sentences())}", file=sys.stderr)
    process.stdin.close()
    process.wait()

    label = args.label or args.model
    out = args.out or RUNS / f"{label}.json"
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps({"label": label, "answers": answers},
                              ensure_ascii=False, indent=2))
    cost = sum(a.get("cost", 0.0) for a in answers.values())
    print(f"Saved {out} (${cost:.2f})")


def load(path):
    """A run as {"label", "answers": {sentence: {answer, error, model, ...}}}."""
    path = Path(path)
    if path.suffix == ".db":
        db = sqlite3.connect(path)
        answers = {
            sentence: {"answer": answer, "error": "", "model": model}
            for sentence, answer, model in db.execute(
                "SELECT sentence, answer, model FROM explanation")
        }
        return {"label": f"{path.name} (stored)", "answers": answers}
    return json.loads(path.read_text())


def e(text):
    return html.escape(str(text))


def render(result):
    """One explanation as HTML, laid out like the app's."""
    if not result:
        return '<p class="muted">No explanation</p>'
    if result.get("error"):
        return f'<p class="error">{e(result["error"])}</p>'
    a = json.loads(result["answer"])
    parts = [f'<p class="translation">{e(a.get("translation", ""))}</p>']

    words = "".join(
        f'<div class="word"><div class="ku">{e(w["surface"])}</div>'
        f'<div class="leipzig">{e(w["leipzig"])}</div>'
        f'<div class="muted"><span class="ku">{e(w["base_form"])}</span>'
        f' ‘{e(w["base_gloss"])}’</div></div>'
        for w in a.get("words", []))
    parts.append(f'<div class="interlinear" dir="rtl">{words or "<i>no words</i>"}</div>')

    def section(title, lines):
        if lines:
            parts.append(f"<h4>{title}</h4><ul>{''.join(lines)}</ul>")

    def cited(ids):
        return f' <span class="muted">({e(", ".join(ids))})</span>' if ids else ""

    section("Parts", [
        f'<li><span class="ku">{e(w["surface"])}</span>: ' + " + ".join(
            f'<span class="ku">{e(m["form"])}</span> ({e(m["function"] or m["kind"])})'
            for m in w["morphemes"]) + "</li>"
        for w in a.get("words", []) if len(w["morphemes"]) > 1])
    section("Constructions", [
        f'<li><b>{e(c["name"])}</b> <span class="ku">{e(" … ".join(c["words"]))}</span>: '
        f'{e(c["explanation"])}{cited(c["sections"])}</li>'
        for c in a.get("constructions", [])])
    section("Notes", [f"<li>{e(n['text'])}{cited(n['sections'])}</li>"
                      for n in a.get("notes", [])])

    usage = [result.get("model", "")]
    if "cost" in result:
        usage.append(f"${result['cost']:.3f}")
    if "output_tokens" in result:
        usage.append(f"{result['output_tokens']} output tokens")
    parts.append(f'<p class="muted small">{e(" · ".join(u for u in usage if u))}</p>')
    return "".join(parts)


STYLE = """
body { font-family: system-ui, sans-serif; font-size: 14px; margin: 16px;
       background: #fff; color: #212529; }
table { border-collapse: collapse; width: 100%; table-layout: fixed; }
th, td { border: 1px solid #dee2e6; padding: 8px; vertical-align: top; }
th { background: #f8f9fa; position: sticky; top: 0; text-align: start; }
tr.sentence td { background: #e7f1ff; font-size: 18px; }
.ku { font-family: "Noto Naskh Arabic", serif; unicode-bidi: isolate; }
.translation { font-style: italic; }
.interlinear { display: flex; flex-wrap: wrap; gap: 6px 14px; }
.word { display: flex; flex-direction: column; align-items: center; }
.word > .ku { font-size: 16px; }
.leipzig { font-family: monospace; font-size: 12px; direction: ltr; }
.word .muted { font-size: 12px; direction: ltr; }
h4 { margin: 10px 0 2px; font-size: 11px; text-transform: uppercase;
     color: #6c757d; }
ul { margin: 0; padding-left: 18px; }
.muted { color: #6c757d; }
.small { font-size: 12px; }
.error { color: #dc3545; }
"""


def compare(args):
    runs = [load(p) for p in args.runs]
    rows = []
    for s in sentences():
        context = " ".join(x for x in (s["before"], "‹…›", s["after"]) if x)
        rows.append(
            f'<tr class="sentence"><td colspan="{len(runs)}" dir="rtl">'
            f'<span class="ku">{e(s["sentence"])}</span> '
            f'<span class="muted small ku">— {e(context)}</span></td></tr>')
        rows.append("<tr>" + "".join(
            f"<td>{render(run['answers'].get(s['sentence']))}</td>"
            for run in runs) + "</tr>")

    def total(run):
        cost = sum(a.get("cost", 0.0) for a in run["answers"].values())
        return f' <span class="muted small">${cost:.2f}</span>' if cost else ""

    heads = "".join(f"<th>{e(r['label'])}{total(r)}</th>" for r in runs)
    page = (f'<!doctype html><html><head><meta charset="utf-8">'
            f'<title>Explanations compared</title><style>{STYLE}</style></head>'
            f'<body><table><thead><tr>{heads}</tr></thead>'
            f'<tbody>{"".join(rows)}</tbody></table></body></html>')
    out = args.out or RUNS / "compare.html"
    out.parent.mkdir(exist_ok=True)
    out.write_text(page)
    print(f"Wrote {out}")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = parser.add_subparsers(dest="command", required=True)

    a = commands.add_parser("ask", help="explain every sentence with Claude")
    a.add_argument("--model", required=True)
    a.add_argument("--grammar", help="the reference (LEXICORP_GRAMMAR_FILE)")
    a.add_argument("--label", help="column title and file name (default: model)")
    a.add_argument("--out", type=Path)
    a.add_argument("--binary", type=Path)
    a.set_defaults(run=ask)

    c = commands.add_parser("compare", help="write the side-by-side page")
    c.add_argument("runs", nargs="+", help="run .json files or .db files")
    c.add_argument("--out", type=Path)
    c.set_defaults(run=compare)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
