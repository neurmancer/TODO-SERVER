#!/usr/bin/env python3
"""Exercise authenticated MP3 delivery and fork lifecycle on an isolated server."""
import hashlib
import http.client
import json
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
PORT = 18081

with tempfile.TemporaryDirectory(prefix='todo-jukebox-test-') as tmp:
    work = Path(tmp)
    runtime = work / '.server'
    for name in ('tls', 'db', 'auth', 'music'):
        (runtime / name).mkdir(parents=True)
    shutil.copytree(ROOT / 'frontend', runtime / 'frontend')
    salt = os.urandom(32)
    digest = hashlib.pbkdf2_hmac('sha256', b'jukebox-test-password', salt, 600000)
    credentials = runtime / 'auth/credentials'
    credentials.write_text(f'pbkdf2-sha256-600000\ntodo\n{salt.hex()}\n{digest.hex()}\n')
    credentials.chmod(0o600)
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes', '-days', '1',
        '-subj', '/CN=localhost', '-keyout', str(runtime / 'tls/key.pem'),
        '-out', str(runtime / 'tls/cert.pem')], check=True, capture_output=True)
    sources = [ROOT / 'serverThingy.c', *sorted((ROOT / 'src').glob('*.c')),
               *sorted((ROOT / 'vendor/md4c').glob('*.c'))]
    binary = work / 'server'
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', f'-DPORT={PORT}', *map(str, sources),
        '-o', str(binary), '-lsqlite3', '-lssl', '-lcrypto'], check=True)
    context = ssl._create_unverified_context()  # Only the ephemeral loopback test certificate.
    cookie = None

    def connect():
        return http.client.HTTPSConnection('127.0.0.1', PORT, context=context, timeout=3)

    def request(path, method='GET', headers=None, body=None, authenticated=True):
        fields = dict(headers or {})
        if authenticated and cookie:
            fields['Cookie'] = cookie
        conn = connect()
        conn.request(method, path, headers=fields, body=body)
        response = conn.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        conn.close()
        return result

    def children():
        return [int(pid) for pid in Path(f'/proc/{server.pid}/task/{server.pid}/children').read_text().split()]

    with (work / 'server.log').open('w+') as log:
        server = subprocess.Popen([str(binary)], env={**os.environ, 'HOME': tmp}, stdout=log, stderr=log)
        held = []
        try:
            for _ in range(100):
                assert server.poll() is None, 'Server exited'
                try:
                    with socket.create_connection(('127.0.0.1', PORT), timeout=.1):
                        break
                except OSError:
                    time.sleep(.05)
            assert request('/jukebox/songs', authenticated=False)[0] == 303
            result = request('/login', 'POST', {'Origin': f'https://127.0.0.1:{PORT}'},
                             b'username=todo&password=jukebox-test-password')
            assert result[0] == 200, result
            cookie = result[1]['Set-Cookie'].split(';')[0]
            assert json.loads(request('/jukebox/songs')[2]) == []
            payload = b'ID3' + bytes(range(256)) * 32
            name = 'A "quoted" café song.MP3'
            (runtime / 'music' / name).write_bytes(payload)
            (runtime / 'music/ignored.txt').write_text('private non-music data')
            (runtime / 'music/empty.mp3').touch()
            (runtime / 'music/leak.mp3').symlink_to(credentials)
            os.mkfifo(runtime / 'music/pipe.mp3')
            tracks = json.loads(request('/jukebox/songs')[2])
            assert len(tracks) == 1 and tracks[0]['title'] == name, tracks
            url = tracks[0]['url']
            assert request(url, authenticated=False)[0] == 303
            result = request(url)
            assert result[0] == 200 and result[2] == payload, result[:2]
            assert result[1]['Content-Type'] == 'audio/mpeg'
            assert result[1]['Accept-Ranges'] == 'bytes'
            result = request(url, 'HEAD', {'Range': 'bytes=2-3'})
            assert result[0] == 200 and result[2] == b'' and int(result[1]['Content-Length']) == len(payload)
            for value, start, end in [('bytes=0-1', 0, 1), ('bytes=100-', 100, len(payload)-1),
                                      ('bytes=-16', len(payload)-16, len(payload)-1),
                                      ('bytes=100-999999', 100, len(payload)-1)]:
                code, headers, body = request(url, headers={'Range': value})
                assert code == 206 and body == payload[start:end+1], (value, code)
                assert headers['Content-Range'] == f'bytes {start}-{end}/{len(payload)}'
            for value in ['bytes=999999-', 'bytes=-0']:
                code, headers, body = request(url, headers={'Range': value})
                assert code == 416 and not body
                assert headers['Content-Range'] == f'bytes */{len(payload)}'
            for value in ['bytes=7-3', 'bytes=0-1,5-6', 'bytes=oops', 'bytes=184467440737095516160-', 'items=0-1']:
                result = request(url, headers={'Range': value})
                assert result[0] == 200 and result[2] == payload
            assert request(url, headers={'Range': 'bytes=1-2', 'If-Range': '"old-version"'})[0] == 200
            for path in ['/jukebox/audio/../../auth/credentials', '/jukebox/audio/%2e%2e',
                         '/jukebox/audio/' + 'f' * 64]:
                assert request(path)[0] == 404
            (runtime / 'music' / name).unlink()
            assert request(url)[0] == 404
            print('Passed: authentication, empty library, filename escaping, filtering, HEAD, byte ranges, traversal and removed files')

            # Each connection stops reading a file larger than the socket buffers.
            # Its worker blocks, while parent HTTP/database requests must still work.
            with (runtime / 'music/large.mp3').open('wb') as out:
                out.truncate(64 * 1024 * 1024)
            url = json.loads(request('/jukebox/songs')[2])[0]['url']
            for _ in range(8):
                conn = connect()
                conn.connect()
                conn.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096)
                conn.request('GET', url, headers={'Cookie': cookie})
                response = conn.getresponse()
                assert response.status == 200
                held.append((conn, response))
            started = time.monotonic()
            assert request('/')[0] == 200
            assert time.monotonic() - started < 2
            assert request(url)[0] == 503, 'Worker limit must bound slow connections'
            for conn, response in held:
                response.close()
                conn.close()
            held.clear()
            for _ in range(80):
                if not children():
                    break
                time.sleep(.1)
            assert not children(), 'Disconnected workers must be reaped even while idle'
            assert request(url, headers={'Range': 'bytes=0-1'})[2] == b'\0\0'
            print('Passed: simultaneous listeners, responsive parent, worker limit, disconnect cleanup and recovery')

            # Session changes happen in the parent, so logout gates later audio requests.
            assert request('/logout', 'POST', {'Origin': f'https://127.0.0.1:{PORT}'}, b'')[0] == 303
            assert request(url)[0] == 303
            result = request('/login', 'POST', {'Origin': f'https://127.0.0.1:{PORT}'},
                             b'username=todo&password=jukebox-test-password')
            cookie = result[1]['Set-Cookie'].split(';')[0]
            conn = connect()
            conn.request('GET', url, headers={'Cookie': cookie})
            response = conn.getresponse()
            held.append((conn, response))
            pids = children()
            assert pids
            server.terminate()
            server.wait(timeout=5)
            assert all(not Path(f'/proc/{pid}').exists() for pid in pids)
            print('Passed: logout protection and shutdown worker cleanup')
        except Exception:
            log.flush()
            log.seek(0)
            print(log.read())
            raise
        finally:
            for conn, response in held:
                response.close()
                conn.close()
            if server.poll() is None:
                server.terminate()
                server.wait(timeout=5)
