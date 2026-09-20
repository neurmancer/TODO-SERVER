#!/usr/bin/env python3
"""Local integration checks; run with a C compiler and Caddy 2.8+ on PATH.

Uses temporary certificates/data and loopback ports 18080 and 18443.
Does not install services or request a public certificate.
"""
#Well we're not in C I don't need to implement shit myself it's me in C territory

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
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
CADDY = shutil.which('caddy')
assert CADDY, 'Install Caddy or add its binary directory to PATH'


bad_args = [[], ['--domain'], *[['--domain', x] for x in (
    'https://todo.example.com', 'a..com', 'a.com\n{', '127.0.0.1',
    '-a.com', 'a.com.', 'a.com:443', 'a.com/path', 'a' * 64 + '.com')],
    ['--domain', 'a.com', '--user', 'a b'],
    ['--domain', 'a.com', '--domain', 'b.com']]

for args in bad_args:
    result = subprocess.run([str(ROOT / 'deploy.sh'), *args], capture_output=True, text=True)
    assert result.returncode != 0 and 'Missing dependency' not in result.stderr, args

print(f'{len(bad_args)} invalid CLI cases rejected') # Yeah how C of you to use 'print'

with tempfile.TemporaryDirectory(prefix='todo-deployment-test-') as tmp:
    work = Path(tmp)
    env = dict(os.environ, HOME=tmp, XDG_DATA_HOME=str(work / 'data'),
               XDG_CONFIG_HOME=str(work / 'config'))
    binary = work / 'server'
    sources = [ROOT / 'serverThingy.c', *sorted((ROOT / 'src').glob('*.c')),
               *sorted((ROOT / 'vendor/md4c').glob('*.c'))]
    subprocess.run(['cc', '-Wall', '-Wextra', '-DPORT=18080', *map(str, sources),
                    '-o', str(binary), '-lsqlite3', '-lssl', '-lcrypto'], check=True)
    tls = work / '.server/tls'
    tls.mkdir(parents=True)
    (work / '.server/db').mkdir()
    (work / '.server/music').mkdir()
    (work / '.server/music/proxy-test.mp3').write_bytes(b'ID3-proxy-audio-fixture')
    shutil.copytree(ROOT / 'frontend', work / '.server/frontend')
    subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:2048', '-nodes',
        '-days', '1', '-subj', '/CN=localhost', '-addext',
        'subjectAltName=DNS:localhost,IP:127.0.0.1',
        '-addext', 'basicConstraints=critical,CA:FALSE',
        '-addext', 'keyUsage=critical,digitalSignature,keyEncipherment',
        '-addext', 'extendedKeyUsage=serverAuth',
        '-keyout', str(tls / 'key.pem'), '-out', str(tls / 'cert.pem')],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    auth_dir = work / '.server/auth'
    auth_dir.mkdir(mode=0o700)
    
    credentials = auth_dir / 'credentials'
    
    salt = os.urandom(32)
    
    digest = hashlib.pbkdf2_hmac('sha256', b'integration-test-password', salt, 600000)
    
    credentials.write_text(f'pbkdf2-sha256-600000\ntodo\n{salt.hex()}\n{digest.hex()}\n')
    
    credentials.chmod(0o600)
    
    template = (ROOT / 'deploy.sh').read_text().split('cat > "$STAGING/Caddyfile" <<EOF\n')[1].split('\nEOF')[0]
    
    config = template.replace('$DOMAIN', 'todo.example.com').replace('$LOGIN_USER', 'todo')
    
    config = config.replace(
        '/etc/todo-caddy/backend-cert.pem', str(tls / 'cert.pem'))
    
    config = config.replace('127.0.0.1:8080', '127.0.0.1:18080')
    
    config_path = work / 'Caddyfile'
    config_path.write_text(config)
    
    subprocess.run([CADDY, 'validate', '--config', str(config_path), '--adapter', 'caddyfile'],
        env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    
    config = config.replace('todo.example.com {', 'localhost:18443 {\n    bind 127.0.0.1\n    tls internal')
    config = config.replace('    admin off', '    admin off\n    skip_install_trust\n    auto_https disable_redirects')
    
    config_path.write_text(config)
    processes = []
    with (work / 'services.log').open('w+') as log:
        try:
            for args in ([str(binary)],
                         [CADDY, 'run', '--config', str(config_path), '--adapter', 'caddyfile']):
                processes.append(subprocess.Popen(args, env=env, stdout=log, stderr=log))
            for port in (18080, 18443):
                for _ in range(100):
                    assert all(p.poll() is None for p in processes), 'Service exited early'
                    try:
                        with socket.create_connection(('127.0.0.1', port), timeout=.1):
                            break
                    except OSError:
                        time.sleep(.1)
                else:
                    raise AssertionError(f'Port {port} did not open')
            # Linux bind table confirms no wildcard/LAN backend listener.
            rows = Path('/proc/net/tcp').read_text().splitlines()[1:]
            listeners = [r.split()[1] for r in rows if r.split()[3] == '0A'
                         and r.split()[1].endswith(':46A0')]
            assert listeners == ['0100007F:46A0'], listeners
            for method in ('GET', 'HEAD'):
                connection = http.client.HTTPConnection('127.0.0.1', 18080, timeout=5)
                connection.request(method, '/', headers={'Host': 'untrusted.example'})
                response = connection.getresponse()
                assert response.status == 308
                assert response.getheader('Location') == 'https://localhost:18080/'
                assert response.read() == b''
                connection.close()
            root_cert = work / 'data/caddy/pki/authorities/local/root.crt'
            for _ in range(100):
                if root_cert.exists():
                    break
                time.sleep(.1)
            context = ssl.create_default_context(cafile=str(root_cert))
            def request(path, cookie=None, data=None, origin='https://localhost:18443'):
                headers = {'Origin': origin}
                if cookie is not None:
                    headers['Cookie'] = cookie
                if data is not None:
                    headers['Content-Type'] = 'application/x-www-form-urlencoded'
                connection = http.client.HTTPSConnection('localhost', 18443, context=context, timeout=5)
                connection.request('POST' if data is not None else 'GET', path, body=data, headers=headers)
                response = connection.getresponse()
                result = response.status, response.read(), dict(response.getheaders())
                connection.close()
                return result
            assert request('/')[0] == 303
            assert request('/')[2]['Location'] == '/login'
            assert b'Enter cyberspace' in request('/login')[1]
            assert request('/login.js')[0] == 200
            assert request('/style.css')[0] == 200
            assert request('/delete', data=b'id=1')[0] == 401
            assert request('/todos/1')[0] == 303
            assert request('/jukebox/songs')[0] == 303
            assert request('/login', data=b'username=todo&password=wrong')[0] == 401
            login_data = b'username=todo&password=integration-test-password'
            assert request('/login', data=login_data, origin='https://evil.example')[0] == 403
            assert request('/login', data=login_data + b'&username=todo')[0] == 401
            response = request('/login', data=login_data)
            assert response[0] == 200, response
            cookie_header = response[2]['Set-Cookie']
            for flag in ('HttpOnly', 'Secure', 'SameSite=Strict', 'Path=/', 'Max-Age=28800'):
                assert flag in cookie_header
            cookie = cookie_header.split(';')[0]
            assert request('/', cookie)[0] == 200
            audio_url = json.loads(request('/jukebox/songs', cookie)[1])[0]['url']
            connection = http.client.HTTPSConnection('localhost', 18443, context=context, timeout=5)
            connection.request('GET', audio_url, headers={'Cookie': cookie, 'Range': 'bytes=0-2'})
            audio_response = connection.getresponse()
            assert audio_response.status == 206 and audio_response.read() == b'ID3'
            assert audio_response.getheader('Content-Range') == 'bytes 0-2/23'
            connection.close()
            assert request('/', cookie + 'bad')[0] == 303
            assert request('/', cookie, data=b'todo=blocked', origin='https://evil.example')[0] == 403
            assert request('/', cookie, b'todo=DeploymentCheck')[0] == 303
            assert b'DeploymentCheck' in request('/', cookie)[1]
            assert request('/logout', cookie, b'')[0] == 303
            assert request('/', cookie)[0] == 303
            # Re-login rotates the session; the old token no longer works.
            cookie = request('/login', data=login_data)[2]['Set-Cookie'].split(';')[0]
            next_cookie = request('/login', cookie, login_data)[2]['Set-Cookie'].split(';')[0]
            assert cookie != next_cookie and request('/', cookie)[0] == 303
            assert request('/', next_cookie)[0] == 200
            for _ in range(5):
                assert request('/login', data=b'username=todo&password=wrong')[0] == 401
            assert request('/login', data=login_data)[0] == 429
            # Server restart invalidates sessions; malformed credentials fail closed.
            processes[0].terminate(); processes[0].wait(timeout=10)
            credentials.chmod(0o644)
            result = subprocess.run([str(binary)], env=env, capture_output=True, timeout=5)
            assert result.returncode != 0
            credentials.chmod(0o600)
            processes[0] = subprocess.Popen([str(binary)], env=env, stdout=log, stderr=log)
            time.sleep(.3)
            assert request('/', next_cookie)[0] == 303
            # No credentials preserves the pre-existing local-only workflow.
            processes[0].terminate(); processes[0].wait(timeout=10)
            credentials.unlink()
            processes[0] = subprocess.Popen([str(binary)], env=env, stdout=log, stderr=log)
            time.sleep(.3)
            assert request('/')[0] == 200
            assert request('/login', data=login_data)[0] == 503
            print('Passed: CLI validation, verified HTTPS, redirects, login assets, cookies, '
                  'route protection, CSRF rejection, session rotation/logout/restart, '
                  'rate limiting, private credential permissions, local mode, and todo creation')

        except Exception:
            log.flush()
            log.seek(0)
            print(log.read())
            raise
        
        finally:
            for process in reversed(processes):
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
