#!/usr/bin/env bash
set -euo pipefail
target=/home/khaled/smarthome/current
stage=/home/khaled/event-trigger-update
node=/home/khaled/smarthome/runtime/node/bin/node
files=(events.js local-server.js)
for f in "${files[@]}"; do "$node" --check "$stage/$f"; done
backup=$(mktemp -d /home/khaled/event-trigger-backup-XXXXXX)
for f in "${files[@]}"; do cp "$target/$f" "$backup/$f"; done
restore(){ for f in "${files[@]}"; do cp "$backup/$f" "$target/$f"; done; sudo -n /usr/bin/systemctl restart smarthome-local.service; echo "Restored $backup"; }
trap restore ERR
for f in "${files[@]}"; do cp "$stage/$f" "$target/$f.new"; mv "$target/$f.new" "$target/$f"; done
sudo -n /usr/bin/systemctl restart smarthome-local.service
for i in {1..20}; do
 if systemctl is-active --quiet smarthome-local.service && curl -fsS http://127.0.0.1:3000/health >/dev/null 2>&1; then trap - ERR; echo "Installed; backup: $backup"; exit 0; fi
 sleep 1
done
false
