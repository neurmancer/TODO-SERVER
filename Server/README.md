# BASIC TO-DO SERVER (No longer only local host)


**Server commands below run from `Server/`:**

```bash
cd Server
```

Root `make`, `make run`, `make rebuild`, and `make clean` work too.

## Run the thing

Needs `make`, a C compiler(I mean yeah...duh), SQLite/OpenSSL development files, the `openssl`
CLI, and `sqlite3` if migrating an existing database.

```bash
./build.sh local
./server
```

Open **https://localhost:8080**. Local mode needs no sudo, installs the frontend,
and preserves your existing database. Missing database? The server makes one.
It only listens on loopback — LAN access doesn't magically fucking happen.

The build generates a self-signed certificate valid for a year. Your browser
will complain; trust `$HOME/.server/tls/cert.pem` locally. **Never share `key.pem`.**

```bash
curl --cacert "$HOME/.server/tls/cert.pem" https://localhost:8080/
# Need extra certificate names? Run before building:
./generate-cert.sh 'DNS:todo.local,IP:192.168.1.20'
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

```bash
python3 setup-auth.py --user todo
./build.sh update
```

Open `/login`. For a manually started server, restart it instead of updating
the daemon. Same command changes the password; restart afterward.
Without configured credentials, local access has no login.

Passwords need 12+ characters and are stored as well-seasoned(salted) hashes. Sessions last up to eight hours; logout or a server restart clears them. Five failed attempts
pause logins for a minute. **Set up login before exposing the thing.**

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

## Keep private shit out of Git

No databases, keys, credentials, deployment configs, or logs in commits.
Caddy config lives in `/etc/todo-caddy/`, its state in `/var/lib/todo-caddy/`,
and app credentials in `$HOME/.server/auth/credentials`. Cloudflare credentials
stay local too. Your domain can still appear in shell history and logs.

## Check it / legal shit

With Caddy on `PATH`:

```bash
python3 scripts/test-deployment.py
```

[GPL-3.0-or-later](LICENSE.md). Dependencies keep their own licenses;
see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Respect the licenses
and go feral with the rest. luv u <3 OwO
