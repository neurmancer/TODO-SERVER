#!/usr/bin/env python3

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

with tempfile.TemporaryDirectory(prefix='todo-build-music-test-') as tmp:

    work = Path(tmp)
    project, home, bin_dir = work / 'project', work / 'home', work / 'bin'

    for directory in (project, home, bin_dir, project / 'frontend'):
        directory.mkdir(parents=True, exist_ok=True)

    shutil.copy(ROOT / 'build.sh', project / 'build.sh')
    (project / 'Makefile').touch()

    for name in ('index.html', 'template.html'):
        (project / 'frontend' / name).write_text('new frontend')
    (project / 'generate-cert.sh').write_text('echo cert >> "$TEST_LOG"\n')

    commands = {
        'id': 'if [ "$1" = -u ]; then echo 1000; else echo test-user; fi',
        'ffprobe': 'exit 0',
        'python3': 'echo "import $*" >> "$TEST_LOG"; exit "${TEST_SYNC_STATUS:-0}"',
        'make': 'echo "make $*" >> "$TEST_LOG"; touch server; chmod +x server',
        'sudo': 'echo unexpected-sudo >> "$TEST_LOG"; exit 99',
        'systemctl': 'echo unexpected-systemctl >> "$TEST_LOG"; exit 99',
    }

    for name, script in commands.items():
        executable = bin_dir / name
        executable.write_text('#!/bin/sh\n' + script + '\n')
        executable.chmod(0o755)

    log = work / 'calls'
    env = {**os.environ, 'HOME': str(home), 'PATH': f'{bin_dir}:' + os.environ['PATH'], 'TEST_LOG': str(log)}

    def run(action, status=0):
        log.write_text('')
        result = subprocess.run(['bash', str(project / 'build.sh'), action],
                                env={**env, 'TEST_SYNC_STATUS': str(status)}, capture_output=True, text=True)
        return result, log.read_text().splitlines()

    result, calls = run('sync-music')

    assert result.returncode == 0, result.stderr
    assert calls == [f'import scripts/import-music.py --output {home}/.server/music'], calls

    result, calls = run('local', 1)
    assert result.returncode != 0 and 'Music sync failed' in result.stderr
    assert not (work / '.server/frontend').exists(), 'Failed sync must stop before replacing runtime files'

    result, calls = run('local')

    assert result.returncode == 0, result.stderr
    assert sum(line.startswith('import ') for line in calls) == 1
    assert (work / '.server/frontend/index.html').read_text() == 'new frontend'
    assert f'import scripts/import-music.py --output {work}/.server/music' in calls
    assert f'TODO_SERVER_PATH={work}/.server' in result.stdout
    assert not (home / '.server/frontend').exists(), 'Local build must not change installed runtime'
    assert not any('unexpected-' in line for line in calls), calls

    installed = home / '.server'
    for root in (installed, work / '.server'):
        (root / 'music/.archive').mkdir(parents=True, exist_ok=True)
        (root / 'music/track.mp3').write_text('song')
        (root / 'music/.archive/old.mp3').write_text('old song')
        (root / 'db').mkdir(exist_ok=True)
        (root / 'db/todo.db').write_text('keep database')
        (root / 'tls').mkdir(exist_ok=True)
        (root / 'tls/key.pem').write_text('keep certificate')

    result, calls = run('nuke-songs')
    assert result.returncode == 0, result.stderr
    assert not calls, 'Nuke must not import, compile, or use sudo'
    assert not (installed / 'music').exists()
    assert (work / '.server/music/track.mp3').exists(), 'Default nuke must preserve local music'
    assert (installed / 'db/todo.db').read_text() == 'keep database'
    assert (installed / 'tls/key.pem').read_text() == 'keep certificate'
    result, calls = run('nuke-songs')
    assert result.returncode == 0, 'Nuking an absent cache is harmless'

    (installed / 'music').symlink_to(work / '.server/music', target_is_directory=True)
    result, calls = run('nuke-songs')
    assert result.returncode != 0 and 'symlink' in result.stderr
    assert (work / '.server/music/track.mp3').exists()

    result = subprocess.run(['bash', str(project / 'build.sh'), 'nuke-songs', '--local'],
                            env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert not (work / '.server/music').exists()
    assert (work / '.server/db/todo.db').read_text() == 'keep database'
    assert (work / '.server/frontend/index.html').read_text() == 'new frontend'

    log.write_text('')
    result = subprocess.run(['bash', str(project / 'build.sh'), 'sync-music', '--local'],
                            env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    assert log.read_text().strip() == f'import scripts/import-music.py --output {work}/.server/music'

    print('Passed: installed/local music paths, sync without sudo/rebuild, failed-sync isolation, runtime installation, scoped nuke and symlink refusal')
