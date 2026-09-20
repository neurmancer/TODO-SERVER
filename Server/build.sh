#!/bin/bash
set -euo pipefail

BINARY_NAME="server"
SERVICE_NAME="server"
INSTALL_PATH="/usr/local/bin/${BINARY_NAME}"
SERVICE_PATH="/etc/systemd/system/${SERVICE_NAME}.service"

die() { echo "ERROR: $*" >&2; exit 1; }

usage() {
    echo "Usage: $0 [install|update|local|sync-music|nuke-songs|remove|delete|--help] [--local]"
    echo "  install  Upgrade packages, build, install, and start the server (default)."
    echo '           Listen only on localhost:8080; use deploy.sh --domain for public HTTPS.'
    echo '  update   Rebuild, install, and restart the service using existing dependencies.'
    echo '  local    Build using the project .server directory; skip service setup.'
    echo '  sync-music  Download new MP3s from the public Drive library; no rebuild/restart.'
    echo '  nuke-songs  Delete the cached music in $HOME/.server/music; keep other runtime data.'
    echo 'Use --local with sync-music or nuke-songs to target the project .server instead.'
    echo 'install/update/local also sync music. Override the folder with TODO_MUSIC_FOLDER.'
    echo '  remove   Uninstall the service and executable; preserve $HOME/.server.'
    echo '  delete   Uninstall and permanently delete $HOME/.server/db and frontend.'
    echo 'Remove and delete leave installed system packages in place.'
}

[[ $# -le 2 ]] || die "Too many arguments. Use --help for usage."
ACTION=${1:-install}
case "$ACTION" in
    install|update|local|sync-music|nuke-songs|remove|delete) ;;
    -h|--help|help) usage; exit 0 ;;
    *) die "Unknown argument: $ACTION. Use --help for usage." ;;
esac
if [[ $# == 2 ]]; then
    [[ $2 == --local && ( $ACTION == sync-music || $ACTION == nuke-songs ) ]] || die "--local is only supported with sync-music or nuke-songs."
fi

# Build and maintain the data as the login user; elevate only system operations.
if [[ $(id -u) == 0 ]]; then
    die "Run ./build.sh as your regular user, without sudo. It uses sudo where needed."
fi

if [[ $ACTION != local && $ACTION != sync-music && $ACTION != nuke-songs ]]; then
    command -v sudo >/dev/null || die "sudo is required."
    command -v systemctl >/dev/null || die "systemd is required."
    [[ -d /run/systemd/system ]] || die "Boot this machine with systemd before managing the service."
fi

if [[ $ACTION == delete ]]; then
    [[ ${HOME:-} == /* && -d $HOME && -w $HOME ]] || die "HOME must be an existing, writable absolute directory."
    SERVER_PATH="$HOME/.server"
    [[ ! -L $SERVER_PATH ]] || die "Refusing to delete data through a symlink at $SERVER_PATH."
fi

# Uninstall independently of the source tree and build dependencies.
if [[ $ACTION == remove || $ACTION == delete ]]; then
    echo ">>> Removing ${SERVICE_NAME}.service..."
    LOAD_STATE=$(sudo systemctl show "${SERVICE_NAME}.service" --property=LoadState --value)
    if [[ $LOAD_STATE != not-found || -e $SERVICE_PATH || -L $SERVICE_PATH ]]; then
        sudo systemctl stop "${SERVICE_NAME}.service"
        sudo systemctl disable "${SERVICE_NAME}.service"
    fi
    sudo rm -f -- "$SERVICE_PATH" "$INSTALL_PATH"
    sudo systemctl daemon-reload
    echo "Service and executable removed."
    if [[ $ACTION == delete ]]; then
        rm -rf -- "$SERVER_PATH/db" "$SERVER_PATH/frontend"
        echo "Database and frontend deleted from $SERVER_PATH."
    else
        echo 'Database and frontend files under $HOME/.server were preserved.'
    fi
    exit 0
fi

[[ ${HOME:-} == /* && -d $HOME && -w $HOME ]] || die "HOME must be an existing, writable absolute directory."
[[ $HOME != *$'\n'* && $HOME != *$'\r'* ]] || die "HOME must not contain line breaks."
RUN_USER=$(id -un)
SERVER_PATH="$HOME/.server"
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [[ $ACTION == local || ${2:-} == --local ]]; then
    SERVER_PATH="$(dirname -- "$SCRIPT_DIR")/.server"
fi
# Keep certificate generation and the runtime pointed at the same data root.
export TODO_SERVER_PATH="$SERVER_PATH"
cd -- "$SCRIPT_DIR"

if [[ $ACTION == nuke-songs ]]; then
    [[ ! -L $SERVER_PATH && ! -L $SERVER_PATH/music ]] || die "Refusing to delete music through a symlink."
    rm -rf -- "$SERVER_PATH/music"
    echo "Cached songs deleted from $SERVER_PATH/music. Reload the app to clear its track list."
    echo "Run ./build.sh sync-music${2:+ --local} to download the library again."
    exit 0
fi

if [[ $ACTION == install ]]; then
    echo ">>> Upgrading packages and installing dependencies..."
    if command -v pacman >/dev/null; then
        sudo pacman -Syu --needed base-devel sqlite curl openssl python ffmpeg
    elif command -v apt-get >/dev/null; then
        sudo apt-get update
        sudo apt-get upgrade -y
        sudo apt-get install -y build-essential libsqlite3-dev sqlite3 curl libssl-dev openssl python3 ffmpeg
    else
        die "Supported package managers are pacman (Arch) and apt-get (Debian/Ubuntu)."
    fi
fi

command -v python3 >/dev/null || die "python3 is required to import the music library."
command -v ffprobe >/dev/null || die "ffprobe is required to validate MP3s. Install the ffmpeg package."

sync_music() {
    echo ">>> Checking the public Drive library for new MP3s..."
    python3 scripts/import-music.py --output "$SERVER_PATH/music" ||
        die "Music sync failed. Cached songs are preserved; rerun ./build.sh sync-music to retry."
}

if [[ $ACTION == sync-music ]]; then
    sync_music
    echo "Music ready. Reload the app/page to refresh its track list; no server restart needed."
    exit 0
fi

[[ -f Makefile && -f frontend/index.html && -f frontend/template.html ]] || die "Project sources or frontend files are missing."

echo ">>> Preparing HTTPS certificate..."
bash ./generate-cert.sh

echo ">>> Building with Makefile..."
make clean TARGET="$BINARY_NAME"
make TARGET="$BINARY_NAME"
[[ -x ./$BINARY_NAME ]] || die "Make did not produce an executable ${BINARY_NAME}."

# Downloads can take a while. Finish them before stopping the running service.
sync_music

TEMP_UNIT=""
TEMP_DB=""
cleanup() {
    if [[ -n $TEMP_UNIT ]]; then rm -f -- "$TEMP_UNIT"; fi
    if [[ -n $TEMP_DB ]]; then rm -f -- "$TEMP_DB"; fi
}
trap cleanup EXIT

if [[ $ACTION != local ]]; then
TEMP_UNIT=$(mktemp --suffix=.service)
#That shit is now some fucking bash wizardry
UNIT_HOME=${HOME//\\/\\\\}
UNIT_HOME=${UNIT_HOME//\"/\\\"}
UNIT_HOME=${UNIT_HOME//%/%%}
cat > "$TEMP_UNIT" <<EOF
[Unit]
Description=Todo server
After=network.target

[Service]
Type=simple
User=${RUN_USER}
Environment="HOME=${UNIT_HOME}"
WorkingDirectory=/
ExecStart=${INSTALL_PATH}
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

# Finish compilation before taking an existing installation offline.
LOAD_STATE=$(sudo systemctl show "${SERVICE_NAME}.service" --property=LoadState --value)
if [[ $LOAD_STATE != not-found ]]; then
    echo ">>> Stopping the existing service for installation..."
    sudo systemctl stop "${SERVICE_NAME}.service"
fi

fi

echo ">>> Installing runtime files in ${SERVER_PATH}..."
mkdir -p -- "$SERVER_PATH/db" "$SERVER_PATH/frontend" "$SERVER_PATH/music"
cp -R -- frontend/. "$SERVER_PATH/frontend/"

if [[ -e $SERVER_PATH/db/todo.db || -L $SERVER_PATH/db/todo.db ]]; then
    echo ">>> Preserving the installed database."
elif [[ -f src/db/todo.db ]]; then
    echo ">>> Migrating the source database with a SQLite backup..."
    # A SQLite backup includes committed WAL data, unlike copying just todo.db.
    TEMP_DB=$(mktemp "$SERVER_PATH/db/migration.XXXXXX")
    (
        cd -- "$SERVER_PATH/db"
        sqlite3 "$SCRIPT_DIR/src/db/todo.db" ".backup ${TEMP_DB##*/}"
    )
    # Never replace a database that appeared while the backup was running.
    ln -- "$TEMP_DB" "$SERVER_PATH/db/todo.db"
    rm -f -- "$TEMP_DB"
    TEMP_DB=""
else
    echo ">>> No source database found; the server will create a fresh todo.db."
fi

if [[ $ACTION == local ]]; then
    echo "Local build ready: ${SCRIPT_DIR}/${BINARY_NAME}"
    echo "Runtime files: ${SERVER_PATH}"
    echo "Site: https://localhost:8080 (self-signed certificate)"
    printf 'Run it with: TODO_SERVER_PATH=%q %q\n' "$SERVER_PATH" "${SCRIPT_DIR}/${BINARY_NAME}"
    exit 0
fi

echo ">>> Installing the executable and systemd unit..."
sudo install -m 0755 -- "./$BINARY_NAME" "$INSTALL_PATH"
sudo install -m 0644 -- "$TEMP_UNIT" "$SERVICE_PATH"
sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}.service"
sudo systemctl restart "${SERVICE_NAME}.service"

echo ">>> Checking startup and HTTPS responses..."
if ! curl --cacert "$SERVER_PATH/tls/cert.pem" --fail --silent --show-error --retry 5 --retry-connrefused \
    --retry-delay 1 --max-time 5 --output /dev/null https://127.0.0.1:8080/ ||
    ! curl --cacert "$SERVER_PATH/tls/cert.pem" --fail --silent --show-error --max-time 5 --output /dev/null https://127.0.0.1:8080/style.css ||
    ! sudo systemctl is-active --quiet "${SERVICE_NAME}.service"; then
    sudo systemctl status --no-pager "${SERVICE_NAME}.service" || true
    sudo journalctl -u "${SERVICE_NAME}.service" -n 30 --no-pager || true
    die "The server did not pass its startup checks. See the diagnostics above."
fi

echo "The backend listens only on localhost:8080; no public firewall port is needed."
echo "For public HTTPS, run ./deploy.sh --domain YOUR_DOMAIN after installing Caddy."

make clean

echo "=============================================="
echo "Server running at:   https://localhost:8080" # Might change in the future tho
echo "Binary installed to: ${INSTALL_PATH}"
echo "Runtime files:       ${SERVER_PATH}"
echo "Service runs as:     ${RUN_USER}"
echo "Starts automatically at boot."
echo ""
echo "Useful commands:"
echo "  ./build.sh update  # Rebuild, install, restart, and check HTTPS"
echo "  ./build.sh sync-music  # Download new songs without rebuilding/restarting"
echo "  ./build.sh nuke-songs  # Clear the installed music cache"
echo "  ./build.sh remove  # Uninstall; keep database and frontend files"
echo "  ./build.sh delete  # Uninstall and permanently delete database and frontend"
echo "  sudo systemctl restart ${SERVICE_NAME}"
echo "  sudo systemctl status ${SERVICE_NAME}"
echo "  sudo systemctl stop ${SERVICE_NAME}"
echo "  sudo journalctl -u ${SERVICE_NAME} -f"
echo "=============================================="
