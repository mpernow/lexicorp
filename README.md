# LexiCorp

This is an application to aid in language learning. It is used to analyse text in a given language and extract the most frequent words, building up a corpus of words in their sentences.

# Building

From the project root, run `cmake --preset <preset>` and `cmake --build --preset <preset>`, where `<preset>` is one of `debug` or `release`. This will create a `/build` directory and build it there.

# Running

The easiest way to run it is with `cmake --build build/debug --target run` or `cmake --build build/release --target run`.
Then open a browser and navigate to the page. For a debug build, it is located at `http://localhost:9090` and for release it is `http://localhost:8080`.

To run the binary directly, pass the document root (the folder containing `resources/`) and Wt's own resources directory (usually `/usr/local/share/Wt/resources` when Wt is built from source). The `;/resources` after the document root means only that folder is served as static files; everything else is handled by the app, which gives clean URLs like `/list` and keeps other files in the document root (such as the database) from being downloadable:

```
./build/release/src/LexiCorp --docroot ".;/resources" --resources-dir /usr/local/share/Wt/resources \
    --http-address 0.0.0.0 --http-port 8080
```

The database, `lexiCorp.db`, is created in the working directory.

# Tests

Tests are implemented using Google Test. They are run with `ctest --preset <release/debug> --output-on-failure`.

# Dependencies

- Wt 4.12.1. For installation instructions, see https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html. This requires:
  - boost: `sudo apt install libboost-all-dev`
  - zlib (libz): `sudo apt install zlib1g-dev`
  - OpenSSL (libopenssl, in order to use HTTPS): `sudo apt install libssl-dev openssl`
  - Then install the Wt package by downloading the release from github and building it.
- CMake 3.21 or later
- g++ 13.3
- SQLite3: `sudo apt install libsqlite3-dev sqlite3`
- vsqlite++: fetched from GitHub when building, see https://github.com/vinzenz/vsqlite-- and `src/db/CMakeLists.txt`
- googletest: fetched from GitHub when building, see `CMakeLists.txt`
- ICU: `sudo apt install libicu-dev`
- libcurl (for the Claude API): `sudo apt install libcurl4-openssl-dev`
- OpenSSL 3.0.13

The Python scripts in `tools/` have their own dependencies, in the `Pipfile`
(`sudo apt install pipenv`): run `pipenv sync` once, then e.g.
`pipenv run python tools/grammar_notes.py --list`. `tools/import_wiktionary.py`
needs only the standard library, so the Pi doesn't need them.

# Deployment

On the server (e.g. a Raspberry Pi), the app runs as a systemd service under its own user, with the binary and `resources/` in `/opt/lexicorp` and the database in `/var/lib/lexicorp`. One-time setup:

```
sudo useradd --system --home /var/lib/lexicorp --create-home lexicorp
sudo mkdir -p /opt/lexicorp
sudo apt install rsync sqlite3
```

`/etc/systemd/system/lexicorp.service`:

```
[Unit]
Description=LexiCorp
After=network-online.target
Wants=network-online.target

[Service]
User=lexicorp
WorkingDirectory=/var/lib/lexicorp
ExecStart=/opt/lexicorp/LexiCorp --docroot "/opt/lexicorp;/resources" \
  --resources-dir /usr/local/share/Wt/resources \
  --http-address 127.0.0.1 --http-port 8080
Restart=on-failure
# Optional: enables "Ask Claude" glosses (see below).
EnvironmentFile=-/etc/lexicorp/claude.env

[Install]
WantedBy=multi-user.target
```

Run `sudo systemctl daemon-reload`, then `./deploy.sh` to build and install, then `sudo systemctl enable lexicorp` so it starts at boot. The app only listens on localhost; `sudo tailscale serve --bg 8080` makes it available over HTTPS on the tailnet.

## Claude glosses

With an API key, a Kurdish word without a gloss gets an "Ask Claude" button on its sources page. The answer (glosses, plus any idioms the word is part of in its sentences) is stored as AI-generated dictionary entries, and the Home page shows this month's requests and their estimated cost. Without a key the button doesn't appear.

With the grammar reference too (`LEXICORP_GRAMMAR_FILE`), each Kurdish sentence on that page gets an "Explain" button: a translation, every word glossed interlinear and split into its parts, and notes on the grammar, citing the reference's sections. Explanations are stored, so a sentence is paid for once.

Create a key in the Claude Console (set a monthly spend limit there), then on the server:

```
sudo mkdir -p /etc/lexicorp
sudo install -m 600 -o root -g root /dev/null /etc/lexicorp/claude.env
sudoedit /etc/lexicorp/claude.env
```

with

```
ANTHROPIC_API_KEY=sk-ant-...
# Optional; the default is claude-sonnet-5-5.
LEXICORP_CLAUDE_MODEL=claude-sonnet-5-5
# Optional: the "Explain" button on Kurdish sentences needs the grammar
# reference built from the grammar books (see TODO.md), readable by the
# lexicorp user.
LEXICORP_GRAMMAR_FILE=/var/lib/lexicorp/grammar.md
# Optional: a condensed version of it with the same sections
# (tools/grammar_merge.py --condense), sent to Claude instead to make
# explanations cheaper; the full one is still what the app shows.
LEXICORP_GRAMMAR_PROMPT_FILE=/var/lib/lexicorp/grammar-condensed.md
# Optional; the default is LEXICORP_CLAUDE_MODEL.
LEXICORP_CLAUDE_EXPLAIN_MODEL=claude-sonnet-5-5
```

and restart the service. systemd reads the file as root, so the `lexicorp` user can't. Locally, export the same variables before `cmake --build build/debug --target run`.

The prompts are `prompts/define_word.md` and `prompts/explain_sentence.md`; they are compiled into the binary, so rebuild after editing them. The grammar reference isn't (it is derived from copyrighted books), so it stays out of git and is read at startup.

To deploy updates, run `./deploy.sh` from the repo on the server. It pulls, builds, runs the tests, backs up the database, installs to `/opt/lexicorp` and restarts the service.

## Dictionary

Glosses for Kurdish words come from Wiktionary, via the kaikki.org extract. Download it and import it into the database (the app must have run once, so the tables exist); re-running replaces the previous import and keeps glosses entered by hand:

```
curl -LO 'https://kaikki.org/dictionary/Central%20Kurdish/kaikki.org-dictionary-CentralKurdish.jsonl'
sudo -u lexicorp python3 tools/import_wiktionary.py \
  --db /var/lib/lexicorp/lexiCorp.db kaikki.org-dictionary-CentralKurdish.jsonl
```

# Licence

The Noto Naskh Arabic font in `resources/fonts/` is licensed under the SIL Open Font License 1.1; see `resources/fonts/OFL.txt`.
