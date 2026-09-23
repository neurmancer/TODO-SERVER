#!/usr/bin/env python3
"""Compile the actual renderer and inspect its HTML without starting a server."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from html.parser import HTMLParser

root = Path(__file__).resolve().parents[1]

class HTML(HTMLParser):
    def __init__(self, source):
        super().__init__()
        self.ids = []
        self.links = []
        self.feed(source)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.append(attrs['id'])
        if tag == 'a':
            self.links.append(attrs.get('href'))

with tempfile.TemporaryDirectory(prefix='todo-markdown-') as directory:
    directory = Path(directory)
    harness = directory / 'render.c'
    harness.write_text('''
#include <stdio.h>
#include <stdlib.h>
#include "markdown.h"
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    char *html = render_markdown(argv[1]);
    if (!html) return 1;
    puts(html);
    free(html);
    return 0;
}
''')
    binary = directory / 'render'
    subprocess.run(shlex.split(os.environ.get('CC', 'cc')) + [
        '-Wall', '-Wextra', '-Werror', '-I', str(root / 'src'), str(harness),
        str(root / 'src/markdown.c'), str(root / 'vendor/md4c/md4c.c'),
        str(root / 'vendor/md4c/md4c-html.c'), str(root / 'vendor/md4c/entity.c'),
        '-o', str(binary)], check=True)

    def render(source):
        return subprocess.check_output([str(binary), source], text=True)

    source = '''[Thing](#thing)

# Thing
## Thing
### Thing-1
#### My **Thing**!
##### Café
###### !!!
## section
## content
Setext heading
--------------
## A &amp; B &#67; &#x44; &eacute;
## [Linked](#thing) `code`
'''
    html = HTML(render(source))
    assert html.ids == ['thing', 'thing-1', 'thing-1-1', 'my-thing', 'café',
                        'section', 'section-1', 'content-1', 'setext-heading',
                        'a--b-c-d-é', 'linked-code'], html.ids
    assert html.links == ['#thing', '#thing']
    assert HTML(render('# Thing')).ids == ['thing'], 'IDs reset for each note'
    assert HTML(render('```html\n<h2>Fake</h2>\n```\n\n<a id="thing"></a>')).ids == []
    unsafe = render('## &quot; onclick=&quot;evil\n\n[x](javascript:alert)')
    assert HTML(unsafe).ids == ['-onclickevil']
    assert HTML(unsafe).links == [None]
    assert '<p>[Thing](#thing)</p>' in render('[Thing]\\(#thing)')
    assert render('') == '\n'
print('Markdown heading and fragment checks passed')
