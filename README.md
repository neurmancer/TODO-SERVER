# BASIC TODO-SERVER

> A basic todo-server to keep track of your idaes (local network)

_Yet again...I lowkey gotta change the title..._ 


> This project started as a proof of concept then started to get **bigger**
and now I need a separate repo to keep developing it 
> And yeah I am getting more and more corpo with building a *FUCKING* CRUD App (I am not a corpo™)

---

## Table of Contents 

- [ToC](#table-of-contents)
> and yeah I'll keep a bullet point in every repo just pointing the table of contents 

- [Disclaimer](#disclaimer)

- [Shit I've Built](#features)

- [Bragging](#other-stuff)

- [Legal Shit](#legal-stuff-and-licensing)

---

### Disclaimer

> The build system (build.sh) uses momenteraly sudo privs and **DO NOT** use it if you are not comfortable with that
-   For further implementation details about it check [Othet stuff](#other-stuff)

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
-   the 8080 accsessibilty stuff (PORT forwarding? IDK the name)

--- 

### Other stuff

> This part is where I brag about execution/compiling specifications

#### Developer&Local Server Test

Build locally and prepare the runtime files without sudo or systemd:

```bash
./build.sh local
./server
```

This compiles `./server`, copies the frontend into `$HOME/.server/frontend`, and
migrates `src/db/todo.db` into `$HOME/.server/db` if no installed database exists.
An existing database is preserved. If there is no source database, the server
creates one on its first run. Build dependencies must already be installed
(`make`, a C compiler, SQLite and OpenSSL development files, the `openssl` CLI,
and `sqlite3` for database migration).

or you can just manually move the frontend and db folders to $HOME/.server/
and use:
```bash
./generate-cert.sh # generate the local HTTPS certificate once
make #to build
make run # still unnecessary but to run (simply ./server)
make clean #to clean shit
```
manually


#### HTTPS

Open **https://localhost:8080** after starting the server. It now accepts HTTPS
only on port 8080, using OpenSSL with TLS 1.2 or newer. LAN URLs are printed on startup.

Both build modes generate a self-signed RSA certificate (valid for 365 days) in
`$HOME/.server/tls/cert.pem` with a private key in `$HOME/.server/tls/key.pem`.
Existing certificates are preserved. The certificate covers `localhost`,
`127.0.0.1`, and active IPv4 addresses discovered with `ip` at generation time.
For additional names or addresses, generate it before running the build:

```bash
./generate-cert.sh 'DNS:todo.local,IP:192.168.1.20'
```

Browsers will warn because this certificate is self-signed. Trust `cert.pem` on
client devices through their browser/OS certificate settings; never share
`key.pem`. To check HTTPS without disabling certificate verification:

```bash
curl --cacert "$HOME/.server/tls/cert.pem" https://localhost:8080/
openssl x509 -in "$HOME/.server/tls/cert.pem" -noout -fingerprint -sha256
```

If your LAN address changes or the certificate expires, move the old certificate
and key aside, rerun `./generate-cert.sh`, and restart the server. Clients will need
to trust the new certificate. You can also place a CA-issued PEM certificate chain
and matching unencrypted private key at those same paths, then restart.
Keep the TLS directory mode `700` and private key mode `600`. Service removal and
data deletion preserve the TLS directory so an uninstall does not silently change
this site's identity.

C

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
-   if firewalld is installed and active, opens TCP 8080 in the `public` zone
    in both runtime and permanent configuration, then verifies both rules
-   Will set the port up 8080 for local traffic (local build skips this) 

To use:

```bash

chmod +x build.sh #if not an executable already
./build.sh  #To build the server
./build.sh remove #To remove daemon and the service file (user db && frontend is presevered )
./build.sh delete #To delete db and frontend as well as unloading the daemon 

```

---


### Future Updates

- I am not promising those but I do say there is a non-zero chance of those happen to exist in this repo one day

- new error reporting using err (BSD version) to journal (still uses perror but journalctl handled)

- getting the device IP on runtime to make the server accessible to LAN instead of caging it only to localhost (Done but need tweaks)

- HTTPS with OpenSSL and local certificate generation (Done)

- And changed colors for highlighted c format markdown (left for the end user I am fine with the current colors)

---
### Legal Stuff and Licensing

This project is licensec under GPL-3.0 thing if you're a badge or law nerd the proper sources are:
[This Repo's License](LICENSE.md)
[Third Party License](THIRD_PARTY_LICENSES.md)

all the third party files placed under vendor subfolder with their respective license so... don't make me say those as if I am in a tux 
you got the idea basically don't steal shit, respect to the MD4C and go feral about the rest luv u <3 OwO

ertificate generation uses OpenSSL's documented
[`req -x509` and `-addext` options](https://docs.openssl.org/3.4/man1/openssl-req/).