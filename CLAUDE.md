# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

LexiCorp is a language-learning aid: paste in a text, and it extracts word
frequencies and builds up a per-language corpus of words together with the
source sentences they appeared in. It is a Wt (C++ web toolkit) server
application backed by SQLite.

## Build, run, test

Configure and build (creates `build/<preset>/`):

```
cmake --preset debug          # or: release
cmake --build --preset debug
```

Run the app (serves over HTTP; open the URL in a browser):

```
cmake --build build/debug --target run     # debug -> http://localhost:9090
cmake --build build/release --target run   # release -> http://localhost:8080
```

The `run` target passes `--docroot "<project root>;/resources"` (the
`;/resources` limits static serving to that folder, so all other URLs go to the
app: clean internal paths like `/list` instead of `/?_=/list`, and files such as
`lexiCorp.db` aren't downloadable) and
`--resources-dir <Wt's installed resources/>` (found at configure time as
`WT_RESOURCES_DIR`; without it the Bootstrap 5 theme CSS 404s), and copies
`resources/` next to the binary as a post-build step. At runtime the app opens/creates
`lexiCorp.db` in the working directory (project root when launched via `run`);
this file is gitignored.

Tests (Google Test, registered with `gtest_discover_tests`):

```
ctest --preset debug --output-on-failure          # all tests
ctest --preset debug -R WordRepositoryTests        # one suite / regex match
ctest --preset debug -N                            # list test names
```

Individual test executables are built next to their library, under
`build/debug/src/<lib>/` (e.g. `build/debug/src/db/word_repository_test`), and
accept the usual `--gtest_filter=...`.

`pages_test` drives whole sessions with `Wt::Test::WTestEnvironment`; each
test runs in its own temp directory because `AppContext` opens `lexiCorp.db`
in the working directory. `WTestEnvironment` can't see cookies the app sets
(only ones passed in with `setCookies`). The `addMetaHeader() with no effect`
warnings in its output come from Wt's Bootstrap 5 theme and are expected.

## Dependencies

External libs are expected to be installed on the system: **Wt 4.12.1**, Boost,
**ICU** (`libicu-dev`), SQLite3, OpenSSL, **libcurl**
(`libcurl4-openssl-dev`; the local Wt is built without SSL, so
`Wt::Http::Client` can't do HTTPS), plus CMake ≥ 3.21 and a C++23 compiler
(g++ 13.3). Two deps are fetched at configure time via `FetchContent`:
**GoogleTest** and **vsqlite++** (the C++ SQLite wrapper, imported as
`vsqlite::vsqlitepp`). See `README.md` for install commands. The Python
scripts in `tools/` take theirs from the `Pipfile` (`pipenv sync`, then
`pipenv run python tools/...`); `import_wiktionary.py` stays stdlib-only so
it runs on the Pi without them.

## Architecture

Five static libraries under `src/`, linked into the `LexiCorp` executable
(`src/main.cpp` is a thin `Wt::WRun` wrapper). Each library keeps public
headers in `src/<lib>/include/<lib>/`.

- **utils** — header-only (`INTERFACE`). `utils::Language` enum
  (English/French/Kurdish/Unknown) plus string conversions. Languages are
  persisted as their string name, never as the enum ordinal.

- **db** — persistence. `DatabaseHandler` owns one `sqlite::connection`.
  Ten repositories (`WordRepository`, `TextRepository`, `WordTextRepository`,
  `SentenceRepository`, `WordSentenceRepository`, `DictionaryRepository`,
  `WordFormRepository`, `ExpressionWordRepository`, `ClaudeUsageRepository`,
  `ExplanationRepository`, each behind an
  `I*Repository` interface) take that connection by reference and
  **create their own table in the constructor**. Model structs live in
  `db/*.h` (`models::Word`, `models::Text`, `models::WordText`,
  `models::Sentence`, `models::WordSentence`, `models::DictionaryEntry`,
  `models::WordForm`, `models::ExpressionWord`, `models::ClaudeUsage`,
  `models::Explanation`); each
  satisfies
  the `db::Utils::IsDbModel` concept by exposing `static tableName` and
  `static schema()`, and `Utils::createTableSql<T>()` turns that into a
  `CREATE TABLE IF NOT EXISTS`. For transactions use `db::Savepoint`
  (`db/Savepoint.h`), not `sqlite::transaction`: it nests (BEGIN can't, so a
  repository method using BEGIN fails inside a caller's transaction) and
  rolls back unless `commit()` is called. Batch writes into one: each
  separate commit waits for the disk, which takes seconds per few hundred rows
  on the Pi's SD card.

- **processor** — `TextProcessor`: `extractSentences` splits the raw text into
  sentences (ICU sentence iterator; every line break ends a sentence, and
  `@ss=standard` keeps "Mr." etc. inside one), keeping each sentence's
  position in code points, then runs the word pipeline on each sentence;
  `computeFrequencies` is the sum over sentences. The ICU locale comes from
  the language (`TextProcessor::locale`), never the server default. Word
  pipeline: NFKC normalization →
  per-language letter normalization (`normalizeLetters`; Kurdish only, via
  ICU transliterator rules: ك→ک, ي/ى→ی, ھ→ه, ه+ZWNJ/ة→ە, but not word-final
  ه, which can be a real /h/; `ViewSourcePage` applies it when highlighting) →
  strip punctuation/symbols/control chars (keeping apostrophes) → case-fold →
  word-boundary segmentation, producing a `word -> count` map. Behaviour pinned
  by tests: digit tokens count as words, internal apostrophes are kept, hyphens
  are stripped before segmentation (so `well-known` becomes one word). Also
  `hashWstring`, a SHA-256 (OpenSSL) digest truncated to `uint64_t`, used to
  identify a text; stored as `std::int64_t` (the same bits). `words()` runs
  the same pipeline but keeps the words in order (for multi-word expressions).

- **claude** — Claude API client (raw HTTPS over libcurl: there is no C++
  SDK). `ClaudeClient::defineWord` sends a word plus up to five of its
  sentences and returns glosses, idioms and token usage; it blocks, so pages
  call it through `runInBackground`. Request building / response parsing
  (`defineWordRequest`, `parseDefineWordResponse`) are free functions tested
  without a network. It uses structured outputs (`output_config.format`, a
  JSON schema), effort `low`, `fallbacks: "default"`, and caches the system
  prompt. The prompt is `prompts/define_word.md`, compiled in through a
  CMake-generated header (`claude/DefineWordPrompt.h`) so the binary stays
  self-contained; it is Sorani-only, so the UI only offers lookups for
  Kurdish (`kClaudeLanguage`). Configured by `ANTHROPIC_API_KEY` (no key: the
  feature is hidden) and `LEXICORP_CLAUDE_MODEL` (default
  `claude-sonnet-5-5`). `pricingFor` holds per-model prices for the Home
  page's cost estimate; add a model there when switching to it.
  `explainSentence` sends a sentence, with its neighbours
  as context, and returns a translation, per-word morphemes and Leipzig
  glosses, constructions and notes citing grammar reference sections
  (`explainSentenceRequest` / `parseExplainSentenceResponse`, effort
  `medium`). The system prompt is the grammar reference followed by
  `prompts/explain_sentence.md`, cached together. The reference
  (`GrammarReference`, `<section id="…">` blocks; ids in
  `prompts/grammar_outline.md`) is derived from copyrighted books, so it is
  read from `LEXICORP_GRAMMAR_FILE` at startup, not compiled in; without it
  `canExplain()` is false. The schema restricts cited sections to the
  reference's ids. `SentenceExplanation::answerJson` is what gets cached;
  `explanationFromJson` reads it back; an answer without words or a
  translation (which both models return now and then) is an error, so it
  is never stored, and a stored one counts as not explained. Model: `SentenceQuery::model`, chosen
  in the sidebar (`kExplainModels`: Sonnet or Haiku, an `explain_model`
  cookie, `AppContext::explainModel`), else `LEXICORP_CLAUDE_EXPLAIN_MODEL`
  (default: the lookup model). Haiku 5.5 costs 5x for prompts over 100k
  tokens, which the full grammar reference is, so `pricingFor` takes the
  request's prompt size and the Home page prices requests one by one
  (`ClaudeUsageRepository::requestsSince`).

- **pages** — one `Wt::WContainerWidget` subclass per screen.
  `LexiCorpMain` (`Wt::WApplication`) builds the shell: a sidebar with a
  language `WComboBox`, a `WMenu` bound to a `WStackedWidget` (using
  internal-path routing), and at the bottom the "Explain with" Sonnet /
  Haiku buttons (only while `canExplain()`) and the dark mode switch. The shell is plain flexbox, not a Wt layout
  manager, so `style.css` can turn the sidebar into a top bar on portrait
  screens (and landscape ones narrower than 576px).
  Routes: `""` → `HomePage` (stats overview),
  `/insert` → `InsertTextPage`, `/list` → `DisplayWordsPage`, `/source` →
  `ViewSourcePage` (all texts) and `/source/word` (the sentences containing
  `selectedWord`, grouped by text, with a per-text "Show full text" toggle
  that marks those sentences in the whole text, under the word's gloss and
  an "Edit gloss" dialog, plus "Ask Claude" for a Kurdish word without a
  gloss; idioms the word is part of are listed in the gloss card and under
  each sentence containing them; each Kurdish sentence has an "Explain"
  button while `canExplain()`; an explanation from another model than the
  sidebar's gets "Explain again with …", which replaces it).
  Explanations (`pages/Explanations.h`): the first click asks Claude in the
  background (with the sentences before and after it as context) and stores
  the answer in `explanation` under `sentenceHash()`, so the same sentence
  in any text reuses it; later clicks show / hide the stored one. The
  result is rendered interlinear (word, Leipzig gloss, base form), then the
  parts of each word (affixes with a tatweel on their joining side),
  constructions and notes with their cited section ids, then "Sources":
  each cited section (`IClaudeClient::grammar()`), opening its Markdown in a
  dialog (`pages/GrammarSections.h`: a small renderer for what the
  reference uses, with section ids in the text linking to those sections
  and a Back button). The answer's
  widgets are held through `Wt::Core::observing_ptr`, since the page can be
  redrawn before it arrives.
  Glosses (`pages/Glosses.h`): a word's entries from the preferred
  `DictionarySource` (manual > Wiktionary > Claude > Google), or, without
  entries of its own, those of a lemma it is a form of (`word_form`).
  `DisplayWordsPage` shows them under each word (AI-generated ones in
  italics); editing saves a `manual` entry, and saving it empty removes it.
  `saveLookup()` (`pages/ClaudeLookup.h`) stores a Claude answer: glosses as
  the word's `claude` entries, idioms (including compound verbs, often split
  by their object) as entries of their own under their dictionary form (part
  of speech `idiom`, e.g. `تاوتوێ کردن`), linked through `expression_word`
  to their words in both the sentence's and the dictionary form, and the
  request's usage. Idioms are dropped unless they have 2–5 words
  (`kMaxExpressionWords`), including the looked-up word, that occur in order
  (gaps allowed) in one of the sentences sent. A sentence on `/source/word`
  gets an idiom's note when it contains the word and another linked word.

### Cross-cutting conventions

- **AppContext** (`pages/include/pages/AppContext.h`) is a single
  `shared_ptr` constructed by `LexiCorpMain` and handed to every page. It holds
  the `DatabaseHandler` and all repositories, the current `selectedLanguage` /
  `selectedWord` / `explainModel`, and `Wt::Signal`s (`languageChanged`,
  `wordSelected`, `explainModelChanged`) that pages connect to in order to
  react to state changed elsewhere. Navigation
  between pages is done with `WApplication::instance()->setInternalPath(...)`.

- **Pages are built once, at startup.** Every page is constructed when the
  session starts, so a page must refresh its data itself: connect to
  `languageChanged`, and to `internalPathChanged` for its own route(s) (see
  `DisplayWordsPage`, `ViewSourcePage`, `HomePage`). Rendering differs by
  path where needed, e.g. `ViewSourcePage` shows all texts on `/source` and
  only the selected word's sentences on `/source/word`.

- **Per-browser preferences** live in one-year cookies read when the session
  starts: `language`, `explain_model` and `theme` (`dark` / `light`; without
  it the browser's `prefers-color-scheme` is used). Dark mode is Bootstrap's
  `data-bs-theme` on `<html>`, so CSS should use `--bs-*` variables, not
  fixed colours.

- **Slow actions** (anything that writes many rows or processes a long
  text): connect the button's `clicked()` to `&WWidget::disable` *before* the
  handler, give it the `busy-button` class, and call `enable()` at the start
  of the handler. `disable()` is a stateless slot, so it runs in the browser
  immediately: no double clicks, and CSS shows a spinner until the response
  (see `InsertTextPage`).

- **Background work** (a Claude request): `runInBackground(work, done)`
  (`pages/Background.h`) runs `work` on a thread and `done` back in the
  session via `WServer::post` + server push. `work` must not touch the
  session or `AppContext`'s connection; do DB writes in `done`. Wt's test
  environment has a server but never runs posted functions, so tests set
  `runBackgroundWorkInForeground` (`useFakeClaude()` in `PagesTest.cpp`
  does, along with `AppContext::sharedClaudeClient`).

- **Destructive actions** go through `confirmDestructive()`
  (`pages/ConfirmDialog.h`), a `WMessageBox` with Cancel and a red confirm
  button. In `pages_test` the open box is found under `app.domRoot()`, not
  `app.root()`. Deleting a word removes its `words`, `word_text` and
  `word_sentence` rows (sentences stay); deleting a text removes its rows in
  every table and subtracts its per-text counts from `words`, dropping
  unknown words that reach zero (known ones stay, with count 0).

- **Migrations**: `migrateDatabase()` (`pages/Migration.h`) brings the
  database to `kDatabaseVersion`, tracked in SQLite's `PRAGMA user_version`,
  in one transaction. `main()` runs it before `WRun`, since it isn't safe
  with concurrent sessions. A schema or data change that existing databases
  need gets a new `migrateToVersionN` step. Version 1 rehashed texts to 64
  bits and generated sentences for existing texts; version 2 letter-normalised
  Kurdish words stored before `normalizeLetters` existed (merging counts) and
  regenerated Kurdish texts' sentence links; version 3 removed Claude idioms
  longer than `kMaxExpressionWords` (whole clauses).

- **Text encoding**: text is held in memory as `std::wstring` of Unicode
  code points (UTF-32) and converted to/from UTF-8 only at the SQLite boundary
  via `db::Utils::toUtf8` / `fromUtf8` (ICU). New DB code and models should
  follow this — take/return `std::wstring`, convert inside the repository.

### Data model

- `words` — PRIMARY KEY `(word, language)`; `numOccurrences`, `known`.
  Frequency updates are upserts that add to the existing count.
- `text` — full source text (the source of truth: everything else can be
  regenerated from it), `hash` (64 bits of the SHA-256) as PRIMARY KEY,
  `language`, `source` label.
- `word_text` — join table, PRIMARY KEY `(word, text_hash)`, per-text
  occurrence count. Used for word counts when deleting a text.
- `sentence` — PRIMARY KEY `(text_hash, ordinal)`; the trimmed sentence and
  its `start` / `length` in the text, in code points.
- `word_sentence` — PRIMARY KEY `(word, language, text_hash, ordinal)`,
  per-sentence occurrence count; indexed on `text_hash`. Powers
  `/source/word`. Only words saved with the text are linked.
- `dictionary` — `id` PRIMARY KEY, indexed on `(headword, language)`; one
  gloss per row (a headword can have several: one per sense and source),
  `part_of_speech`, `source` (`manual` / `wiktionary` / `claude` / `google`),
  `ai_generated`, `updated_at` (Unix time). Headwords are normalised like
  `words`, and independent of it: deleting a word keeps its entries.
- `word_form` — PRIMARY KEY `(form, language, lemma)`, `tags`: inflected and
  alternative forms pointing at a headword.
- `expression_word` — PRIMARY KEY `(word, language, expression)`: links each
  word of an idiom to it, in every form seen. The idiom's gloss is a
  `dictionary` entry whose headword is its dictionary form's normalised words
  joined by single spaces.
- `explanation` — `sentence_hash` (`TextProcessor::hashWstring` of the
  sentence) PRIMARY KEY, the sentence, Claude's answer as JSON, the model,
  `created_at`. Saving again replaces it.
- `claude_usage` — one row per Claude API request: `created_at`, `model`,
  `purpose`, input / output / cache-read / cache-creation tokens. The Home
  page shows this month's total (UTC) and an estimated cost, priced per
  request.

`tools/import_wiktionary.py` fills `dictionary` (source `wiktionary`) and
`word_form` from the kaikki.org Central Kurdish extract, replacing its
earlier rows; the data file stays out of git (`*.jsonl` is ignored). It
re-implements the word normalisation in Python, so a change to
`kKurdishLetterRules` or the punctuation handling in `TextProcessor` must be
mirrored there.

The grammar reference for "Explain" is built from the PDFs in `pdfs/` in two
steps, both through the Claude API and both writing to the ignored
`grammar/` directory: `tools/grammar_notes.py` turns each chapter (a table
of PDF page ranges) into faithful notes, reading the Kurdish from page
images since the PDFs' text layers garble it; `tools/grammar_merge.py`
writes one section per entry of `prompts/grammar_outline.md` from all the
notes and assembles `grammar/reference.md`, the file `LEXICORP_GRAMMAR_FILE`
names. The outline's ids are what explanations cite, so keep them stable.
`grammar_merge.py --condense` rewrites each section shorter (asked for 40%,
it came out at about two thirds: 78k tokens against 117k) into
`grammar/condensed/` and `grammar/reference-condensed.md`: with
`LEXICORP_GRAMMAR_PROMPT_FILE` set, that is what explanations send, while
`grammar()` (the Sources dialog) stays the full reference. The client
ignores a condensed reference whose section ids differ from the full one's.
Both files are copied to the Pi by hand (`/var/lib/lexicorp/grammar.md`
and `grammar-condensed.md`, mode 640, `root:lexicorp`).

`tools/explain_eval.py` compares explanations of the sentences in
`tools/explain_fixtures.json`, to try other models, prompts or references:
`ask` explains them all through `explain_sentence`
(`src/claude/ExplainCli.cpp`, the app's own request, one JSON line per
sentence on stdin and stdout; paid) and saves a run in the ignored
`explain-runs/` (give each run its own `--label`, or it overwrites the
last run of that model); `compare` puts runs, or a database's stored
explanations, side by side in `explain-runs/compare.html`.

The typical flow: `InsertTextPage` runs `TextProcessor::extractSentences`,
lets the user pick which words to keep and mark as known, then writes rows to
the five corpus tables in one transaction (sentences via `saveSentences()`,
`pages/Sentences.h`, shared with the migration); `DisplayWordsPage`
lists/sorts/filters `words`; `ViewSourcePage` joins `word_sentence` →
`sentence` → `text`.

## Deployment (Raspberry Pi)

`deploy.sh` (run on the Pi as a normal user) does `git pull --ff-only`,
release build + tests, a pre-deploy DB backup, installs the binary and
`resources/` to `/opt/lexicorp`, and restarts the `lexicorp` systemd service.
The service runs as user `lexicorp` with `WorkingDirectory=/var/lib/lexicorp`,
so the live database is `/var/lib/lexicorp/lexiCorp.db`; the API key comes
from a root-owned, mode-600 `EnvironmentFile=` (see `README.md`); it listens on
`127.0.0.1:8080` and is exposed via `tailscale serve`. The binary must stay
self-contained (our libs and vsqlite++ linked statically — see
`BUILD_SHARED_LIBS` in the top-level `CMakeLists.txt`) since it is copied
out of the build tree.

## Wt quirks

Each of these caused a real bug; the workaround lives in `LexiCorpMain.cpp`
unless noted.

- `WMenu::setInternalPathEnabled()` without an argument uses the internal path
  the session *started* on as its base path, so reloading on `/list` broke
  routing. Always pass `"/"`.
- `WMenu` doesn't mark the item selected at first render as active;
  `markActive` syncs Bootstrap's `active` class on `itemSelected`.
- With the default JavaScript bootstrap, `addMetaHeader()` / `addMetaLink()`
  never reach the browser. `<head>` tags (viewport, manifest, icons) are added
  from JavaScript in `addHeadTags()`.
- `WStackedWidget` sets `overflow: hidden` inline, which breaks
  `position: sticky` in the pages (table headers, Submit button); the stack is
  set to `Overflow::Visible`. Don't wrap sticky content in an `overflow: auto`
  container either.
- Wt layout managers (`WHBoxLayout` etc.) size widgets with inline styles that
  CSS media queries can't override, hence the plain-flexbox shell.
- `--docroot` without a static-path list (`"<dir>;/resources"`) makes Wt fall
  back to `/?_=/path` URLs and serves every file in the docroot.
- `WText` with `TextFormat::XHTML` normalises entities (`&#39;` comes back as
  `'`). `highlightWithBreakIterator` in `ViewSourcePage.cpp` does the escaping
  itself, so pass it raw text; escaping first double-escapes. Sentence
  positions are code points but `icu::UnicodeString` indexes UTF-16, so
  convert with `moveIndex32` (see `highlightSentences`).

## Notes

- Do not run `git commit` yourself. Leave staging and committing to the
  repository owner.
- `DatabaseHandler` turns off vsqlite++'s statement cache. The cache keeps
  statements without resetting them, so a query left before its last row
  (e.g. `getByText` returning the first match) held a read lock for the
  connection's lifetime. Other sessions, and the import script, then got
  "database is locked" on every write.
- Table-level constraints are smuggled into `schema()` as fake columns, e.g.
  `{"PRIMARY", "KEY(word, language)"}`. `createTableSql<T>()` just joins
  `"<first> <second>"` pairs with commas, so this emits a valid
  `PRIMARY KEY(...)` clause. Brittle, but that's the current mechanism.
- `resources/fonts/` holds Noto Naskh Arabic as woff2 subsets of the Arabic
  ranges (OFL, see `OFL.txt`), used for Kurdish via `@font-face` +
  `unicode-range` in `resources/style.css`. To regenerate or widen the subset,
  run `python3 -m fontTools.subset` on the source TTF with
  `--layout-features='*' --flavor=woff2` and the same `--unicodes` ranges.
- `resources/manifest.json` + `resources/icons/` make the app installable
  ("Add to Home Screen"). The icons (an open book with "a" and "ک" on
  Bootstrap blue `#0d6efd`) are generated by `tools/make_icons.py` (Pillow);
  that colour is also the `theme-color` in `addHeadTags()`.
- Write Unicode bidi controls (isolates U+2066–2069, marks U+200E/200F) as
  `\u` escapes, as `isolate()` in `pages/Bidi.h` does: raw ones are
  invisible in an editor, and GCC's `-Wbidi-chars` warns about a literal
  that opens one without closing it.
- `Wt::Json::serialize` breaks lines even at indent 0; strip the newlines
  where one line per object is needed (`ExplainCli.cpp`).
- No committed `.clang-format`; existing code follows LLVM style (2-space
  indent, 80 columns).
