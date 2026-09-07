# TODO-SERVER

> A basic todo-server to keep track of your idaes (local network)

> This project started as a proof of concept then started to get **bigger**
and now I need a separate repo to keep developing it 
> And yeah I am getting more and more corpo with building a *FUCKING* CRUD App (but I swear I won't put a tie trust me...it doesn't work yet either...so I can't say 'I built one' (yeah I am coping with that (and yeah I am using parens within parens again(_you can do nothing about it_(except complaining about how your eyes are bleeding)))))

## Table of Contents 

- [I'll add shit her eventually but rn just scroll](#table-of-contents)
> and yeah I'll keep a bullet point in every repo just pointing the table of contents 


### DEV BRANCH MISSONS 

> This is where I'll study networking on the fly while trying to make this shit better
> Basically I copied the current main branch version from my other repo (Basic-C-Examples) where this started as a proof of concept project and I wanna focus on this for a while (Well that's a lie at this point I modularized it (or how tf u spell it))

> Project will contain lots of comments across each file since I'll left my learning process visible (I mean this is the dev branch duh)

### Possible Future paths for this project

- [X] A modular structure (well...kinda started)
- [X] A basic focused-on-project web framework (to make my job easier not entrepriese level shit)
- [ ] A basic templating engine to use .html templates instead of what I am doing rn (Shit I'll be working on)
- [ ] Database implementation (and shit I'll be experimentign on)


### Folders and files in the repo

- main server file
- a makefile with compile-time conf options
- src subfolder for QoL update (Yeah I am thinking my little web router as a basic minecraft mod)
- frontend folder (which only includes index.html for now)

- template.c and template.h added but haven't used nor added to Makefile

### Compile Thing

```bash

make # To build
make clean # To clean-up the mess
make MAIN=toChanTheSource.c TARGET=alsoTheOutput

make rebuild
make run    #Honestly...does anybody types that instead of ./output? anyways I added already it's 4 AM
```