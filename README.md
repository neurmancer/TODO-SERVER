# BASIC TODO-SERVER

### A Shitty Enterprise™ Product

A C server to keep track of your ideas. Started as a proof of concept,
now I'm writing deployment docs for a *FUCKING* CRUD app. I am not a corpo™.

SQLite, Markdown via MD4C, my own templating shit HTTPS login, and a jukebox
because apparently a todo list needs a soundtrack


## Table of Contents

- [Phoenix](#table-of-contents)
- [Don't get lost](#where-the-shit-lives)
- [Bragging](#bragging)
- [Compile?](#compile--run)
- [Licenses](#legal-stuff-and-licensing)


## Where the shit lives

- `Server/` — C backend, web frontend, build and deployment scripts. [Server Details](Server/README.md)
- `Client/` — Android WebView wrapper. [Build the APK](Client/README.md).
- `$HOME/.server/` — runtime database, frontend, credentials, and certificates.

## Bragging

- Well...we got cool stuff such as
    
    - Randomized music playing jukebox using direct HTTPS audio links (YouTube finally left the building _Long live Shitty JukebBox Lineage_)

    - Dope-ass login screen
    
    - markdown formatting for todos('cuz...yk it looks nice).
    
    - We got C as server...well at least started like that than the doctrine have become:
    'Whatever keeps the shit on the internet' than cloudflare, SSL and SQLite3 happened...
    
    - Lowkey since MD4C handles the markdown maybe juuuuust maybe I may go for my LaTeX parser? to add it too...I need to settle the score with MD4C lol

- Those were the selling points I guess..

## Compile & Run

Dependencies and setup bullshit: [Server](Server/README.md) / [Android](Client/README.md).
Start each side from the repo root.

### Server — the C shit

```bash
cd Server
./build.sh local
./server
```

Open **https://localhost:8080**. `Ctrl+C` stops it.
Or install/update the daemon from `Server/` as your regular user:

```bash
./build.sh install # Uses sudo, upgrades packages, installs the thing
./build.sh update  # Rebuild and restart after changing shit
```

### Android — yup...ja*a

```bash
cd Client
cp -n server.properties.example server.properties #if you used dwm before you know the concept of copying shit to another file right?
# Set serverUrl in server.properties to your deployed HTTPS domain
./gradlew assembleDebug lintDebug
```

Copy `app/build/outputs/apk/debug/app-debug.apk` to your phone and install.
Or, with USB debugging authorized, from `Client/`:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Open **Todo Server** and let the frontend do its fucking job.

### Legal Stuff and Licensing

This project is licensec under GPL-3.0 thing if you're a badge or law nerd the proper sources are:
[This Repo's License](LICENSE.md)
[Third Party License](THIRD_PARTY_LICENSES.md)

all the third party files placed under vendor subfolder with their respective license so... don't make me say those as if I am in a tux 
you got the idea basically don't steal closed-sourced shit, respect to the MD4C&Gradle and go feral about the rest. Luv u <3 OwO
