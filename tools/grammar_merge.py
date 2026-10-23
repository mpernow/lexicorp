#!/usr/bin/env python3
"""Merges the grammar notes into the grammar reference, one section at a time.

Step 5 of the "Grammar reference" item in TODO.md. The sections, their ids
and their scopes come from prompts/grammar_outline.md; the material comes
from the chapter notes in grammar/notes/ (written by grammar_notes.py). Every
request carries all the notes, cached, so a section can use what any book
says about its topic; the outline's sources only say where to look first.

Each section is written to grammar/sections/<id>.md, so one can be redone
on its own, and grammar/reference.md is assembled from them in outline
order as `<section id="…" title="…">` blocks: the file LEXICORP_GRAMMAR_FILE
points to. Like the notes, both stay out of git.

Needs the Anthropic SDK from the Pipfile (`pipenv sync`) and
ANTHROPIC_API_KEY.

    pipenv run python tools/grammar_merge.py --list
    pipenv run python tools/grammar_merge.py --count     # tokens, free
    pipenv run python tools/grammar_merge.py clitics.agent
    pipenv run python tools/grammar_merge.py             # every missing one
    pipenv run python tools/grammar_merge.py --assemble  # reference only

--condense rewrites each finished section shorter, into grammar/condensed/,
and assembles grammar/reference-condensed.md: the version sent to Claude
with every explanation (LEXICORP_GRAMMAR_PROMPT_FILE), while the app shows
the full one. Only the sections are sent, not the notes.

    pipenv run python tools/grammar_merge.py --condense            # all
    pipenv run python tools/grammar_merge.py --condense clitics.agent
"""

import argparse
import re
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

from grammar_notes import BOOKS, CHAPTERS, check_kurdish

ROOT = Path(__file__).resolve().parent.parent
OUTLINE = ROOT / "prompts" / "grammar_outline.md"
EXPLAIN_PROMPT = ROOT / "prompts" / "explain_sentence.md"
NOTES_DIR = ROOT / "grammar" / "notes"
SECTIONS_DIR = ROOT / "grammar" / "sections"
REFERENCE = ROOT / "grammar" / "reference.md"
CONDENSED_DIR = ROOT / "grammar" / "condensed"
CONDENSED_REFERENCE = ROOT / "grammar" / "reference-condensed.md"

DEFAULT_MODEL = "claude-opus-5-5"

# US dollars per million tokens: input, output, cache read, cache write.
PRICES = {
    "claude-opus-5-5": (4.0, 20.0, 0.20, 5.0),
    "claude-sonnet-5-5": (2.0, 10.0, 0.20, 2.50),
}


@dataclass(frozen=True)
class Section:
    id: str
    title: str
    scope: str
    sources: str
    group: str


_ENTRY = re.compile(r"^- `([a-z0-9.-]+)` — (.+)$")
_SOURCES = re.compile(r"^  Sources: (.+)$")


def read_outline():
    """The outline's sections, in order."""
    sections = []
    group = ""
    lines = OUTLINE.read_text().splitlines()
    for i, line in enumerate(lines):
        if line.startswith("## "):
            group = line[3:].strip()
        elif match := _ENTRY.match(line):
            next_line = lines[i + 1] if i + 1 < len(lines) else ""
            sources = _SOURCES.match(next_line)
            if not sources:
                sys.exit(f"{OUTLINE.name}: no Sources line after {match[1]}")
            # "Title. What it covers."
            title, _, scope = match[2].partition(". ")
            sections.append(
                Section(match[1], title.rstrip("."), scope, sources[1], group))
    ids = [s.id for s in sections]
    if len(set(ids)) != len(ids):
        sys.exit(f"{OUTLINE.name}: duplicate ids")
    return sections


def notes_block():
    """Every chapter's notes, in chapter-table order, each tagged."""
    parts = []
    for chapter in CHAPTERS:
        path = NOTES_DIR / chapter.book / f"{chapter.id}.md"
        if not path.exists():
            sys.exit(f"missing notes: {path.relative_to(ROOT)} "
                     "(run grammar_notes.py first)")
        book = BOOKS[chapter.book].citation
        parts.append(f'<notes chapter="{chapter.id}" book="{book}">\n'
                     f"{path.read_text().strip()}\n</notes>")
    return "<grammar_notes>\n" + "\n\n".join(parts) + "\n</grammar_notes>"


INSTRUCTIONS = """\
You are writing one section of a compact reference grammar of Sorani \
(Central) Kurdish.

How it will be used: the finished reference is given, as context, to a \
model that explains authentic Sorani sentences (news, prose) to an \
English-speaking learner. For every word it gives the base form, splits it \
into stem, affixes and clitics with their functions, glosses it in Leipzig \
style, explains constructions spanning several words, and cites the \
reference's section ids. So the reference is for reading, not writing: what \
matters is how to recognise a form in running text, what it can mean, and \
how to tell it apart from forms that look the same (-ی as possessor, izâfa \
or 3sg agent; -ە as copula, close izâfa or the end of a noun; -ەوە as a \
verb suffix or a circumposition, ...).

Above are notes on three grammars, one `<notes>` block per chapter:
- Thackston, Sorani Kurdish: A Reference Grammar (detailed and \
authoritative; numbered §§, romanized examples).
- Kim, A Basic Guide to Kurdish Grammar (a learner's book; simpler \
explanations, many everyday examples).
- Kurdisk grammatik (a Swedish course grammar; the notes are translated).
The notes are faithful to their books, with source tags, and keep the \
books' typos marked `(sic)` or with the romanization.

The outline of the whole reference, one section per line (id — title. \
scope):
<outline>
{outline}
</outline>

Write the section named in the user message:
- Cover what its scope says. Leave what belongs to another section to that \
section, and point there with its id in parentheses, e.g. "(see \
clitics.agent)". Only use ids from the outline.
- Merge the books into one account. Use Thackston's terms (absolute / \
indefinite / definite state, izâfa, agent affixes, ...), with a common \
alternative in parentheses the first time (e.g. izâfa (ezafe)). Use Kim's \
or the Swedish book's explanation where it is clearer, and their extra \
points where they add something. Where the books disagree, say so in one \
line and name them.
- Use only what the notes say. If the notes don't cover part of the scope, \
leave it out; don't fill it in from your own knowledge.
- Rules as bullet points; paradigms as compact Markdown tables; then 2 to 5 \
short examples from the notes that show the rules at work, each as \
`- Kurdish — translation`, with the relevant parts glossed where that \
helps. Don't invent examples.
- Kurdish in standard Central Kurdish Unicode spelling (ک, ی, ە, ڕ, ڵ, ێ, \
ۆ; no presentation forms), today's standard spelling of each word; mention a \
book's older or dialect spelling only where a reader may meet it. \
Romanization only where pronunciation matters (ř, ł, stress).
- Leave out the books' typos, exercises, page references, teaching remarks \
("we will see later") and cross-references to their own chapters.
- As short as completeness allows: usually 150 to 400 words plus tables and \
examples; the central sections (clitics, the ergative, the izâfa, verb \
forms) may be longer.
- No heading for the section itself (its title is added); bold labels or \
`####` subheadings inside it if needed.
- End with one line listing the books' sections used, e.g. \
`Sources: Thackston §§ 27–27.2; Kim ch. 9; Kurdisk grammatik 1.1.2.`

Reply with the section's Markdown only."""


def system_prompt(sections):
    outline = "\n".join(f"{s.id} — {s.title}. {s.scope}" for s in sections)
    return [
        {"type": "text", "text": notes_block()},
        {"type": "text", "text": INSTRUCTIONS.format(outline=outline),
         # Caches the notes and the instructions together.
         "cache_control": {"type": "ephemeral"}},
    ]


def user_message(section):
    return (f"Section: {section.id} — {section.title}\n"
            f"Scope: {section.scope}\n"
            f"Main sources: {section.sources}")


CONDENSE_INSTRUCTIONS = """\
You are condensing one section of a reference grammar of Sorani (Central) \
Kurdish.

How it will be used: the reference is given, as context, to a model that \
explains authentic Sorani sentences to an English-speaking learner: base \
forms, stems, affixes and clitics with their functions, Leipzig glosses, \
constructions, citing the reference's section ids. It is sent with every \
request, so every token counts. What matters is how to recognise a form in \
running text, what it can mean, and how to tell it apart from forms that \
look the same.

The outline of the whole reference, one section per line (id — title. \
scope):
<outline>
{outline}
</outline>

Rewrite the section in the user message to about 40% of its length:
- Keep every rule and every distinction, above all the recognition points \
(which -ی, which -ە, ...). Keep the paradigm tables, made compact: merge \
columns, drop rows that only repeat a pattern already shown.
- Keep the cross-references "(see id)". Only use ids from the outline.
- Keep one short example per rule where the rule is unclear without it, \
as `- Kurdish — translation`. Copy the Kurdish exactly as written.
- Drop the Sources line, which book says what, repeated explanations, \
further examples, romanization except where pronunciation matters, and \
dialect notes a reader of standard written Sorani won't need. Keep a \
disagreement between the books only where it changes how a form is read.
- Add nothing that isn't in the section.
- No heading for the section itself; bold labels or `####` subheadings \
inside it if needed.

Reply with the condensed Markdown only."""


def condense_system_prompt(sections):
    outline = "\n".join(f"{s.id} — {s.title}. {s.scope}" for s in sections)
    return [{"type": "text",
             "text": CONDENSE_INSTRUCTIONS.format(outline=outline),
             "cache_control": {"type": "ephemeral"}}]


def condense_message(section):
    return (f"Section: {section.id} — {section.title}\n\n"
            f"{section_path(section).read_text().strip()}")


def clean(text, section):
    """The section body as the model wrote it, minus a repeated title."""
    text = text.strip()
    first, _, rest = text.partition("\n")
    if first.startswith("#") and section.title.lower() in first.lower():
        text = rest.strip()
    if "<section" in text or "</section" in text:
        raise ValueError("the answer contains section tags")
    return text


def write_section(client, model, effort, system, section, message):
    with client.beta.messages.stream(
        model=model,
        max_tokens=32000,
        thinking={"type": "adaptive"},
        output_config={"effort": effort},
        betas=["server-side-fallback-2026-07-01"],
        fallbacks="default",
        system=system,
        messages=[{"role": "user", "content": message}],
    ) as stream:
        response = stream.get_final_message()
    if response.stop_reason == "refusal":
        raise RuntimeError("refused")
    if response.stop_reason == "max_tokens":
        raise RuntimeError("cut off at max_tokens")
    text = "".join(b.text for b in response.content if b.type == "text")
    return clean(text, section), response.model, response.usage


def cost(model, usage):
    prices = PRICES.get(model)
    if not prices:
        return None
    return (usage.input_tokens * prices[0] + usage.output_tokens * prices[1]
            + (usage.cache_read_input_tokens or 0) * prices[2]
            + (usage.cache_creation_input_tokens or 0) * prices[3]) / 1e6


def section_path(section):
    return SECTIONS_DIR / f"{section.id}.md"


def condensed_path(section):
    return CONDENSED_DIR / f"{section.id}.md"


def unknown_ids(text, sections):
    """The ids `text` cites as "(see …)" that the outline doesn't have."""
    cited = set(re.findall(r"\bsee ([a-z]+\.[a-z0-9.-]*[a-z0-9])", text))
    return cited - {s.id for s in sections}


def assemble(sections, path_of=section_path, reference=REFERENCE):
    """Writes the reference from the sections written so far."""
    parts = ["# Sorani Kurdish grammar reference\n"]
    group = None
    missing = []
    for section in sections:
        path = path_of(section)
        if not path.exists():
            missing.append(section.id)
            continue
        if section.group != group:
            group = section.group
            parts.append(f"## {group}\n")
        title = section.title.replace('"', "'")
        parts.append(f'<section id="{section.id}" title="{title}">\n'
                     f"{path.read_text().strip()}\n</section>\n")
    reference.write_text("\n".join(parts))
    print(f"wrote {reference.relative_to(ROOT)}: {len(sections) - len(missing)}"
          f" of {len(sections)} sections, {len(reference.read_text())} chars")
    if missing:
        print(f"  missing: {', '.join(missing)}")
    # The schema only lets answers cite the reference's ids, so the
    # prompt's examples must use real ones.
    cited = set(re.findall(r"\(([a-z]+\.[a-z0-9.-]+)\)",
                           EXPLAIN_PROMPT.read_text()))
    unknown = cited - {s.id for s in sections}
    if unknown:
        print(f"  {EXPLAIN_PROMPT.name} cites ids not in the outline: "
              f"{', '.join(sorted(unknown))}")


def count(client, model, sections):
    system = system_prompt(sections)
    merge_input = client.messages.count_tokens(
        model=model, system=system,
        messages=[{"role": "user", "content": user_message(sections[0])}],
    ).input_tokens
    print(f"merge request input: {merge_input} tokens (notes, outline, "
          f"instructions), cached after the first request")
    prices = PRICES.get(model)
    if prices:
        print(f"  cache write ${merge_input * prices[3] / 1e6:.2f}, then "
              f"${merge_input * prices[2] / 1e6:.3f} per section to read it")
    if REFERENCE.exists():
        reference = client.messages.count_tokens(
            model=model,
            messages=[{"role": "user", "content": REFERENCE.read_text()}],
        ).input_tokens
        print(f"reference: {reference} tokens")
    if CONDENSED_REFERENCE.exists():
        condensed = client.messages.count_tokens(
            model=model,
            messages=[{"role": "user",
                       "content": CONDENSED_REFERENCE.read_text()}],
        ).input_tokens
        print(f"condensed reference: {condensed} tokens")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("sections", nargs="*",
                        help="section ids (default: all without a file)")
    parser.add_argument("--list", action="store_true", help="list sections")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the request instead of sending it")
    parser.add_argument("--count", action="store_true",
                        help="count tokens of the merge input and reference")
    parser.add_argument("--assemble", action="store_true",
                        help="only assemble the reference")
    parser.add_argument("--force", action="store_true",
                        help="redo sections that already have a file")
    parser.add_argument("--condense", action="store_true",
                        help="write condensed sections and their reference")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--effort",
                        choices=["low", "medium", "high", "xhigh", "max"],
                        help="default: high, medium with --condense")
    parser.add_argument("--workers", type=int, default=4)
    args = parser.parse_args()

    sections = read_outline()
    by_id = {s.id: s for s in sections}
    effort = args.effort or ("medium" if args.condense else "high")
    # What this run writes: the sections, or their condensed versions.
    if args.condense:
        path_of, reference = condensed_path, CONDENSED_REFERENCE
        out_dir = CONDENSED_DIR
    else:
        path_of, reference = section_path, REFERENCE
        out_dir = SECTIONS_DIR

    if args.list:
        for s in sections:
            status = "done" if path_of(s).exists() else "-"
            print(f"{status:4} {s.id:24} {s.title}")
        return
    if args.assemble:
        assemble(sections, path_of, reference)
        return

    unknown = [i for i in args.sections if i not in by_id]
    if unknown:
        sys.exit(f"unknown sections: {', '.join(unknown)}")
    todo = [by_id[i] for i in args.sections] or sections
    if args.condense:
        unwritten = [s.id for s in todo if not section_path(s).exists()]
        if unwritten:
            sys.exit(f"write these sections first: {', '.join(unwritten)}")
    if not args.force:
        skipped = [s for s in todo if path_of(s).exists()]
        if args.sections:
            for s in skipped:
                print(f"{s.id}: exists, skipping (--force to redo)")
        todo = [s for s in todo if s not in skipped]

    if args.dry_run and args.condense:
        system = condense_system_prompt(sections)
        print(system[0]["text"])
        for s in todo[:1]:
            print(f"--- user\n{condense_message(s)}")
        print(f"--- {len(todo)} sections to condense")
        return
    if args.dry_run:
        system = system_prompt(sections)
        print(f"--- system: notes {len(system[0]['text'])} chars, "
              f"instructions {len(system[1]['text'])} chars")
        print(system[1]["text"])
        for s in todo[:3]:
            print(f"--- user\n{user_message(s)}")
        print(f"--- {len(todo)} sections to write")
        return

    import anthropic

    client = anthropic.Anthropic()
    if args.count:
        count(client, args.model, sections)
        return
    if not todo:
        print("nothing to do")
        assemble(sections, path_of, reference)
        return

    if args.condense:
        system = condense_system_prompt(sections)
        message = condense_message
    else:
        system = system_prompt(sections)
        message = user_message
    out_dir.mkdir(parents=True, exist_ok=True)
    total = 0.0
    failed = []

    def run(section):
        return section, write_section(client, args.model, effort, system,
                                      section, message(section))

    def done(section, result):
        nonlocal total
        text, model, usage = result
        path_of(section).write_text(text + "\n")
        dollars = cost(model, usage)
        total += dollars or 0
        print(f"{section.id}: {len(text)} chars, {model}, "
              f"{usage.input_tokens} in + "
              f"{usage.cache_read_input_tokens or 0} cached + "
              f"{usage.cache_creation_input_tokens or 0} cache write, "
              f"{usage.output_tokens} out"
              + (f", ${dollars:.2f}" if dollars is not None else ""))
        for problem in check_kurdish(text):
            print(f"  suspicious Kurdish, {problem}")
        if args.condense:
            full = len(section_path(section).read_text().strip())
            print(f"  {100 * len(text) // full}% of the section")
        if unknown := unknown_ids(text, sections):
            print(f"  cites ids not in the outline: {', '.join(sorted(unknown))}")

    # The first request writes the cache; the others then read it.
    first, rest = todo[0], todo[1:]
    for batch in ([first], rest):
        with ThreadPoolExecutor(max_workers=args.workers) as pool:
            futures = [pool.submit(run, s) for s in batch]
            for future in as_completed(futures):
                try:
                    done(*future.result())
                except Exception as error:  # noqa: BLE001 - report, go on
                    section = batch[futures.index(future)]
                    failed.append(section.id)
                    print(f"{section.id}: FAILED: {error}")

    print(f"total ${total:.2f}")
    assemble(sections, path_of, reference)
    if failed:
        sys.exit(f"failed: {' '.join(failed)} (run again to retry them)")


if __name__ == "__main__":
    main()
