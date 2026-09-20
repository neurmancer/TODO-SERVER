#!/usr/bin/env python3
"""Cache MP3s from a public Google Drive folder; no account credentials required."""
import argparse
from collections import deque
from html.parser import HTMLParser
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from urllib.parse import urlencode, urlparse
from urllib.request import Request, urlopen

DEFAULT_FOLDER = '1vR_uOUoc2pt6UXissU6Z4Cns8Ema39Y1'

# Now fucking with OOP you should thank me for acting like fucking Napster
class DrivePage(HTMLParser):
    
    def __init__(self):
        super().__init__()
        self.links = []
        self.href = None
        self.text = []
        self.action = None
        self.fields = {}

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
    
        if tag == 'a':
            self.href = attrs.get('href', '')
            self.text = []
    
        if tag == 'form' and attrs.get('id') == 'download-form':
            self.action = attrs.get('action')
    
        if tag == 'input' and attrs.get('type') == 'hidden' and attrs.get('name'):
            self.fields[attrs['name']] = attrs.get('value', '')

    def handle_data(self, data):
        if self.href is not None:
            self.text.append(data)

    def handle_endtag(self, tag):
        if tag == 'a' and self.href is not None:
            self.links.append((self.href, ''.join(self.text).strip()))
            self.href = None


def request(url):
    return urlopen(Request(url, headers={'User-Agent': 'Mozilla/5.0'}), timeout=30)


def folder_id(value):
    if re.fullmatch(r'[\w-]{10,}', value, flags=re.ASCII):
        return value
    
    parsed = urlparse(value)
    match = re.search(r'/folders/([\w-]+)', parsed.path, flags=re.ASCII)
    
    if parsed.scheme == 'https' and parsed.hostname == 'drive.google.com' and match:
        return match[1]
    
    raise ValueError('Expected a Google Drive folder ID or HTTPS folder link')


def list_tracks(folder, root_tracks=None):
    pending, seen, tracks = deque([folder]), set(), {}
    
    while pending:
        current = pending.popleft()
        if current in seen:
            continue
        
        seen.add(current)
        
        if len(seen) > 128:
            raise ValueError('Folder tree exceeds 128 folders')
        
        with request('https://drive.google.com/embeddedfolderview?' + urlencode({'id': current})) as response:
        
            document = response.read(8 * 1024 * 1024 + 1)
        
            if len(document) > 8 * 1024 * 1024:
                raise ValueError('Folder listing is too large')
        
        page = DrivePage()
        page.feed(document.decode('utf-8'))
        
        # Fail explicitly if Google returns a login/error page instead of a listing.
        # One thing web-dev taught me: Act like everybody out there trying to kill you including the cute little pussy
        if 'flip-contents' not in document.decode('utf-8'):
            raise ValueError(f'Folder {current} is not publicly readable, or Drive changed its listing format')
        for url, name in page.links:
    
            parsed = urlparse(url)
            if parsed.scheme != 'https' or parsed.hostname != 'drive.google.com':
                continue
    
            child = re.fullmatch(r'/drive/(?:u/\d+/)?folders/([\w-]+)', parsed.path, flags=re.ASCII)
            file = re.fullmatch(r'/file/d/([\w-]+)/view', parsed.path, flags=re.ASCII)
    
            if child:
                pending.append(child[1])
            
            elif file and name.lower().endswith('.mp3'): # I hate using 'elif' btw...
                if current == folder:
                    if root_tracks is not None:
                        root_tracks.add(file[1])
                    continue
                tracks[file[1]] = name
    
                if len(tracks) > 4096:
                    raise ValueError('Library exceeds 4096 tracks')
    
    return sorted(tracks.items(), key=lambda track: (track[1].casefold(), track[0]))


def local_name(file_id, title):
    # Flatten folders; the Drive ID distinguishes duplicate titles never use a
    # remote path as a local path, and reserve space below NAME_MAX for the ID
    title = re.sub(r'[\x00-\x1f\x7f/\\]', '_', title[:-4]).strip(' .') or 'track'
    title = title.encode('utf-8')[:150].decode('utf-8', errors='ignore')
    
    return f'{title} [{file_id}].mp3'


def exclude_root_cache(output, root_ids):
    archive = None
    for path in output.iterdir():
        match = re.search(r' \[([\w-]+)\]\.mp3$', path.name, flags=re.ASCII)
        if not match or match[1] not in root_ids or path.is_symlink() or not path.is_file():
            continue
        if archive is None:
            archive = Path(tempfile.mkdtemp(prefix='.excluded-root-', dir=output))
        path.rename(archive / path.name)
        print(f'Excluded root-level track from playback: {path.name}')


def download(file_id, output, max_bytes):
    url = 'https://drive.google.com/uc?' + urlencode({'export': 'download', 'id': file_id})
    for attempt in range(2):
        with request(url) as response:
            if response.headers.get_content_type() == 'text/html':
                page = DrivePage()
                page.feed(response.read(1024 * 1024).decode('utf-8'))
                if attempt or page.action != 'https://drive.usercontent.google.com/download':
                    raise ValueError('Drive returned a confirmation/error page instead of audio')
                fields = {key: value for key, value in page.fields.items() if key in ('id', 'export', 'confirm', 'uuid')}
                fields.update(id=file_id, export='download')
                url = page.action + '?' + urlencode(fields)
                continue
            total = 0
            with output.open('wb') as target:
                while chunk := response.read(128 * 1024):
                    total += len(chunk)
                    if total > max_bytes:
                        raise ValueError('Track exceeds the configured download size limit')
                    target.write(chunk)
            if not total:
                raise ValueError('Drive returned an empty file')
            return
    raise ValueError('Could not download track')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', nargs='?', default=os.environ.get('TODO_MUSIC_FOLDER') or DEFAULT_FOLDER,
                        help='Public Drive folder link or ID (default: shared Cyberspace library; override with TODO_MUSIC_FOLDER)')
    parser.add_argument('--output', type=Path, default=Path.home() / '.server/music')
    parser.add_argument('--list', action='store_true', help='List MP3s without downloading')
    parser.add_argument('--limit', type=int, help='Import only the first N tracks (for testing)')
    parser.add_argument('--max-mib', type=int, default=256, help='Maximum size per track (default: 256 MiB)')
    args = parser.parse_args()
    if args.max_mib < 1 or (args.limit is not None and args.limit < 1):
        parser.error('Size and track limits must be positive')
    if not args.list and not shutil.which('ffprobe'):
        parser.error('Install ffprobe (part of FFmpeg) to validate downloaded MP3s')
    root_tracks = set()
    tracks = list_tracks(folder_id(args.folder), root_tracks=root_tracks)
    excluded_ids = root_tracks - {file_id for file_id, _ in tracks}
    print(f'Found {len(tracks)} MP3 files in subfolders; root-level MP3s and other files are ignored.')
    if args.limit:
        tracks = tracks[:args.limit]
    if args.list:
        for file_id, title in tracks:
            print(f'{file_id}\t{title}')
        return
    args.output.mkdir(parents=True, exist_ok=True)
    # Archive earlier root-level imports so a sync also fixes existing libraries.
    exclude_root_cache(args.output, excluded_ids)
    failures = 0
    for file_id, title in tracks:
        destination = args.output / local_name(file_id, title)
        if destination.is_file() and not destination.is_symlink() and destination.stat().st_size:
            print(f'Already cached: {title}')
            continue
        try:
            # The server ignores this hidden staging directory. Publish atomically
            # only after download and format validation succeed.
            with tempfile.TemporaryDirectory(prefix='.import-', dir=args.output) as staging:
                partial = Path(staging) / 'track.mp3'
                download(file_id, partial, args.max_mib * 1024 * 1024)
                result = subprocess.run(['ffprobe', '-v', 'error', '-select_streams', 'a:0',
                    '-show_entries', 'stream=codec_name', '-of', 'json', str(partial)],
                    capture_output=True, text=True, timeout=30, check=True)
                streams = json.loads(result.stdout).get('streams', [])
                if not streams or streams[0].get('codec_name') != 'mp3':
                    raise ValueError('Downloaded content is not MP3 audio')
                partial.replace(destination)
            print(f'Imported: {title}', flush=True)
        except Exception as error:
            failures += 1
            print(f'Failed: {title}: {error}', flush=True)
    if failures:
        raise SystemExit(f'{failures} track(s) failed; rerun to retry. Existing files were preserved.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError) as error:
        raise SystemExit(str(error))
