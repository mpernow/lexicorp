#!/usr/bin/env python3
"""Imports Central Kurdish (Sorani) glosses from Wiktionary into LexiCorp.

Reads the kaikki.org extract of the English Wiktionary (one JSON entry per
line), e.g.

    curl -LO 'https://kaikki.org/dictionary/Central%20Kurdish/kaikki.org-dictionary-CentralKurdish.jsonl'

and writes, in one transaction:

- `dictionary`: one row per sense, with `source = 'wiktionary'`;
- `word_form`: inflected and alternative forms (conjugations, plurals,
  "form of" / "alternative form of" senses) pointing at their headword, so a
  word without an entry of its own can show its lemma's gloss.

Earlier Wiktionary rows and all word forms for the language are replaced, so
the import can be re-run with a newer extract. Manual entries are kept.

Headwords and forms are normalised as `TextProcessor` normalises stored words
(NFKC, Kurdish letter variants, punctuation, case folding); entries that
normalise to more than one word can never match a stored word and are skipped.

The tables are created by LexiCorp, so run the app once (or deploy) before
importing. On the Pi:

    sudo -u lexicorp python3 tools/import_wiktionary.py \\
        --db /var/lib/lexicorp/lexiCorp.db kaikki.org-dictionary-CentralKurdish.jsonl
"""

import argparse
import json
import re
import sqlite3
import sys
import time
import unicodedata

LANG_CODE = "ckb"
LANGUAGE = "Kurdish"  # as utils::to_string(Language::Kurdish)
SOURCE = "wiktionary"

# Keep in sync with kKurdishLetterRules in src/processor/TextProcessor.cpp.
_HEH_ZWNJ = re.compile("[هھ]‌")
_LETTERS = str.maketrans(
    {
        "ھ": "ه",  # ھ → ه
        "ة": "ە",  # ة → ە
        "ك": "ک",  # ك → ک
        "ي": "ی",  # ي → ی
        "ى": "ی",  # ى → ی
    }
)
# Hyphens join compounds; other punctuation, symbols and control characters
# separate words. Apostrophes are kept (see TextProcessor's constructor).
_JOINERS = {"-", "‐", "‑"}
_APOSTROPHES = {"'", "’"}

# Arabic short-vowel marks (fatha, kasra, sukun, ...). Conjugation tables
# write them (بِنێم) but running text normally doesn't (بنێم).
_VOWEL_MARKS = re.compile("[\u064b-\u0652]")

# Form tags that don't describe a form of the word.
_SKIPPED_FORM_TAGS = {"romanization", "table-tags", "inflection-template",
                      "canonical"}
# Tags that only describe kaikki's parsing.
_NOISE_TAGS = {"error-unrecognized-form", "form-of", "alt-of"}


def normalize(text):
    """The words `text` is stored as, normalised like TextProcessor does."""
    text = unicodedata.normalize("NFKC", text)
    text = _HEH_ZWNJ.sub("ە", text).translate(_LETTERS)
    cleaned = []
    for c in text:
        if c in _JOINERS:
            continue
        category = unicodedata.category(c)
        if (category[0] in "PS" or category == "Cc") and c not in _APOSTROPHES:
            cleaned.append(" ")
        else:
            cleaned.append(c)
    return "".join(cleaned).casefold().split()


def single_word(text):
    words = normalize(text)
    return words[0] if len(words) == 1 else None


def tags_of(item):
    return set(item.get("tags", [])) - _NOISE_TAGS


def read_entries(path):
    """Yields (headword, part of speech, glosses, forms) for each usable
    entry; forms are (form, tags) pairs."""
    with open(path, encoding="utf-8") as lines:
        for line_number, line in enumerate(lines, 1):
            try:
                entry = json.loads(line)
            except json.JSONDecodeError as e:
                sys.exit(f"{path}:{line_number}: {e}")
            if entry.get("lang_code") != LANG_CODE:
                continue
            headword = single_word(entry.get("word", ""))
            if not headword:
                continue

            glosses, forms = [], []
            for sense in entry.get("senses", []):
                if sense.get("glosses"):
                    # The last gloss is the most specific one of a sub-sense.
                    glosses.append(sense["glosses"][-1])
                # "form of X" / "alternative form of X": headword → X.
                for target in sense.get("form_of", []) + sense.get("alt_of", []):
                    lemma = single_word(target.get("word", ""))
                    if lemma and lemma != headword:
                        forms.append((headword, lemma, tags_of(sense)))

            for form in entry.get("forms", []):
                tags = tags_of(form)
                if tags & _SKIPPED_FORM_TAGS:
                    continue
                word = single_word(form.get("form", ""))
                if not word:
                    continue
                for spelling in {word, _VOWEL_MARKS.sub("", word)}:
                    if spelling != headword:
                        forms.append((spelling, headword, tags))

            yield headword, entry.get("pos", ""), glosses, forms


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("jsonl", help="kaikki.org JSONL extract")
    parser.add_argument("--db", default="lexiCorp.db",
                        help="LexiCorp database (default: %(default)s)")
    args = parser.parse_args()

    connection = sqlite3.connect(args.db, timeout=30)
    tables = {row[0] for row in connection.execute(
        "SELECT name FROM sqlite_master WHERE type = 'table'")}
    missing = {"dictionary", "word_form"} - tables
    if missing:
        sys.exit(f"{args.db} has no {', '.join(sorted(missing))} table; "
                 "start LexiCorp once to create them.")

    entries = set()  # (headword, gloss, part of speech), in order below
    ordered_entries = []
    form_tags = {}  # (form, lemma) → tags
    for headword, pos, glosses, forms in read_entries(args.jsonl):
        for gloss in glosses:
            key = (headword, gloss, pos)
            if key not in entries:
                entries.add(key)
                ordered_entries.append(key)
        for form, lemma, tags in forms:
            form_tags.setdefault((form, lemma), set()).update(tags)

    now = int(time.time())
    with connection:  # one transaction
        connection.execute(
            "DELETE FROM dictionary WHERE source = ? AND language = ?",
            (SOURCE, LANGUAGE))
        connection.execute("DELETE FROM word_form WHERE language = ?",
                           (LANGUAGE,))
        connection.executemany(
            "INSERT INTO dictionary(headword, language, gloss, "
            "part_of_speech, source, ai_generated, updated_at) "
            "VALUES(?, ?, ?, ?, ?, 0, ?)",
            [(headword, LANGUAGE, gloss, pos, SOURCE, now)
             for headword, gloss, pos in ordered_entries])
        connection.executemany(
            "INSERT INTO word_form(form, language, lemma, tags) "
            "VALUES(?, ?, ?, ?)",
            [(form, LANGUAGE, lemma, " ".join(sorted(tags)))
             for (form, lemma), tags in sorted(form_tags.items())])
    connection.close()

    headwords = len({headword for headword, _, _ in ordered_entries})
    print(f"Imported {len(ordered_entries)} glosses for {headwords} headwords "
          f"and {len(form_tags)} word forms.")


if __name__ == "__main__":
    main()
