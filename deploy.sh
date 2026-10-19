#!/usr/bin/env bash
# Deploy LexiCorp: pull, build, test, install to /opt/lexicorp, restart.
# Run as a normal user from anywhere; uses sudo only where needed.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_DIR=/opt/lexicorp
DATA_DIR=/var/lib/lexicorp
SERVICE=lexicorp

cd "$REPO_DIR"

echo "==> Pulling"
git pull --ff-only

echo "==> Building"
cmake --preset release
cmake --build --preset release -j2

echo "==> Testing"
ctest --preset release --output-on-failure

echo "==> Backing up database"
if [[ -f "$DATA_DIR/lexiCorp.db" ]]; then
  sudo -u lexicorp sqlite3 "$DATA_DIR/lexiCorp.db" \
    ".backup $DATA_DIR/backup-predeploy.db"
fi

echo "==> Installing"
# Copy to a temp name and rename: overwriting a running binary in place
# fails with "Text file busy".
sudo install -m 755 build/release/src/LexiCorp "$INSTALL_DIR/LexiCorp.new"
sudo mv "$INSTALL_DIR/LexiCorp.new" "$INSTALL_DIR/LexiCorp"
sudo rsync -a --delete resources/ "$INSTALL_DIR/resources/"

echo "==> Restarting"
sudo systemctl restart "$SERVICE"
sleep 2
if systemctl is-active --quiet "$SERVICE"; then
  echo "==> Deployed $(git rev-parse --short HEAD)"
else
  echo "!! $SERVICE failed to start:" >&2
  journalctl -u "$SERVICE" -n 20 --no-pager >&2
  exit 1
fi
