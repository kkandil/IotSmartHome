#!/usr/bin/env bash
set -euo pipefail
if [[ $EUID -ne 0 ]]; then echo 'Run with sudo bash install-activity-update.sh'; exit 1; fi
source_dir=$(cd -- "$(dirname -- "$0")" && pwd)
target=$(systemctl show smarthome-local.service --property=WorkingDirectory --value)
case "$target" in
  /home/khaled/smarthome/current|/opt/smarthome/current) ;;
  *) echo "Unexpected service directory: $target. No files changed."; exit 1;;
esac
[[ -f "$target/local-server.js" && -d "$target/node_modules" ]] || { echo 'Existing hub installation not found'; exit 1; }
node=/home/khaled/smarthome/runtime/node/bin/node
[[ -x "$node" ]] || node=/opt/smarthome/runtime/bin/node
[[ -x "$node" ]] || { echo 'Existing Node runtime not found'; exit 1; }
files=(activity-history.js esp-action.js events.js home-sync.js local-server.js protocol.js device-templates.js control-history.js variable-state.js)
for file in "${files[@]}"; do "$node" --check "$source_dir/$file"; done
backup="$(dirname "$target")/backups/activity-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$backup"
chmod 700 "$backup"
for file in "${files[@]}"; do [[ ! -f "$target/$file" ]] || cp -p "$target/$file" "$backup/$file"; done
printf '%s\n' "Backup: $backup"
# Only the hub service is stopped. Configuration, firmware and voice files are preserved.
systemctl stop smarthome-local.service
recover() {
  echo 'Update failed; restoring previous code.'
  for file in "${files[@]}"; do [[ ! -f "$backup/$file" ]] || cp -p "$backup/$file" "$target/$file"; done
  systemctl restart smarthome-local.service || true
  echo "Review: sudo journalctl -u smarthome-local -n 60 --no-pager"
}
trap recover ERR
for file in "${files[@]}"; do install -m 644 "$source_dir/$file" "$target/$file"; done
systemctl start smarthome-local.service
for attempt in {1..20}; do
  if systemctl is-active --quiet smarthome-local.service && curl -fsS http://127.0.0.1:3000/health >/dev/null; then
    trap - ERR
    echo 'Hub update installed. Install the new APK, then open history in the app.'
    echo "Rollback code is saved at $backup"
    exit 0
  fi
  sleep 1
done
false
