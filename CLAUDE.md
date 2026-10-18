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
**ICU** (`libicu-dev`), SQLite3, OpenSSL, plus CMake ≥ 3.21 and a C++23
compiler (g++ 13.3). Two deps are fetched at configure time via `FetchContent`:
**GoogleTest** and **vsqlite++** (the C++ SQLite wrapper, imported as
`vsqlite::vsqlitepp`). See `README.md` for install commands.

## Architecture

Four static libraries under `src/`, linked into the `LexiCorp` executable
(`src/main.cpp` is a thin `Wt::WRun` wrapper). Each library keeps public
headers in `src/<lib>/include/<lib>/`.

- **utils** — header-only (`INTERFACE`). `utils::Language` enum
  (English/French/Kurdish/Unknown) plus string conversions. Languages are
  persisted as their string name, never as the enum ordinal.

- **db** — persistence. `DatabaseHandler` owns one `sqlite::connection`.
  Three repositories (`WordRepository`, `TextRepository`, `WordTextRepository`,
  each behind an `I*Repository` interface) take that connection by reference and
  **create their own table in the constructor**. Model structs live in
  `db/*.h` (`models::Word`, `models::Text`, `models::WordText`); each satisfies
  the `db::Utils::IsDbModel` concept by exposing `static tableName` and
  `static schema()`, and `Utils::createTableSql<T>()` turns that into a
  `CREATE TABLE IF NOT EXISTS`. For transactions use `db::Savepoint`
  (`db/Savepoint.h`), not `sqlite::transaction`: it nests (BEGIN can't, so a
  repository method using BEGIN fails inside a caller's transaction) and
  rolls back unless `commit()` is called. Batch writes into one: each
  separate commit waits for the disk, which takes seconds per few hundred rows
  on the Pi's SD card.

- **processor** — `TextProcessor`: ICU-based pipeline of NFKC normalization →
  strip punctuation/symbols/control chars (keeping apostrophes) → case-fold →
  word-boundary segmentation, producing a `word -> count` map. Behaviour pinned
  by tests: digit tokens count as words, internal apostrophes are kept, hyphens
  are stripped before segmentation (so `well-known` becomes one word). Also
  `hashWstring`, a SHA-256 (OpenSSL) digest truncated to `uint64_t`, used to
  identify a text.

- **pages** — one `Wt::WContainerWidget` subclass per screen.
  `LexiCorpMain` (`Wt::WApplication`) builds the shell: a sidebar with a
  language `WComboBox` and a `WMenu` bound to a `WStackedWidget`, using
  internal-path routing. The shell is plain flexbox, not a Wt layout
  manager, so `style.css` can turn the sidebar into a top bar on portrait
  screens (and landscape ones narrower than 576px).
  Routes: `""` → `HomePage` (stats overview),
  `/insert` → `InsertTextPage`, `/list` → `DisplayWordsPage`, `/source` →
  `ViewSourcePage` (all texts) and `/source/word` (texts containing
  `selectedWord`, highlighted).

### Cross-cutting conventions

- **AppContext** (`pages/include/pages/AppContext.h`) is a single
  `shared_ptr` constructed by `LexiCorpMain` and handed to every page. It holds
  the `DatabaseHandler` and all repositories, the current `selectedLanguage` /
  `selectedWord`, and `Wt::Signal`s (`languageChanged`, `wordSelected`) that
  pages connect to in order to react to state changed elsewhere. Navigation
  between pages is done with `WApplication::instance()->setInternalPath(...)`.

- **Pages are built once, at startup.** Every page is constructed when the
  session starts, so a page must refresh its data itself: connect to
  `languageChanged`, and to `internalPathChanged` for its own route(s) (see
  `DisplayWordsPage`, `ViewSourcePage`, `HomePage`). Rendering differs by
  path where needed, e.g. `ViewSourcePage` shows all texts on `/source` and
  only the selected word's texts on `/source/word`.

- **Per-browser preferences** live in one-year cookies read when the session
  starts: `language` and `theme` (`dark` / `light`; without it the browser's
  `prefers-color-scheme` is used). Dark mode is Bootstrap's `data-bs-theme`
  on `<html>`, so CSS should use `--bs-*` variables, not fixed colours.

- **Slow actions** (anything that writes many rows or processes a long
  text): connect the button's `clicked()` to `&WWidget::disable` *before* the
  handler, give it the `busy-button` class, and call `enable()` at the start
  of the handler. `disable()` is a stateless slot, so it runs in the browser
  immediately: no double clicks, and CSS shows a spinner until the response
  (see `InsertTextPage`).

- **Text encoding**: text is held in memory as `std::wstring` of Unicode
  code points (UTF-32) and converted to/from UTF-8 only at the SQLite boundary
  via `db::Utils::toUtf8` / `fromUtf8` (ICU). New DB code and models should
  follow this — take/return `std::wstring`, convert inside the repository.

### Data model

- `words` — PRIMARY KEY `(word, language)`; `numOccurrences`, `known`.
  Frequency updates are upserts that add to the existing count.
- `text` — full source text, `hash` (the truncated SHA-256) as PRIMARY KEY,
  `language`, `source` label.
- `word_text` — join table, PRIMARY KEY `(word, text_hash)`, per-text
  occurrence count. Powers `ViewSourcePage`, which shows every stored text
  containing a word with that word highlighted.

The typical flow: `InsertTextPage` runs `TextProcessor`, lets the user pick
which words to keep and mark as known, then writes rows to all three tables;
`DisplayWordsPage` lists/sorts/filters `words`; `ViewSourcePage` joins back
through `word_text` to `text`.

## Deployment (Raspberry Pi)

`deploy.sh` (run on the Pi as a normal user) does `git pull --ff-only`,
release build + tests, a pre-deploy DB backup, installs the binary and
`resources/` to `/opt/lexicorp`, and restarts the `lexicorp` systemd service.
The service runs as user `lexicorp` with `WorkingDirectory=/var/lib/lexicorp`,
so the live database is `/var/lib/lexicorp/lexiCorp.db`; it listens on
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
  itself, so pass it raw text; escaping first double-escapes.

## Notes

- Do not run `git commit` yourself. Leave staging and committing to the
  repository owner.
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
- No committed `.clang-format`; existing code follows LLVM style (2-space
  indent, 80 columns).
