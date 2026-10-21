#!/usr/bin/env python3
"""Turns chapters of the Sorani grammar books into faithful Markdown notes.

First pass of building the grammar reference for "Explain sentence" (see the
"Grammar reference" item in TODO.md). The books' text layers have clean
prose, but their Kurdish script is garbled (broken font mappings), so each
chapter goes to Claude both as page images (a PDF cut to the chapter's pages)
and as its pdftotext text: the explanations come from the text, the Kurdish
from the pages. The notes are per book and per chapter; merging them into
one reference with stable section ids is a later step.

The PDFs (in `pdfs/`) and the notes (in `grammar/notes/`) are derived from
copyrighted books and stay out of git.

Needs poppler-utils (pdftotext, pdfseparate, pdfunite), the Anthropic SDK
from the Pipfile (`pipenv sync`), and ANTHROPIC_API_KEY in the environment.

    pipenv run python tools/grammar_notes.py --list
    pipenv run python tools/grammar_notes.py thackston-27 --dry-run
    pipenv run python tools/grammar_notes.py thackston-27
"""

import argparse
import base64
import re
import subprocess
import sys
import tempfile
import unicodedata
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PDF_DIR = ROOT / "pdfs"
NOTES_DIR = ROOT / "grammar" / "notes"

DEFAULT_MODEL = "claude-opus-5-5"


@dataclass(frozen=True)
class Book:
    file: str
    citation: str  # how sources are tagged in the notes, e.g. "Thackston"
    # PDF page number minus printed page number, for citing printed pages;
    # None to cite PDF pages.
    page_offset: int | None
    description: str


BOOKS = {
    "thackston": Book(
        "sorani_complete.pdf",
        "Thackston",
        8,
        "W. M. Thackston, Sorani Kurdish: A Reference Grammar with Selected "
        "Readings. In English. Every Kurdish example is followed by "
        "Thackston's romanization, which survives cleanly in the text layer "
        "(â, î, ê, û, ô, ř, ł; í is the unwritten short vowel; acute accents "
        "on other vowels mark stress).",
    ),
    "kim": Book(
        "Kims_Kurdish_Grammar.pdf",
        "Kim",
        9,
        "Danielle H. Kim, A Basic Guide to Kurdish Grammar (a learner's "
        "self-study book). In English. Kurdish is written in an old font "
        "encoding with Arabic letters standing in for Kurdish ones, and has "
        "no romanization. Skip exercises and their answers.",
    ),
    "swedish": Book(
        "complete_kurd_gr.pdf",
        "Kurdisk grammatik",
        None,  # printed page numbers restart in each unit
        "Kurdisk grammatik (a Swedish course grammar of Sorani, in units "
        "whose section numbers restart from 1). In Swedish, with a later "
        "part in English; some examples have IPA in square brackets. Most "
        "Kurdish script is missing from the text layer. Skip exercises and "
        "their answers.",
    ),
}


@dataclass(frozen=True)
class Chapter:
    id: str
    book: str
    first_page: int  # PDF pages, inclusive
    last_page: int
    title: str
    # What to cover, when the page range also holds parts of neighbouring
    # chapters.
    scope: str = ""


# Step 1 of the plan: each book's chapters as PDF page ranges, checked with
# pdftotext (Thackston's table of contents is sometimes a page off). A page
# where one chapter ends and the next begins belongs to both, and `scope`
# says where to stop. Left out: Thackston's Sorani-Kurmanji conversion table
# and readings (from PDF p. 96), and the Swedish book's lesson texts,
# exercises and word lists (PDF pp. 57-114).
CHAPTERS = [
    # Thackston: the grammar is printed pp. 1-88.
    Chapter("thackston-phonology", "thackston", 9, 15,
            "The Phonology of Sorani Kurdish; Stress; The Writing System"),
    Chapter("thackston-01-09", "thackston", 16, 22,
            "§§ 1–9. Noun states, demonstratives, the izâfa",
            "§§ 1–9 only. The last page begins § 10: skip it."),
    Chapter("thackston-10-14", "thackston", 22, 32,
            "§§ 10–14.1. Pronouns, enclitic -îsh, numbers, comparison, "
            "prepositions",
            "§§ 10–14.1 only. The first page ends § 9: skip it."),
    Chapter("thackston-15-19", "thackston", 33, 42,
            "§§ 15–19. Present copulas, ‘to have’, the present "
            "habitual/progressive, verbs in -awa, the present subjunctive",
            "§§ 15–19 only. The last page begins § 20: skip it."),
    Chapter("thackston-20-26", "thackston", 42, 49,
            "§§ 20–26. ‘To want’, ‘to be able’, ‘to remember’, pronominal "
            "objects, the imperative, the intransitive simple and habitual "
            "past",
            "§§ 20–26 only. The first page ends § 19 and the last begins "
            "§ 27: skip those."),
    Chapter("thackston-27", "thackston", 49, 61,
            "§ 27. The Simple Past (Transitive): The Ergative",
            "§ 27 and its subsections § 27.1–27.4 only. The first page ends "
            "§ 26 and the last begins § 28: skip those."),
    Chapter("thackston-28-33", "thackston", 61, 71,
            "§§ 28–33. The perfect active participle, the present and past "
            "perfect, the past subjunctive, the irrealis, the past "
            "conditional",
            "§§ 28–33 only. The first page ends § 27.4 and the last begins "
            "§ 34: skip those."),
    Chapter("thackston-34-36", "thackston", 71, 77,
            "§§ 34–36. The passive voice, postposed verbal complements, "
            "factitive verbs",
            "§§ 34–36 only. The first page ends § 33: skip it."),
    Chapter("thackston-37-42", "thackston", 78, 85,
            "§§ 37–42. Temporal duration, subordinating conjunctions, "
            "relative clauses, directional nouns in -è, orthographic "
            "peculiarities, vowel contractions"),
    Chapter("thackston-tenses", "thackston", 86, 95,
            "Verb Tenses and Moods; Synopsis of Tenses and Moods; "
            "Conditional Sentence Types"),
    # Kim: chapters 1-14, printed pp. 1-89.
    Chapter("kim-01-02", "kim", 10, 20,
            "Chapters 1–2. Kurdish Alphabet; Grammatical Units"),
    Chapter("kim-03", "kim", 21, 31, "Chapter 3. The Noun Phrase"),
    Chapter("kim-04-07", "kim", 32, 42,
            "Chapters 4–7. Linking Verbs; Questions; Adjectives; "
            "Prepositions – Part I"),
    Chapter("kim-08-09-present", "kim", 43, 59,
            "Chapter 8. Verb Types; Chapter 9. Tenses: present and future, "
            "imperatives",
            "Chapter 8, and chapter 9 up to its \"Past Tense\" part, which "
            "starts on the next page."),
    Chapter("kim-09-past", "kim", 60, 69,
            "Chapter 9. Tenses: past tenses, present and past perfect",
            "The \"Past Tense\" part of chapter 9, to the end of the "
            "chapter."),
    Chapter("kim-10-11", "kim", 70, 77,
            "Chapters 10–11. Irregular Verbs; Passives"),
    Chapter("kim-12", "kim", 78, 84, "Chapter 12. Prepositions – Part II"),
    Chapter("kim-13", "kim", 85, 95, "Chapter 13. Subjunctives"),
    Chapter("kim-14", "kim", 96, 98, "Chapter 14. Conjunctions"),
    # Kurdisk grammatik: grammar units on PDF pp. 3-56, then (after the
    # lessons) more grammar in English on pp. 115-122.
    Chapter("swedish-01", "swedish", 3, 14,
            "Ortografi och uttal; Substantivböjning (orthography and "
            "pronunciation; noun inflection)"),
    Chapter("swedish-02", "swedish", 15, 20,
            "Adjektiv; Pronomen; Kopulaverb (adjectives, pronouns, the "
            "copula)"),
    Chapter("swedish-03", "swedish", 21, 27,
            "Räkneord; Enkla verb (numerals; verb stems, present indicative "
            "and subjunctive, imperative)"),
    Chapter("swedish-04", "swedish", 28, 38,
            "Preteritum indikativ; Imperfekt indikativ; Adverb (simple "
            "past, imperfect, adverbs)"),
    Chapter("swedish-05", "swedish", 39, 46,
            "Perfekt particip och perfekt; Pre- och postpositioner (perfect "
            "participle and perfect; prepositions and postpositions)"),
    Chapter("swedish-06", "swedish", 47, 51,
            "Pluskvamperfekt; Konjunktioner och subjunktioner; "
            "Interjektioner (past perfect, conjunctions, interjections)"),
    Chapter("swedish-07", "swedish", 52, 56, "Verb i passiv (the passive)"),
    Chapter("swedish-week10", "swedish", 115, 122,
            "Some more grammatical points (future, verbal affixes, "
            "subjunctive, imperative, conditionals, ability and wishes, "
            "obligation, ‘let’s’, plans)",
            "The grammar points only, not the exercises. This part is in "
            "English and writes Kurdish in the Latin (Hawar) alphabet: give "
            "each example in Sorani script, converted from the Latin, "
            "followed by the Latin as its romanization."),
]

INSTRUCTIONS = """\
You are turning one chapter of a Sorani Kurdish grammar into study notes. \
The notes will later be merged with notes from two other grammars into a \
compact reference that a model cites when explaining Kurdish sentences to an \
English-speaking learner, so they must be faithful to this book and complete \
for its chapter.

The book: {description}

The chapter: {title} ({pages}).{scope}

You have the chapter's pages as a PDF and, below, the text extracted from \
them with pdftotext. Use the extracted text for the prose, but never copy \
Kurdish script from it: its font mappings are broken, so Kurdish letters \
come out as wrong Arabic letters, presentation forms, Latin symbols or \
control characters. Read every Kurdish word from the page images instead.

Write the notes in English Markdown:
- Follow the book's own structure: a `##` heading per section or subsection, \
with its number.
- Keep every rule, paradigm and exception the book gives, stated as rules \
rather than retold as prose. Leave out page furniture (running heads, page \
numbers) and anything outside the chapter's scope.
- Paradigms as Markdown tables.
- Examples as list items: Kurdish in standard Unicode Sorani script, then \
the romanization if the book gives one, then the book's translation: \
`- کتێبەکەم خوێندەوە — kitêbakam xwêndawa — I read the book.` Keep all of \
the book's examples.
- Kurdish script in standard Central Kurdish Unicode: ک not ك, ی not ي or ى, \
ە not ة or ه+ZWNJ, ه not ھ, and ڕ ڵ ێ ۆ as single letters, no \
presentation forms. Don't add vowel marks the book doesn't have.
- Where the book gives a romanization, the script must agree with it; if \
the page image and the romanization disagree, follow the image and add \
`(romanization: …)`.
- Tag each section with its source: the book's section number (or chapter) \
and the {page_kind}, e.g. `[{citation} 2.3, p. 46]`.
- Translate non-English explanations into English; keep the book's \
grammatical terms, with the English term in parentheses where they are not \
English.
- Mark anything you cannot read with confidence as `[?]` rather than \
guessing, and add nothing the book doesn't say.

Reply with the notes only, starting with a `#` heading for the chapter.

<extracted_text>
{text}
</extracted_text>
"""


def pdf_path(chapter):
    return PDF_DIR / BOOKS[chapter.book].file


def extract_text(chapter):
    return subprocess.run(
        ["pdftotext", "-layout", "-f", str(chapter.first_page), "-l",
         str(chapter.last_page), str(pdf_path(chapter)), "-"],
        check=True, capture_output=True, text=True,
    ).stdout


def chapter_pdf(chapter):
    """The chapter's pages as a PDF of their own."""
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(
            ["pdfseparate", "-f", str(chapter.first_page), "-l",
             str(chapter.last_page), str(pdf_path(chapter)),
             f"{tmp}/page-%04d.pdf"],
            check=True, capture_output=True,
        )
        pages = sorted(Path(tmp).glob("page-*.pdf"))
        out = Path(tmp) / "chapter.pdf"
        subprocess.run(["pdfunite", *map(str, pages), str(out)], check=True,
                       capture_output=True)
        return out.read_bytes()


def prompt(chapter):
    book = BOOKS[chapter.book]
    pages = f"PDF pages {chapter.first_page}–{chapter.last_page}"
    if book.page_offset is None:
        page_kind = "PDF page (the book's printed page numbers restart)"
    else:
        pages += (f", printed pages {chapter.first_page - book.page_offset}–"
                  f"{chapter.last_page - book.page_offset}")
        page_kind = "printed page"
    return INSTRUCTIONS.format(
        description=book.description,
        title=chapter.title,
        pages=pages,
        scope=f" Cover: {chapter.scope}" if chapter.scope else "",
        citation=book.citation,
        page_kind=page_kind,
        text=extract_text(chapter),
    )


# Characters that shouldn't appear in clean Sorani: Arabic presentation
# forms, private-use and control characters, and the Arabic-only letters
# TextProcessor normalises (kKurdishLetterRules).
_SUSPICIOUS = re.compile(
    "[ﭐ-﷿ﹰ-﻿-\x00-\x08\x0b-\x1f"
    "كيىة]|[هھ]‌"
)


def check_kurdish(notes):
    """Lines of the notes with characters that suggest garbled Kurdish."""
    problems = []
    for number, line in enumerate(notes.splitlines(), 1):
        found = sorted(set(m.group() for m in _SUSPICIOUS.finditer(line)))
        if found:
            names = ", ".join(
                f"U+{ord(c[0]):04X} {unicodedata.name(c[0], '?')}"
                for c in found)
            problems.append(f"line {number}: {names}")
    return problems


def write_notes(chapter, model, effort):
    import anthropic

    client = anthropic.Anthropic()
    pdf = base64.standard_b64encode(chapter_pdf(chapter)).decode()
    with client.beta.messages.stream(
        model=model,
        max_tokens=64000,
        thinking={"type": "adaptive"},
        output_config={"effort": effort},
        betas=["server-side-fallback-2026-07-01"],
        fallbacks="default",
        messages=[{
            "role": "user",
            "content": [
                {"type": "document",
                 "source": {"type": "base64", "media_type": "application/pdf",
                            "data": pdf}},
                {"type": "text", "text": prompt(chapter)},
            ],
        }],
    ) as stream:
        response = stream.get_final_message()

    usage = response.usage
    print(f"{response.model}: {usage.input_tokens} input, "
          f"{usage.output_tokens} output tokens", file=sys.stderr)
    if response.stop_reason == "refusal":
        sys.exit(f"{chapter.id}: refused")
    if response.stop_reason == "max_tokens":
        sys.exit(f"{chapter.id}: cut off at max_tokens")
    return "".join(b.text for b in response.content if b.type == "text")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("chapters", nargs="*", help="chapter ids")
    parser.add_argument("--list", action="store_true", help="list chapters")
    parser.add_argument("--dry-run", action="store_true",
                        help="print the prompt instead of calling Claude")
    parser.add_argument("--force", action="store_true",
                        help="redo chapters that already have notes")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--effort", default="high",
                        choices=["low", "medium", "high", "xhigh", "max"])
    args = parser.parse_args()

    by_id = {c.id: c for c in CHAPTERS}
    if args.list or not args.chapters:
        for c in CHAPTERS:
            print(f"{c.id:16} PDF pp. {c.first_page}-{c.last_page}  {c.title}")
        return
    unknown = [i for i in args.chapters if i not in by_id]
    if unknown:
        sys.exit(f"unknown chapters: {', '.join(unknown)}")

    for chapter in (by_id[i] for i in args.chapters):
        if args.dry_run:
            pages = len(chapter_pdf(chapter))
            print(f"--- {chapter.id}: chapter PDF {pages} bytes")
            print(prompt(chapter))
            continue
        out = NOTES_DIR / chapter.book / f"{chapter.id}.md"
        if out.exists() and not args.force:
            print(f"{chapter.id}: {out} exists, skipping (--force to redo)")
            continue
        notes = write_notes(chapter, args.model, args.effort)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(notes)
        print(f"{chapter.id}: wrote {out.relative_to(ROOT)}")
        for problem in check_kurdish(notes):
            print(f"  suspicious Kurdish, {problem}")


if __name__ == "__main__":
    main()
