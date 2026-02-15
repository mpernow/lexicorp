# LexiCorp

This is an application to aid in language learning. It is used to analyse text in a given language and extract the most frequent words, building up a corpus of words in theit sentences.

# Building

From the project root, run `cmake --preset <preset>` and `cmake --build --preset <preset>`, where `<preset>` is one of `debug` or `release`. This will create a `/build` directory and build it there.

# Running

The easiest way to run it is with `cmake --build build/debug --target run` or `cmake --build build/release --target run`.
Then open a browser and navigate to the page. For a debug build, it is located at `http://localhost:9090` and for release it is `http://localhost:8080`.

# Tests

Tests are implemented using Google Test. They are run with `ctest --preset <release/debug> --output-on-failure`.

# Dependencies

- Wt 4.12.1. For installation instructions, see https://www.webtoolkit.eu/wt/doc/reference/html/InstallationUnix.html. This requires:
  - boost: `sudo apt install libboost-all-dev`
  - zlib (libz): `sudo apt install zlib1g-dev`
  - OpenSSL (libopenssl, in order to use HTTPS): `sudo apt install libssl-dev openssl`
  - Then install the Wt package by downloading the release from github and building it.
- CMake 3.20 or later
- g++ 13.3
- SQLite3: `sudo apt install libsqlite3-dev sqlite3`
- vsqlite++: fetched from GitHub when building, see https://github.com/vinzenz/vsqlite-- and `src/db/CMakeLists.txt`
- googletest: fetched from Github when building, see `src/CMakeLists.txt`
