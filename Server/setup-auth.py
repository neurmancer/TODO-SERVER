#!/usr/bin/env python3
"""Set the single-user login outside the repository; restart the backend afterward."""
import argparse
import getpass
import hashlib
import os
from pathlib import Path
import re
import secrets
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--user', default='todo', help='Login username (default: todo)')

args = parser.parse_args()

if not re.fullmatch(r'[A-Za-z0-9_-]{1,64}', args.user):
    parser.error('Username must contain 1–64 letters, digits, underscores, or hyphens.')

if os.geteuid() == 0:
    parser.error('Run as the regular user who runs server.service, without sudo.')

password = getpass.getpass('Choose a password (at least 12 characters): ')

if len(password) < 12 or len(password.encode()) > 1024:
    parser.error('Use at least 12 characters and at most 1024 UTF-8 bytes.')


if password != getpass.getpass('Confirm password: '):
    parser.error('Passwords did not match; nothing was changed.')

salt = secrets.token_bytes(32)

digest = hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 600000)

directory = Path.home() / '.server/auth'

if directory.is_symlink():
    parser.error('Refusing a symlinked auth directory.')

directory.mkdir(mode=0o700, parents=True, exist_ok=True)

directory.chmod(0o700)

fd, temporary = tempfile.mkstemp(prefix='.credentials-', dir=directory)

try:

    with os.fdopen(fd, 'w') as output:
        output.write(f'pbkdf2-sha256-600000\n{args.user}\n{salt.hex()}\n{digest.hex()}\n')
    os.replace(temporary, directory / 'credentials')

finally:    # Me after 3 weeks of coding shit increasingly drifting further from C 
    if os.path.exists(temporary):
        os.unlink(temporary)


print('Login configured. Run ./build.sh update (or restart server.service if already updated).')
