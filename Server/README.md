# BASIC TO-DO SERVER (No longer only local host)


**Server commands below run from `Server/`:**

```bash
cd Server
```

Root `make`, `make run`, `make rebuild`, and `make clean` work too.

## Run the thing

Needs `make`, a C compiler(I mean yeah...duh), SQLite/OpenSSL development files, the `openssl`
CLI, and `sqlite3` if migrating an existing database.
Music import also needs Python 3 and `ffprobe` (from the `ffmpeg` package).
`./build.sh install` installs these dependencies on supported distributions.

```bash
./build.sh local
TODO_SERVER_PATH="$PWD/../.server" ./server
```

Open **https://localhost:8080**. Local mode needs no sudo, installs the frontend,
and preserves your existing local database in the project's `.server/` directory.
Installed builds use `$HOME/.server/` instead. Missing database? The server makes one.
Use the launch command printed by `build.sh local` so the binary uses the local runtime.
It only listens on loopback — LAN access doesn't magically fucking happen.

The build generates a self-signed certificate valid for a year. Your browser
will complain; trust `../.server/tls/cert.pem` for a local build (or
`$HOME/.server/tls/cert.pem` for the installed service). **Never share `key.pem`.**

```bash
curl --cacert "../.server/tls/cert.pem" https://localhost:8080/
# Need extra certificate names? Run before building:
TODO_SERVER_PATH="$PWD/../.server" ./generate-cert.sh 'DNS:todo.local,IP:192.168.1.20'
```

Existing certs stay put. to renew or change names move the old cert/key aside,
regenerate, restart the server, and trust the new cert.

## Install, update, remove

**DO not run as root**. Installation uses sudo itself, upgrades system packages on
Arch/Debian/Ubuntu, installs dependencies, and sets up `server.service`.
Read the fucking script before handing it privileges. **Or use local build if you don't trust the dev(I wouldn't if I were you)**

```bash
./build.sh install # Install the daemon
./build.sh update  # Rebuild, install, restart, check HTTPS
./build.sh remove  # Remove daemon/binary; keep runtime data
./build.sh delete  # Remove daemon AND runtime data. Yes, your todos too.
```

`update` keeps your database/certs and skips package upgrades. Compilation
happens before stopping the running service. Changed the frontend? Run it too.

## Login shit

Each account has its own todos. Users cannot view or change another account's
todos, even with a direct todo URL or ID. The music library remains shared.

```bash
python3 setup-auth.py --user yourname # First account, or reset this user's password
# Purely added just 'cuz my friend wannted to use this too lol
python3 setup-auth.py --user second  # Add another account; prompts for its password
python3 setup-auth.py --list
python3 setup-auth.py --user second --rename newname # Keeps password and todos
./build.sh update
```

For a local build, add `--runtime ../.server` to each setup command (or set
`TODO_SERVER_PATH`). The default runtime is `$HOME/.server`. Restart a manually
started backend instead of updating the daemon.

Accounts are stored in `auth/users.db` with unique usernames and salted
PBKDF2-SHA256 password hashes. Reusing a username resets only that account's
password. Restart after account changes to invalidate existing sessions.
Usernames are case-sensitive and accept 1–64 letters, digits, underscores, or
hyphens. Passwords need 12+ characters.

Existing single-user credentials are imported automatically by `setup-auth.py`
before adding another account. The original account keeps ID 1 and all existing
todos. On a server without previous credentials, the first account owns existing
local todos. Renaming an account preserves its ID and todos. Keep both runtime
databases (`auth/users.db` and `db/todo.db`) together when backing up or restoring.
The old credential file is ignored once the user database exists.

Open `/login` to sign in. Sessions last up to eight hours; logout clears that
session and a server restart clears all sessions. Five failed attempts pause
logins for a minute. Without either user database or legacy credentials, local
access has no login. **Set up login before exposing the thing.**

## Put it on the fucking internet

For direct public access, install [Caddy 2.8+](https://caddyserver.com/docs/install).
`deploy.sh` expects `/usr/bin/caddy`, its system user/group, and systemd.
Point DNS at your public IP and allow/forward TCP 80 and 443. Only add AAAA if
IPv6 actually works. **Don't expose port 8080.**

```bash
./build.sh install
./deploy.sh --domain todo.example.com --user todo
```

It asks for a password and creates `todo-caddy.service`. Caddy handles public
certificates and verifies the local backend's cert. If an unused default Caddy
service owns the ports, stop it first. Already hosting other sites? Integrate
this app into that config instead of kicking your other shit offline.

```bash
sudo journalctl -u todo-caddy
sudo systemctl disable --now todo-caddy # Stop public access
```

Rerun `deploy.sh` to change the domain/password. After renewing the backend
cert, restart `server.service` and rerun deployment to refresh Caddy's copy.
Stop public access before removing the backend; `build.sh` leaves the gateway.

## ISP doing CGNAT bullshit?

Use Cloudflare Tunnel instead of the Caddy route. Needs a domain on Cloudflare
and separately installed `cloudflared`. Set up app login first, enable
**Always Use HTTPS** in Cloudflare, then authorize with `cloudflared tunnel login`.
The server machine still needs to stay on, obviously.

The existing tunnel setup routes to `https://127.0.0.1:8080`, uses `caPool` to
trust the backend cert with hostname `localhost`, and checks login is enabled
before starting. Its private config is `$HOME/.cloudflared/todo-server.yml`;
its user service is `$HOME/.config/systemd/user/todo-tunnel.service`.
These are local deployment files, not installed by `build.sh` or `deploy.sh`.

```bash
systemctl --user status todo-tunnel
systemctl --user restart todo-tunnel
journalctl --user -u todo-tunnel -f
systemctl --user disable --now todo-tunnel # Stop public access
```

User lingering keeps the configured tunnel running without a desktop login.
After renewing the backend cert, restart both backend and tunnel. Don't use
`deploy.sh` to manage this route — that one's for Caddy.

## Independent MP3 jukebox

### Well...This shit is too long I'll format those readmes when I am done with the fucking project...

Each browser/app has its own shuffled queue, playback position, pause/resume,
and seek control. Nothing plays until the listener taps Play. Music keeps
playing during the site's internal TODO navigation. The Android WebView still
does not promise screen-off/background playback.

`./build.sh install`, `update`, and `local` automatically import MP3s from the
[Napster 2.0](https://drive.google.com/drive/folders/1vR_uOUoc2pt6UXissU6Z4Cns8Ema39Y1)
into `$HOME/.server/music/` for installed builds, or the project's `.server/music/`
for `local`, using only MP3s inside its subfolders (recursively).
MP3s directly in the Drive root folder are skipped. Earlier root-level imports
are moved into a hidden `.excluded-root-*` archive during sync, removing them
from playback without deleting the files. Anyone installing the
project gets this library by default. Initial setup downloads the library;
later runs skip existing files and download newly added songs.

When songs are added to Drive, pull them without rebuilding or restarting:

```bash
./build.sh sync-music
./build.sh sync-music --local # Target the local build's cache instead
```

To clear cached songs, including old copies after renaming tracks on Drive:

```bash
./build.sh nuke-songs
./build.sh sync-music
# For a local build, add --local to both commands.
```

`nuke-songs` permanently removes only the selected music directory, including
archived songs. It leaves todos, frontend, credentials, certificates, and Drive
files intact. It needs no sudo, rebuild, or restart. Reload the page afterward.

Then reload the page/app to refresh its track list. There is no background
polling: additions are fetched on the next build or explicit `sync-music` run.
Drive deletions do not delete local songs. Build-time sync finishes before
stopping an installed service; a failed sync stops the build and preserves
the running service and previously cached music.

To use another public folder, set `TODO_MUSIC_FOLDER` when running `build.sh`,
or pass a folder to the importer. You can also place MP3s directly in the music
directory:

```bash
# Inspect first; this does not download files:
python3 scripts/import-music.py --list
# Import MP3s only; requires ffprobe from the FFmpeg package:
python3 scripts/import-music.py 'PUBLIC_FOLDER_LINK_OR_ID'
```

### Nerdy part 

```text

    The importer ignores non-MP3 files, validates downloaded audio, and publishes
    files atomically. Existing files are skipped; rerun to retry failures or add
    new songs. It never deletes cached music. If a Drive file's contents change,
    remove its cached copy before importing again. Downloads are capped at 256 MiB
    per track by default (`--max-mib` changes this); `--limit 1` imports one track
    for a quick check, and `--output PATH` chooses another cache directory.
    Public Drive listing/download pages can change or be rate-limited; import
    failures leave the existing cache usable. No Drive credentials go to clients.

    After changing the code/frontend, run `./build.sh update` for an installed
    service, or `./build.sh local` and restart a manually launched server. Music
    is preserved by installation/update. Refresh the webpage after adding tracks;
    an empty-library Play button also refreshes the catalogue.

    The server exposes authenticated `/jukebox/songs` JSON and
    `/jukebox/audio/<track-id>` MP3 responses. Audio supports GET, HEAD, and single
    byte ranges (206/416) for browser seeking. Local symlinks and non-regular files
    are excluded. The library supports up to 4096 tracks in the runtime directory;
    the importer flattens its folder tree and distinguishes duplicate titles.

    Audio transfers run in up to eight forked workers. The parent retains all
    database/session changes and continues serving the site. Workers close their
    copy of the listening socket, exit on disconnect/write timeout, are reaped
    while idle, and stop when the server shuts down. Each transfer has a ten-minute
    upper bound. Excess simultaneous transfers receive 503 and can be retried.
    Playback needs no FFmpeg process, YouTube iframe, or shared radio service.
```

```bash
make test # Python 3, Node.js, C compiler; uses isolated loopback port 18081
```

On devices, check first-tap playback, pause/resume, seeking, Next, automatic
advance, and TODO navigation in Android Chrome, the Android app, and iOS Safari.
Open two clients and confirm their controls do not affect each other.

## Keep private shit out of Git

No databases, keys, credentials, deployment configs, or logs in commits.
Caddy config lives in `/etc/todo-caddy/`, its state in `/var/lib/todo-caddy/`,
and app accounts in `$HOME/.server/auth/users.db` (legacy: `auth/credentials`). Cloudflare credentials
stay local too. Your domain can still appear in shell history and logs.

## Check it / legal shit

With Caddy on `PATH`:

```bash
python3 scripts/test-deployment.py
```

[GPL-3.0-or-later](LICENSE.md). Dependencies keep their own licenses;
see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Respect the licenses
and go feral with the rest. luv u <3 OwO
