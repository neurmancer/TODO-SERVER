#!/usr/bin/env python3
"""Add users or reset passwords in the runtime user database; restart after changes."""
import argparse
import getpass
import hashlib
import os
from pathlib import Path
import re
import secrets
import sqlite3
import stat


def configure(args, password=None):
    """Preserve the original account as ID 1 and never replace another user's data."""
    directory = Path(args.runtime).expanduser() / 'auth'
    if directory.is_symlink():
        raise ValueError('Refusing a symlinked auth directory.')
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    directory.chmod(0o700)
    database = directory / 'users.db'
    legacy = directory / 'credentials'
    for path in (database, legacy):
        if path.exists() or path.is_symlink():
            info = path.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid() or info.st_mode & 0o077:
                raise ValueError(f'{path} must be a private regular file owned by you (mode 600).')
    if args.list and not database.exists() and not legacy.exists():
        return
    old_mask = os.umask(0o077)
    try:
        db = sqlite3.connect(database)
    finally:
        os.umask(old_mask)
    try:
        with db:
            db.execute('CREATE TABLE IF NOT EXISTS users ('
                       'id INTEGER PRIMARY KEY AUTOINCREMENT, '
                       'username TEXT NOT NULL UNIQUE, '
                       'salt BLOB NOT NULL CHECK(length(salt)=32), '
                       'password_hash BLOB NOT NULL CHECK(length(password_hash)=32))')
            if not db.execute('SELECT 1 FROM users LIMIT 1').fetchone() and legacy.exists():
                fields = legacy.read_text().split()
                if len(fields) != 4 or fields[0] != 'pbkdf2-sha256-600000':
                    raise ValueError('Invalid legacy credentials; nothing was imported.')
                _, user, salt, digest = fields
                db.execute('INSERT INTO users(id,username,salt,password_hash) VALUES(1,?,?,?)',
                           (user, bytes.fromhex(salt), bytes.fromhex(digest)))
            if args.list:
                for uid, username in db.execute('SELECT id,username FROM users ORDER BY id'):
                    print(f'{uid}\t{username}')
                return
            if args.rename:
                if db.execute('UPDATE users SET username=? WHERE username=?',
                              (args.rename, args.user)).rowcount != 1:
                    raise ValueError('User does not exist.')
            else:
                salt = secrets.token_bytes(32)
                digest = hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 600000)
                db.execute('INSERT INTO users(username,salt,password_hash) VALUES(?,?,?) '
                           'ON CONFLICT(username) DO UPDATE SET salt=excluded.salt, password_hash=excluded.password_hash',
                           (args.user, salt, digest))
    finally:
        db.close()
    print(f'User saved in {database}. Restart the backend to clear existing sessions.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--user', default='todo', help='Username to add or update (default: todo)')
    parser.add_argument('--runtime', default=os.environ.get('TODO_SERVER_PATH', str(Path.home() / '.server')),
                        help='Runtime directory (default: TODO_SERVER_PATH or ~/.server)')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--list', action='store_true', help='List user IDs and usernames')
    mode.add_argument('--rename', metavar='NEW_NAME', help='Rename --user, preserving todos and password')
    args = parser.parse_args()
    for name in (args.user, args.rename):
        if name is not None and not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', name):
            parser.error('Username must contain 1–64 letters, digits, underscores, or hyphens.')
    if os.geteuid() == 0:
        parser.error('Run as the regular user who runs server.service, without sudo.')
    password = None
    if not args.list and not args.rename:
        password = getpass.getpass('Choose a password (at least 12 characters): ')
        if len(password) < 12 or len(password.encode()) > 1024:
            parser.error('Use at least 12 characters and at most 1024 UTF-8 bytes.')
        if password != getpass.getpass('Confirm password: '):
            parser.error('Passwords did not match; nothing was changed.')
    try:
        configure(args, password)
    except (ValueError, OSError, sqlite3.Error) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
