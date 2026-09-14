(() => {
    const preview = document.getElementById('notes-preview');
    const form = document.getElementById('notes-editor');
    const edit = document.getElementById('edit-notes');
    const cancel = document.getElementById('cancel-notes');
    const save = document.getElementById('save-notes');
    const content = document.getElementById('content');
    if (!preview || !form || !edit || !cancel || !save || !content) return;

    const savedContent = content.value;

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
        } else if (event.key === 'Tab' && !event.shiftKey) {
            event.preventDefault();
            const scrollTop = content.scrollTop;
            content.setRangeText('    ', content.selectionStart, content.selectionEnd, 'end');
            content.scrollTop = scrollTop;
            content.dispatchEvent(new Event('input', { bubbles: true }));
        }
    });
})();
