(() => {
    function initEditor() {
       
        const preview = document.getElementById('notes-preview');
        const form = document.getElementById('notes-editor');
        const edit = document.getElementById('edit-notes');
        const cancel = document.getElementById('cancel-notes');
        const save = document.getElementById('save-notes');
        const content = document.getElementById('content');
       
        if (!preview || !form || !edit || !cancel || !save || !content) return;

        if (form.dataset.initialized) return;
        
        form.dataset.initialized = 'true';
       
        const savedContent = content.value;
        const tabWidth = 4;

        function replace(start, end, text, selectionStart = start + text.length,
            selectionEnd = selectionStart, direction = 'none') {
            const { scrollTop, scrollLeft } = content;

            content.setRangeText(text, start, end, 'end');
            content.setSelectionRange(selectionStart, selectionEnd, direction);
            content.scrollTop = scrollTop;
            content.scrollLeft = scrollLeft;
            content.dispatchEvent(new Event('input', { bubbles: true }));
        }

        function column(text) {
            return [...text].reduce((col, char) =>
                col + (char === '\t' ? tabWidth - col % tabWidth : 1), 0);
        }

        function indentLines(unindent) {
            const { value, selectionStart: start, selectionEnd: end, selectionDirection } = content;
            const first = value.slice(0, start).lastIndexOf('\n') + 1;
            // A selection ending at the next line's start excludes that line
            
            const lastSelected = end > start && value[end - 1] === '\n' ? end - 1 : end;
            const nextNewline = value.indexOf('\n', lastSelected);
            const last = nextNewline < 0 ? value.length : nextNewline;
            const edits = [];
            
            let offset = first;
            
            const text = value.slice(first, last).split('\n').map(line => {
            
                const removed = unindent ? (line.match(/^(?:\t| {1,4})/) || [''])[0].length : 0;
            
                const added = unindent ? '' : ' '.repeat(tabWidth);
            
                edits.push({ offset, removed, added: added.length });
            
                offset += line.length + 1;
            
                return added + line.slice(removed);
            }).join('\n');
            const mapPosition = position => position + edits.reduce((delta, edit) => {
                if (position < edit.offset) return delta;
                return delta + edit.added - Math.min(edit.removed, position - edit.offset);
            }, 0);
            replace(first, last, text, mapPosition(start), mapPosition(end), selectionDirection);
        }

        edit.addEventListener('click', () => {
            preview.hidden = true;
            form.hidden = false;
            edit.setAttribute('aria-expanded', 'true');
            content.focus();
        });

        cancel.addEventListener('click', () => {
            content.value = savedContent;
            form.hidden = true;
            preview.hidden = false;
            edit.setAttribute('aria-expanded', 'false');
            edit.focus();
        });

        content.addEventListener('keydown', event => {
            if (event.isComposing || event.ctrlKey || event.altKey || event.metaKey) return;
            if (event.key === 'Escape') {
                event.preventDefault();
                save.focus();
                return;
            }

            const { value, selectionStart: start, selectionEnd: end } = content;
            const lineStart = value.slice(0, start).lastIndexOf('\n') + 1;
            const before = value.slice(lineStart, start);
            
            if (event.key === 'Tab') {
            
                event.preventDefault();
            
                if (event.shiftKey || start !== end) {
                    indentLines(event.shiftKey);
                } 
                
                else {
                    replace(start, end, ' '.repeat(tabWidth - column(before) % tabWidth));
                }
            } 
            
            else if (event.key === 'Backspace' && start === end && /^[ \t]+$/.test(before)) {
                event.preventDefault();
            
                const spaces = (before.match(/ +$/) || [''])[0].length;
                const remove = spaces ? Math.min(spaces, column(before) % tabWidth || tabWidth) : 1;
            
                replace(start - remove, end, '');
            } 
            else if (event.key === 'Enter' && !event.shiftKey) {
            
                const indent = before.match(/^[ \t]+/);
            
                if (indent) {
                    event.preventDefault();
                    replace(start, end, '\n' + indent[0]);
                }
            }
        });
    }

    initEditor();
    
    document.addEventListener('pagechange', initEditor);
})();
