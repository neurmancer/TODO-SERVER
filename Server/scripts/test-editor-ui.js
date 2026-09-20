// Exercise the shipped keyboard handlers without requiring a browser.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const source = fs.readFileSync(path.join(__dirname, '../frontend/editor.js'), 'utf8');

function editor(value, start, end = start, direction = 'none') {
    const elements = Object.fromEntries(['notes-preview', 'notes-editor', 'edit-notes',
        'cancel-notes', 'save-notes', 'content'].map(id => [id, {
        dataset: {}, handlers: {}, attrs: {},
        addEventListener(type, handler) { this.handlers[type] = handler; },
        setAttribute(name, value) { this.attrs[name] = value; },
        focus() { this.focused = true; },
    }]));
    const content = Object.assign(elements.content, {
        value, selectionStart: start, selectionEnd: end, selectionDirection: direction,
        scrollTop: 123, scrollLeft: 12, inputs: 0,
        setRangeText(text, from, to) {
            this.value = this.value.slice(0, from) + text + this.value.slice(to);
        },
        setSelectionRange(from, to, direction) {
            this.selectionStart = from;
            this.selectionEnd = to;
            this.selectionDirection = direction;
        },
        dispatchEvent(event) { if (event.type === 'input') this.inputs++; },
    });
    const document = { getElementById: id => elements[id], addEventListener() {} };
    vm.runInNewContext(source, { document, Event });
    return { content, elements, key(key, modifiers = {}) {
        const event = { key, preventDefault() { this.prevented = true; }, ...modifiers };
        content.handlers.keydown(event);
        assert.equal(content.scrollTop, 123);
        assert.equal(content.scrollLeft, 12);
        return !!event.prevented;
    } };
}

let e = editor('ab', 2);
e.key('Tab');
assert.equal(e.content.value, 'ab  ');
assert.equal(e.content.selectionStart, 4);
assert.equal(e.content.inputs, 1);

for (const [value, expected] of [['    ', ''], ['      ', '    '], ['\t', ''], ['\t  ', '\t']]) {
    e = editor(value + 'note', value.length);
    assert.equal(e.key('Backspace'), true);
    assert.equal(e.content.value, expected + 'note');
    assert.equal(e.content.selectionStart, expected.length);
}
for (const [value, start, end] of [['note    ', 8, 8], ['    note', 0, 4], ['note', 0, 0]]) {
    e = editor(value, start, end);
    assert.equal(e.key('Backspace'), false, 'Ordinary deletion remains native');
}

e = editor('one\ntwo\nthree', 0, 8, 'backward');
e.key('Tab');
assert.equal(e.content.value, '    one\n    two\nthree');
assert.equal(e.content.selectionDirection, 'backward');
assert.equal(e.content.selectionEnd, 16);
e.key('Tab', { shiftKey: true });
assert.equal(e.content.value, 'one\ntwo\nthree');
assert.equal(e.content.selectionStart, 0);
assert.equal(e.content.selectionEnd, 8);

e = editor('    note', 2);
e.key('Tab', { shiftKey: true });
assert.equal(e.content.value, 'note');
assert.equal(e.content.selectionStart, 0);
e = editor('\nnext', 0);
e.key('Tab');
assert.equal(e.content.value, '    \nnext');
e = editor('\t  note', 7);
e.key('Enter');
assert.equal(e.content.value, '\t  note\n\t  ');
assert.equal(e.content.selectionStart, 11);

for (const modifier of ['ctrlKey', 'altKey', 'metaKey', 'isComposing']) {
    e = editor('    note', 4);
    for (const key of ['Tab', 'Backspace', 'Enter']) {
        assert.equal(e.key(key, { [modifier]: true }), false);
        assert.equal(e.content.value, '    note');
    }
}
e = editor('original', 8);
e.elements['edit-notes'].handlers.click();
assert.equal(e.elements['notes-editor'].hidden, false);
e.key('Tab');
e.key('Escape');
assert.equal(e.elements['save-notes'].focused, true);
e.elements['cancel-notes'].handlers.click();
assert.equal(e.content.value, 'original');
assert.equal(e.elements['notes-editor'].hidden, true);
assert.equal(e.elements['edit-notes'].focused, true);
console.log('Editor keyboard and cancel checks passed');
