// Run the actual navigator with a small DOM/history harness.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../frontend/navigation.js'), 'utf8');
const handlers = {};
const windowHandlers = {};
const entries = [{ href: 'https://todo.test/todos/2', state: {} }];
let index = 0;
let fetches = 0;
const location = {
    get href() { return entries[index].href; },
    get origin() { return new URL(this.href).origin; },
    get pathname() { return new URL(this.href).pathname; },
    get search() { return new URL(this.href).search; },
};
const window = { location, scrollY: 40,
    addEventListener(type, handler) { windowHandlers[type] = handler; },
    scrollTo(x, y) { this.scrollY = y; },
};
const target = { setAttribute() {}, focus() { this.focused = true; },
    scrollIntoView() { window.scrollY = 800; } };
const content = { contains() { return true; }, querySelector() { return null; } };
const history = {
    get state() { return entries[index].state; },
    replaceState(state, unused, url) { entries[index] = { state, href: String(url) }; },
    pushState(state, unused, url) {
        entries.splice(index + 1);
        entries.push({ state, href: String(url) });
        index++;
    },
};
const document = {
    getElementById(id) {
        if (id === 'page-content') return content;
        if (id === 'navigation-status') return {};
        if (['thing', 'café'].includes(id)) return target;
        return null;
    },
    querySelectorAll() { return []; },
    addEventListener(type, handler) { handlers[type] = handler; },
};
vm.runInNewContext(source, { window, document, history, URL, console,
    fetch() { fetches++; return Promise.resolve({ json: async () => ({ enabled: false }) }); },
});
function click(href, extra = {}) {
    const link = { href: new URL(href, location.href).href, hasAttribute() { return false; } };
    const event = { button: 0, target: { closest() { return link; } },
        preventDefault() { this.defaultPrevented = true; }, ...extra };
    handlers.click(event);
    return event;
}
function pop(next) { index = next; windowHandlers.popstate(); }
assert.equal(click('#thing').defaultPrevented, true);
assert.equal(location.href, 'https://todo.test/todos/2#thing');
assert.equal(window.scrollY, 800);
assert.equal(target.focused, true);
assert.equal(entries.length, 2);
window.scrollY = 920;
windowHandlers.scroll();
pop(0);
assert.equal(window.scrollY, 40);
pop(1);
assert.equal(window.scrollY, 920);
click('#thing');
assert.equal(entries.length, 2, 'Repeated anchor clicks must not add history entries');
assert.equal(window.scrollY, 800);
click('#caf%C3%A9');
assert.equal(window.scrollY, 800);
assert.doesNotThrow(() => click('#%broken'));
assert.equal(window.scrollY, 800, 'Missing fragments keep the current scroll');
click('#');
assert.equal(window.scrollY, 0);
assert.equal(click('#thing', { ctrlKey: true }).defaultPrevented, undefined);
assert.equal(click('#thing', { button: 1 }).defaultPrevented, undefined);
assert.equal(click('https://elsewhere.test/#thing').defaultPrevented, undefined);
// Native entries (for example, location.hash assignments) may have no saved position.
entries.push({ href: 'https://todo.test/todos/2#thing', state: null });
pop(entries.length - 1);
assert.equal(window.scrollY, 800);
assert.equal(fetches, 1, 'Only the initial auth status request should fetch');
console.log('In-page fragment navigation and history checks passed');
