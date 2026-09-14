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

- [Usage](#usage)

- [Legal Shit](#legal-stuff-and-licensing)

---

### Disclaimer

> The build system (build.sh) uses momenteraly sudo privs and **DO NOT** use it if you are not comfortable with that
    - For further implementation details about it check [Othet stuff](#other-stuff)

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

--- 

### Other stuff

> This part is where I brag about execution/compiling specifications

#### Developer&Local Server Test

- If you want to use the server as a test server and tweak it according to your taste I highly **recommend** you to use the given Makefile
- But local compiling requries changes in the source code itself because the current status of the soruce is build.sh competible and seeks
the designed .server subfolder in $HOME instead of the local files you can put the folders there manually if you don't want the daemoization
with : 
```bash

cd ~  && mkdir .server

cp frontend  ~/.server && cp db  ~/.server # assuming you're in the TODO-SERVER folder

```

```bash

make    #compile the shit
make clean  #clean the mess
make run    # I mean that is pointless since you can just do ./server but I've added it already

```


#### Full Build 

**Important Shit**

build.sh will use **sudo** privs for update, package installation, and moving server and server.service files to the said folders 
If you haven't checked the code yourself or you don't trust the author (me) **DO NOT ALLOW**

---
- Full build file is not a mere build file the file does various tasks such as:

-   Update system packages (on Arch and Debian/Ubuntu)
-   Install dependencies
-   Compiles the server
-   creates server.service and moves it into /etc/systemd/system/
-   moves the server into /usr/local/bin/
-   cleans the residue of the build before leaving

To use:

```bash

chmod +x build.sh #if not an executable already
./build.sh  #To build the server
./build.sh remove #To remove daemon and the service file (user db && frontend is presevered )
./build.sh deletye #To delete db and frontend as well as unloading the daemon 

```

---


### Legal Stuff and Licensing

This project is licensec under GPL-3.0 thing if you're a badge or law nerd the proper sources are:
[This Repo's License](LICENSE.md)
[Third Party License](THIRD_PARTY_LICENSES.md)

all the third party usage placed under vendor subfolder with their respective license so... don't make me say those as if I am in a tux 
you got the idea <3