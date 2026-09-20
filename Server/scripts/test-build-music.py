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
    assert not (home / '.server/frontend').exists(), 'Failed sync must stop before replacing runtime files'

    result, calls = run('local')

    assert result.returncode == 0, result.stderr
    assert sum(line.startswith('import ') for line in calls) == 1
    assert (home / '.server/frontend/index.html').read_text() == 'new frontend'
    assert not any('unexpected-' in line for line in calls), calls

    print('Passed: sync-only without sudo/rebuild, automatic local-build sync, fail-before-runtime-update, runtime installation')
