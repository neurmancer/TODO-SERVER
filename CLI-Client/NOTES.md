# CLI Client Implementation Notes


> This is where I'll try to bump C percentage of the repo up...

### Shit that I am planning to do:

- [ ] A basic text editor to edit todos

  Start with a growable text buffer, cursor position, scroll offset and a dirty
  flag. Keep the original notes around until Save actually succeeds. Network
  died? Keep the fucking draft. Esc can go back, but ask before nuking edits.

  Steal the web editor's behavior from `Server/frontend/editor.js`: four-column
  tab stops, Shift+Tab to unindent, Backspace to eat leading indentation and
  Enter to carry it onto the next line. Normal text still deletes normally.
  Add Home/End, Delete and page scrolling before getting fancy. Undo/redo should
  remember edits and cursor positions, not make me retype half a todo.

  UTF-8 bytes and terminal columns aren't the same shit. Keep buffer offsets
  separate from screen coordinates; don't backspace through half a character.
  An external `$EDITOR` could be an early escape hatch while this thing grows:
  private temp file, leave terminal mode, launch, read it back, restore the UI.

- [ ] Raw mode menu walk with arrow keys (or WASD or hjkl dunno yet...)

  First target: Linux/POSIX. I'll split terminal input/rendering from menu and
  editor logic (`terminal.c`, `ui.c`, `editor.c`) so changing keybinds doesn't
  require surgery on the whole fucking client...ncurses
  for less plumbing, or termios + escape sequences if suffering is the point.(_with me it kinda is tho..._)

  Arrows everywhere; hjkl/WASD can be menu aliases. In the editor those letters
  must actually type letters...obviously. Track selection and viewport
  separately so long lists scroll without losing the selected todo.

  With a handmade backend, decode complete escape sequences and give a lone
  Esc a short timeout. Handle terminal resize, redraw only what needs it, and
  restore terminal settings/cursor visibility on exit and handled signals.
  Signal handlers should flag work for the main loop. Don't leave the shell
  looking fucking possessed. Refuse interactive mode when stdin isn't a TTY.

- [ ] functioning CRUD continues...

  Keep the server as the owner of the database. A C HTTP layer using libcurl
  could handle HTTPS, form encoding and session cookies. No client poking at
  `.server` SQLite files and wondering why remote access doesn't work.

  Current server contract, straight from `Server/serverThingy.c` and
  `Server/src/handlers.c`:

  | Action | What exists right now |
  | --- | --- |
  | List / read | `GET /` and `GET /todos/<id>` return HTML |
  | Create | `POST /` with `todo=<title>`; starts notes with the same text |
  | Edit notes | `POST /update` with `id` and `content`; no title rename yet |
  | Mark done / pending | `POST /complete` with `id` and `completed=1` or `0` |
  | Delete | `POST /delete` with `id` |


  Login already uses `POST /login` with `username` and `password`, then a
  `__Host-todo_session` cookie. Keep it in memory initially; detect expired
  sessions without throwing away the draft. Verify HTTPS; local installs can
  use their generated `tls/cert.pem` as the trusted cert.

  `Server/src/request.h` currently caps request bodies at 8191 bytes. Check the
  ENCODED form size, including field names and ID, before saving; percent
  encoding makes shit bigger. Show failures, confirm deletion, and don't
  blindly retry a create after a timeout—it might already have landed.

- [ ] A basic version of the jukebox is possible for the CLI too

  The useful bits already exist: `GET /jukebox/songs` returns track titles and
  URLs; `/jukebox/audio/<track-id>` serves MP3s with byte-range support. Use the
  same authenticated connection setup as todos. No second music downloader or
  Drive integration needed in this client, thank fuck.

  MVP could hand playback to a separate player process while the C client owns
  the queue and controls. Pick a player with a control channel for pause/next
  later; don't block the input loop until the song ends. Download through the
  client's HTTP layer to a private cache first if passing authentication to the
  player becomes a pain. Keep session cookies out of process arguments.

  Each client gets its own shuffled queue. Play only when asked, handle an
  empty library, advance when a track ends, and stop/reap the player on exit.
  Audio decoding in C can be a later rabbit hole. Plenty of holes here already.

- [ ] If I loose my mind I can try to make CLI render markdowns not merely write them...

  MD4C is already vendored in `Server/vendor/md4c/`. Reuse its parser callbacks
  for a terminal renderer; `Server/src/markdown.c` currently produces HTML, so
  that output isn't the terminal preview. Start with headings, lists, quotes,
  emphasis and fenced code. Links can show their URL; images get alt text.
  Tables and syntax highlighting can wait until I've recovered mentally.


    not today tho...it's 12AM and I have school tomorrow
