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
- OpenSSL 3.0.13

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

[Install]
WantedBy=multi-user.target
```

Run `sudo systemctl daemon-reload`, then `./deploy.sh` to build and install, then `sudo systemctl enable lexicorp` so it starts at boot. The app only listens on localhost; `sudo tailscale serve --bg 8080` makes it available over HTTPS on the tailnet.

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
