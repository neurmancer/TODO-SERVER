# BASIC TODO-SERVER

### A Shitty Enterprise™ Product

> A basic todo-server to keep track of your idaes (local network)

_Yet again...I lowkey gotta change the title..._ 




> This project started as a proof of concept then started to get **bigger**
and now I need a separate repo to keep developing it 
> And yeah I am getting more and more corpo with building a *FUCKING* CRUD App (I am not a corpo™)

---

## Table of Contents

> and yeah I'll keep a bullet point in every repo just pointing the table of contents

- [ToC](#table-of-contents)
- [Where the shit lives](#project-layout)
- [Disclaimer](#disclaimer)
- [Shit I've Built](#features)
- [Bragging](#other-stuff)
- [Run the thing locally](#developerlocal-server-test)
- [HTTPS and browser complaints](#https)
- [Full Build](#full-build)
- [Shit I might build next](#future-updates)
- [Online Drafting](#deploy-on-your-domain)
- [CGNAT being a pain? Use a tunnel](#cloudflare-tunnel-for-cgnat)
- [Keep the private shit private](#keep-the-private-shit-private)
- [Updating or taking it down](#updating-or-taking-it-down)
- [Check that the thing actually works](#check-if-that-thing-actually-works)
- [In Dev or will be In Dev](#dev-blog)
- [Legal Shit](#legal-stuff-and-licensing)

---

### Project layout

The server stuff lives in `Server/` now. Making room for the client without
turning the repo root into another fucking junk drawer.

```text
README.md
LICENSE.md
THIRD_PARTY_LICENSES.md
Server/
    Makefile
    build.sh
    deploy.sh
    generate-cert.sh
    setup-auth.py
    serverThingy.c
    frontend/
    src/
    scripts/
    vendor/
```

**Run the commands below from inside `Server/`:**

```bash
cd Server
```

Or update the daemon straight from the repo root with `./Server/build.sh update`.
For a plain compile from the root, use `make -C Server`.
The existing web frontend stays with the server for now. Runtime data in
`$HOME/.server` and the installed daemon/tunnel don't move with the source files.

### Disclaimer

> The build system (build.sh) uses momenteraly sudo privs and **DO NOT** use it if you are not comfortable with that
-   For further implementation details about it check [Other stuff](#other-stuff)

--- 

### Features

> A To-Do app that does the thing on localhost (for now) 

- Implemented shit:
-   A basic templating engine with my own syntax (I'd forget the syntax otherwise)
-   Database for keeping shit sane and classified appropriately
-   Basic router/ framework-ish behavior for handling requests
-   Markdown style rendering support (Outsourced via MD4C)
-   A randomized song selection 'cuz why not
-   A cool front-end (really tho it looks dope!)
-   HTTPS Certificate generation
-   HTTPS on localhost:8080, and now you can put the thing on your own domain too

--- 

### Other stuff

> This part is where I brag about execution/compiling specifications

#### Developer&Local Server Test

Wanna just run the thing locally? No sudo or systemd ceremony needed:

```bash
./build.sh local
./server
```

This builds `./server` and puts the frontend in `$HOME/.server/frontend`.
If there's no installed db yet, it brings `src/db/todo.db` over to
`$HOME/.server/db`. Already got a db there? It leaves your shit alone.
No source db either? I got you, the server makes one on its first run.

You still gotta have the dependencies installed tho: `make`, a C compiler (I assume you know what a compiler is...),
SQLite and OpenSSL development files, the `openssl` CLI, and `sqlite3` if
there's a db to migrate. Local mode won't go package shopping for you.

or you can just manually move the frontend and db folders to $HOME/.server/
and do shit like:
```bash
./generate-cert.sh # generate the local HTTPS certificate once
make #to build
make run # still unnecessary but to run (simply ./server)
make clean #to clean shit
```
manually


#### HTTPS

Start the server and open **https://localhost:8080**. OpenSSL handles HTTPS
with TLS 1.2 or newer, and the server sticks to loopback (this machine only)
LAN and internet access **do not** fucking magically happen just 'cuz it's runnin

Forgot the `https://` part? Plain HTTP GET/HEAD requests get redirected to
`https://localhost:8080/` now. The actual app still goes through HTTPS.
Yes... typing localhost shouldn't have been a whole fucking debugging session.

The build script makes a self-signed RSA certificate that lasts 365 days:
`$HOME/.server/tls/cert.pem`, with its private key in `$HOME/.server/tls/key.pem`.
Already got a certificate? It keeps it.

The cert covers `localhost`, `127.0.0.1`, and whatever active IPv4 addresses
`ip` finds when it's generated. Need another name or address on it? Generate
it before building:

```bash
./generate-cert.sh 'DNS:todo.local,IP:192.168.1.20'
```

The browser will still complain 'cuz this certificate is self-signed.
Trust `cert.pem` through your browser/OS certificate settings if you want to
use it locally. **DO NOT share `key.pem`.** That one's private for a fucking reason.

To check that HTTPS actually works with certificate verification still on:

```bash
curl --cacert "$HOME/.server/tls/cert.pem" https://localhost:8080/
openssl x509 -in "$HOME/.server/tls/cert.pem" -noout -fingerprint -sha256
```

If an address on the cert changes or the cert expires, move the old cert/key
pair aside, rerun `./generate-cert.sh`, and restart the server. You'll need to
trust the new cert again. Or put a CA-issued PEM certificate chain and its
matching unencrypted private key at those same paths, then restart.


#### Full Build 

**Important Shit**

build.sh will use **sudo** (elevated) privs for update, package installation, and moving server and server.service files to the said folders 
If you haven't checked the code yourself or you don't trust the author (me (the gremlin with dyslexia)) **DO NOT ALLOW**

---
- Full build file is not a mere build file the file does various tasks such as:

-   Update system packages (on Arch and Debian/Ubuntu)
-   Install dependencies
-   Compiles the server
-   creates server.service and moves it into /etc/systemd/system/
-   moves the server into /usr/local/bin/
-   cleans the residue of the build before leaving
-   keeps the backend chilling on localhost:8080 (no public firewall ports opened)
-   Now it deploys the site too Da fuck?

To use:

```bash

chmod +x build.sh #if not an executable already
./build.sh  #To build the server
./build.sh update # Rebuild, install, restart, and check HTTPS using installed dependencies
./build.sh remove #To remove daemon and the service file (user db && frontend is presevered )
./build.sh delete #To delete db and frontend as well as unloading the daemon (nukes everything)

```

Changed some shit because div wasn't 'centered enough'? and wanna get the daemon caught up? Just run
`./build.sh update` as your regular user. It builds first, then stops the
service, installs the binary and frontend, restarts the thing, and checks HTTPS.
If compilation eats shit, it hasn't stopped your running server yet.

Your db and certificates stay put. It'll ask for sudo where installation and
service management need it, but skips the whole package-upgrade adventure.
Local build leftovers get cleaned only after the installed server passes its
checks. No more manually juggling `make`, `install`, and `systemctl`... I made
a build script for a reason apparently.

---


### Future Updates

- I am not promising those but I do say there is a non-zero chance of those happen to exist in this repo one day

- new error reporting using err (BSD version) to journal (still uses perror but journalctl handled)

- Put the thing on your own domain via Caddy (Done, instructions below)

- HTTPS with OpenSSL and local certificate generation (Done)

- And changed colors for highlighted c format markdown (left for the end user I am fine with the current colors)

---


### Deploy on your domain

> So localhost isn't enough anymore... fine, we're putting the fucking thing on a domain.

The default is still **https://localhost:8080** with a self-signed cert.
You don't need a domain just to use the app. For public access, Caddy runs on
the same Linux machine and forwards requests to the local HTTPS server.

1. Run `./build.sh install` as your regular user to get the backend installed.
2. Install **Caddy 2.8 or newer** using the [official package instructions](https://caddyserver.com/docs/install).
   Yep, you gotta install this one separately. `deploy.sh` expects `/usr/bin/caddy`,
   the `caddy` system user/group, and systemd.
   If the default Caddy service is running but isn't hosting anything else,
   stop and disable it with `sudo systemctl disable --now caddy`.
   Already hosting other sites with Caddy? Add this app to that configuration;
   this script sets up its own gateway, so don't kick your other sites offline.
3. Point your domain's A record at this machine's public IPv4 address.
   Only add an AAAA record if IPv6 actually reaches the machine too.
   Allow inbound TCP 80 and 443 in your host/provider firewall, and forward
   those ports if you're behind a router. **Don't forward 8080.**
   Got old public 8080 firewall rules from an earlier build? Remove those too.
   The backend now listens only on loopback either way.
4. Give it your domain locally (the example below is... an example):

   ```bash
   ./deploy.sh --domain todo.example.com
   # Wanna pick the login username too? Default is todo:
   ./deploy.sh --domain todo.example.com --user mylogin
   ```

It asks you to choose and confirm a password (at least 12 characters), checks
Caddy's config, and sets up `todo-caddy.service` to start at boot. Daemon shit,
just like the backend. You'll need Python 3 for the password setup; the full
build installs it too.

Wanna use the login locally without a domain? From the project folder:

```bash
python3 setup-auth.py --user todo #(or make this whater you want)   # What have I become? Tie getting another knot around the neck...
./build.sh update
```


Then open **https://localhost:8080/login**. You can also preview that page before
configuring credentials, but signing in won't work until you've set them up.
Without credentials, the original local workflow still works without a login.

Already deployed with the old browser popup? Run `./build.sh update`, then rerun
`./deploy.sh --domain YOUR_DOMAIN`. Pick a password again when prompted; that
replaces the old gateway login with the app's login page.

Sessions last up to eight hours. **Dip out** invalidates the current session;
restarting the server clears all sessions. Passwords are stored as salted(well seasoned)
PBKDF2-SHA256 hashes (600,000 iterations), and cookies are Secure, HttpOnly, and
SameSite=Strict. Five failed attempts pause login attempts for a minute across
this single-user server. Changing the password uses the same setup command;
restart the backend afterward so it loads the new credentials.

Caddy also checks the backend's certificate. We're keeping verification on.
The backend handles one HTTP/1.1 request per connection.

Once DNS and network access are sorted, Caddy gets the public certificate and
renews it automatically. A service saying "running" doesn't mean the cert is
ready tho. Open your domain over HTTPS and check `sudo journalctl -u todo-caddy`
if the browser is still unhappy. The actual docs, if you wanna dig into it:
[Caddy automatic HTTPS](https://caddyserver.com/docs/automatic-https) and
[the password-storage guidance](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).

#### Cloudflare Tunnel for CGNAT

ISP put you behind CGNAT? Port forwarding on your router alone won't get this
thing online. Cloudflare Tunnel lets the machine connect outward instead.
The domain needs to be active on Cloudflare, and `cloudflared` needs to be
installed separately. Your app still runs here, so keep the machine awake.

For this setup, traffic goes through Cloudflare and `cloudflared` straight to
`https://127.0.0.1:8080`. Caddy isn't needed in that path. Enable **Always Use
HTTPS** under your domain's **SSL/TLS → Edge Certificates** settings too.

Set up the app login with `python3 setup-auth.py --user todo`, then run
`./build.sh update` before publishing anything. Authorize the connector locally
with `cloudflared tunnel login`. Keep the authorization certificate and tunnel
credentials out of the repo, obviously.

The configured tunnel uses a user service named `todo-tunnel.service`:

```bash
systemctl --user status todo-tunnel
systemctl --user restart todo-tunnel
journalctl --user -u todo-tunnel -f
systemctl --user disable --now todo-tunnel # Take public access offline
```

Its private configuration lives in `$HOME/.cloudflared/todo-server.yml`; the
service file lives in `$HOME/.config/systemd/user/todo-tunnel.service`.
The configuration pins the backend certificate through `caPool`, expects
`localhost` on that certificate, and keeps TLS verification on. A startup check
refuses to launch the tunnel unless the backend says its login is enabled.

User lingering is enabled for boot startup, so the tunnel doesn't need an open
terminal or desktop login. The backend is still the system `server.service`.
After replacing the backend's local certificate, restart both services so the
tunnel loads its new trusted certificate. Don't run `deploy.sh --domain` to
manage this setup; that script installs the separate Caddy deployment.

#### Keep the private shit **private**

Your actual domain goes in `/etc/todo-caddy/Caddyfile`, readable only by root
and the caddy group. The username, salt, and password hash stay in
`$HOME/.server/auth/credentials` (mode `600`, in a `700` directory). Caddy keeps its certificates, private keys,
and account data under `/var/lib/todo-caddy`, with restricted permissions.
The backend key stays in `$HOME/.server/tls`; only its public certificate gets
copied to `/etc/todo-caddy/backend-cert.pem`.

**None of that shit belongs in Git.** Don't drag deployment configs, logs, your love-life, or runtime
folders into a commit. The domain can still show up in shell history, process
arguments during deployment, and local service logs. If you don't want to type
the actual domain into shell history:

```bash
read -r -p 'Domain: ' TODO_DOMAIN
./deploy.sh --domain "$TODO_DOMAIN"
unset TODO_DOMAIN
```

#### Updating or taking it down

Run `deploy.sh` again to change the hostname, pick a new login password, and
restart the gateway. The backend's self-signed cert still expires yearly:
renew it using the local HTTPS instructions above, restart `server.service`,
and rerun `deploy.sh` so Caddy gets the new trusted copy. Caddy handles renewal
of the public cert automatically; that's a separate cert.

Wanna stop public access? `sudo systemctl disable --now todo-caddy`.
Do that before `./build.sh remove` or `./build.sh delete`. Those remove the
backend; they leave the gateway config and certificates alone.

#### Check if that thing actually works

With Caddy on `PATH`, run:

```bash
python3 scripts/test-deployment.py
```

**I ain't explaining python shit for fuck sake**
- figure it out yourself it's English anyways


### Dev Blog

- An andriod client app using ja*a (I mean I've already crossed the language borders for this repo so...java might appear as well)

- Login Page (Also done)

- Actual deployment process (I am still trying to figure out why my ISP bitching about my setup) (Done and gone)

- Cloudflare stuff (Done) 


### Legal Stuff and Licensing

This project is licensec under GPL-3.0 thing if you're a badge or law nerd the proper sources are:
[This Repo's License](LICENSE.md)
[Third Party License](THIRD_PARTY_LICENSES.md)

all the third party files placed under vendor subfolder with their respective license so... don't make me say those as if I am in a tux 
you got the idea basically don't steal shit, respect to the MD4C and go feral about the rest luv u <3 OwO

And the certificate generation bits use OpenSSL's
[`req -x509` and `-addext` options](https://docs.openssl.org/3.4/man1/openssl-req/),
if you wanna see what those flags are doing.
