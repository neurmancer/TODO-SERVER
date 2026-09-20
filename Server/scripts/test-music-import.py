#!/usr/bin/env python3
"""Check public-folder filtering and safe cache download handling without network."""
from email.message import Message
import importlib.util
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('music_import', Path(__file__).with_name('import-music.py'))
music = importlib.util.module_from_spec(spec)
spec.loader.exec_module(music)


class Response(io.BytesIO):
    def __init__(self, data, content_type='text/html'):
        super().__init__(data)
        self.headers = Message()
        self.headers['Content-Type'] = content_type


class ImportTests(unittest.TestCase):
    def test_default_library_additions_and_failed_sync_preserve_cache(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)

            def download(file_id, target, max_bytes):
                target.write_bytes(b'ID3-' + file_id.encode())
                if file_id == 'broken':
                    raise OSError('connection interrupted')

            with patch('sys.argv', ['import-music.py', '--output', tmp]), \
                 patch.dict(music.os.environ, {'TODO_MUSIC_FOLDER': ''}), \
                 patch.object(music.shutil, 'which', return_value='/usr/bin/ffprobe'), \
                 patch.object(music, 'list_tracks') as listing, \
                 patch.object(music, 'download', side_effect=download) as fetch, \
                 patch.object(music.subprocess, 'run', return_value=SimpleNamespace(stdout='{"streams":[{"codec_name":"mp3"}]}')), \
                 patch('sys.stdout', new_callable=io.StringIO):
                listing.return_value = [('old', 'Old.mp3')]
                music.main()
                listing.assert_called_with(music.DEFAULT_FOLDER, root_tracks=set())
                old = output / music.local_name('old', 'Old.mp3')
                original = old.read_bytes(), old.stat().st_mtime_ns
                listing.return_value.append(('new', 'New.mp3'))
                music.main()
                music.main()
                self.assertEqual([call.args[0] for call in fetch.call_args_list], ['old', 'new'])
                self.assertEqual((old.read_bytes(), old.stat().st_mtime_ns), original)
                listing.return_value.append(('broken', 'Broken.mp3'))
                with self.assertRaises(SystemExit):
                    music.main()
                self.assertEqual(len(list(output.glob('*.mp3'))), 2)
                self.assertEqual(list(output.glob('.import-*')), [])
                self.assertEqual(old.read_bytes(), original[0])

    def test_recursive_mp3_filter(self):
        parent = b'''<div id="flip-contents">
            <a href="https://drive.google.com/drive/folders/child_folder">Playlist</a>
            <a href="https://drive.google.com/file/d/song_a/view">A &amp; B.MP3</a>
            <a href="https://drive.google.com/file/d/other/view">songdata.h</a>
            <a href="https://other.example/file/d/trap/view">trap.mp3</a></div>'''
        child = b'''<div id="flip-contents">
            <a href="https://drive.google.com/drive/folders/root_folder">Cycle</a>
            <a href="https://drive.google.com/drive/folders/nested_folder">Nested</a>
            <a href="https://drive.google.com/file/d/song_b/view">second.mp3</a></div>'''
        nested = b'''<div id="flip-contents">
            <a href="https://drive.google.com/file/d/song_c/view">third.mp3</a></div>'''
        with patch.object(music, 'request', side_effect=[Response(parent), Response(child), Response(nested)]) as request:
            excluded = set()
            self.assertEqual(music.list_tracks('root_folder', root_tracks=excluded), [('song_b', 'second.mp3'), ('song_c', 'third.mp3')])
            self.assertEqual(excluded, {'song_a'})
            self.assertEqual(request.call_count, 3)

    def test_old_root_imports_are_archived_without_touching_other_music(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp)
            old = output / music.local_name('root_id', 'Root.mp3')
            keep = output / music.local_name('child_id', 'Child.mp3')
            manual = output / 'My own.mp3'
            for track in (old, keep, manual):
                track.write_bytes(b'ID3audio')
            with patch('sys.stdout', new_callable=io.StringIO):
                music.exclude_root_cache(output, {'root_id'})
                music.exclude_root_cache(output, {'root_id'})
            self.assertFalse(old.exists())
            self.assertTrue(keep.exists() and manual.exists())
            archived = list(output.glob('.excluded-root-*/*.mp3'))
            self.assertEqual(len(archived), 1)
            self.assertEqual(archived[0].read_bytes(), b'ID3audio')

    def test_inaccessible_listing_fails(self):
        with patch.object(music, 'request', return_value=Response(b'<html>Please sign in</html>')):
            with self.assertRaisesRegex(ValueError, 'not publicly readable'):
                music.list_tracks('root_folder')

    def test_flat_names_and_duplicate_titles(self):
        name = music.local_name('track_one', '../../escape/\\bad\nname.mp3')
        self.assertEqual(Path(name).name, name)
        self.assertNotIn('\n', name)
        self.assertNotEqual(music.local_name('track_one', 'same.mp3'), music.local_name('track_two', 'same.mp3'))
        self.assertLessEqual(len(music.local_name('x' * 33, 'ü' * 255 + '.mp3').encode()), 255)

    def test_confirmation_download(self):
        confirmation = b'''<form id="download-form" action="https://drive.usercontent.google.com/download">
            <input type="hidden" name="confirm" value="t">
            <input type="hidden" name="uuid" value="test"></form>'''
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / 'partial'
            with patch.object(music, 'request', side_effect=[Response(confirmation), Response(b'ID3test', 'audio/mpeg')]) as request:
                music.download('file_id', target, 1024)
                self.assertEqual(target.read_bytes(), b'ID3test')
                self.assertIn('https://drive.usercontent.google.com/download?', request.call_args.args[0])
                self.assertIn('id=file_id', request.call_args.args[0])

    def test_html_and_oversized_download_fail(self):
        with tempfile.TemporaryDirectory() as tmp:
            target = Path(tmp) / 'partial'
            for response in (Response(b'<html>quota exceeded</html>'), Response(b'ID3too-big', 'audio/mpeg')):
                with patch.object(music, 'request', return_value=response):
                    with self.assertRaises(ValueError):
                        music.download('file_id', target, 3)


if __name__ == '__main__':
    unittest.main()
