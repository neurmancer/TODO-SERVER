// Browser-independent playback-flow checks actual audio decoding is checked on devices.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../frontend/jukebox.js'), 'utf8');
const flush = () => new Promise(resolve => setImmediate(resolve));

class Element {
    constructor() {
        this.handlers = {};
        this.attrs = {};
        this.textContent = '';
        this.hidden = true;
        this.classList = { toggle() {} };
    }
    addEventListener(event, handler) { (this.handlers[event] ||= []).push(handler); }
    emit(event) { for (const handler of this.handlers[event] || []) handler({}); }
    setAttribute(key, value) { this.attrs[key] = value; }
    append(child) { this.child = child; }
}

function client(catalogue) {
    const elements = Object.fromEntries(['musicBtn', 'nextSongBtn', 'musicInfoBtn', 'music-player', 'music-status']
        .map(id => [id, new Element()]));
    const title = new Element(), seek = new Element(), time = new Element();
    elements['music-player'].querySelector = selector => ({ '.music-title': title, input: seek, output: time })[selector];
    let audio, gesture = false, calls = 0;
    class Audio extends Element {
        constructor() {
            super();    //Owww thx...
            audio = this;
            this.paused = true;
            this.currentTime = 0;
            this.duration = NaN;
            this.playCalls = 0;
            this.loadCalls = 0;
        }
        play() {
            this.playCalls++;
            this.playWasInGesture = gesture;
            if (this.rejectPlay) return Promise.reject(Object.assign(new Error('blocked'), { name: 'NotAllowedError' }));
            this.paused = false;
            this.emit('playing');
            return Promise.resolve();
        }
        
        pause() { this.paused = true; this.emit('pause'); }
        load() { this.loadCalls++; this.error = null; }
    }

    vm.runInNewContext(source, {
        document: { getElementById: id => elements[id] }, Audio,
        window: {}, navigator: {}, AbortSignal,
        fetch: async () => { calls++; return { ok: true, json: async () => catalogue }; },
        setTimeout: () => 1, clearTimeout() {}, console,
    });

    return {
        elements, audio, title, seek, time,
        get fetchCalls() { return calls; },
        click(id) { gesture = true; elements[id].emit('click'); gesture = false; },
    };
}

(async () => {
    const tracks = ['a', 'b', 'c'].map(id => ({ title: `Song ${id}.mp3`, url: '/jukebox/audio/' + id.repeat(64) }));
    const first = client(tracks), second = client(tracks);
    
    await flush();
    
    assert.equal(first.audio.playCalls, 0, 'Loading the page must remain silent');
    assert.equal(first.elements['music-player'].hidden, true, 'Track info starts hidden');
    first.click('musicInfoBtn');
    assert.equal(first.elements['music-player'].hidden, false, 'Info can open before playback');
    assert.equal(first.elements.musicInfoBtn.attrs['aria-expanded'], 'true');
    assert.equal(first.audio.playCalls, 0, 'Info toggle must not start playback');
    first.click('musicInfoBtn');
    
    first.click('musicBtn');
    
    assert.equal(first.audio.playWasInGesture, true, 'Initial playback must remain inside the tap');
    assert.equal(first.elements.musicBtn.attrs['aria-pressed'], 'true');
    assert.equal(second.audio.playCalls, 0, 'Another listener must remain independent');
    assert.equal(first.elements['music-player'].hidden, true, 'Playing must not reveal track info');
    first.click('musicInfoBtn');
    assert.equal(first.elements['music-player'].hidden, false);
    assert.equal(first.audio.playCalls, 1, 'Opening info must not restart playback');
    
    first.audio.duration = 200;
    first.audio.emit('loadedmetadata');
    first.seek.value = 500;
    first.seek.emit('input');
    //Yup trying to make the code more readable-ish
    assert.equal(first.audio.currentTime, 100);
    assert.equal(second.audio.currentTime, 0);
    assert.equal(first.time.textContent, '1:40 / 3:20');
    assert.equal(first.time.attrs['data-text'], first.time.textContent, 'Duration glitch follows playback');
    assert.equal(first.title.attrs['data-text'], first.title.textContent, 'Title glitch follows track');
    first.click('musicInfoBtn');
    assert.equal(first.elements.musicInfoBtn.attrs['aria-expanded'], 'false');
    assert.equal(first.audio.paused, false, 'Closing info must not pause playback');
    
    first.click('musicBtn');
    
    assert.equal(first.audio.paused, true);
    
    first.click('musicBtn');
    
    assert.equal(first.audio.currentTime, 100, 'Resume must retain playback position');
    
    const heard = new Set([first.audio.src]);
    
    for (let i = 0; i < 2; i++) {
        first.click('nextSongBtn');
        assert.equal(first.audio.playWasInGesture, true);
        heard.add(first.audio.src);
    }
    
    assert.equal(heard.size, 3, 'Shuffle should play each track once before repeating');
    
    const last = first.audio.src;
    
    first.click('nextSongBtn');
    
    assert.notEqual(first.audio.src, last, 'Queue refill should avoid an immediate repeat');
    
    const plays = first.audio.playCalls;
    
    first.audio.emit('ended');
    
    assert.equal(first.audio.playCalls, plays + 1);
    assert.equal(first.elements['music-player'].hidden, true, 'Skipping and automatic advance must keep info hidden');
    
    first.audio.error = { code: 3 };
    first.audio.emit('error');
    
    assert.equal(first.audio.playCalls, plays + 1, 'A bad file must not trigger an endless automatic skip loop');
    
    first.audio.paused = true;
    first.audio.rejectPlay = true;
    first.click('musicBtn');
    
    await flush();
    
    assert.equal(first.audio.loadCalls, 1, 'Retry must reload a failed resource');
    assert.match(first.elements['music-status'].textContent, /Tap play/);
    assert.equal(first.fetchCalls, 1, 'Track switching should not wait for an async catalogue request');
    
    const empty = client([]);
    
    await flush();
    
    assert.equal(empty.elements.nextSongBtn.disabled, true);
    
    empty.click('musicBtn');
    
    await flush();
    
    assert.equal(empty.fetchCalls, 2, 'Empty libraries must be refreshable after import');
    assert.equal(empty.audio.playCalls, 0);

    const named = client([{ title: 'Nightcore - American Idiot [abcdefghijk].mp3', url: '/jukebox/audio/' + 'd'.repeat(64) }]);
    await flush();
    named.click('musicBtn');
    assert.equal(named.title.textContent, 'Nightcore - American Idiot', 'Keep artist and song while removing filename suffixes');
    named.click('musicInfoBtn');
    named.click('nextSongBtn');
    assert.equal(named.elements['music-player'].hidden, false, 'An open panel stays open across tracks');
    
    console.log('Passed: track-info toggle, artist/song display, silent startup, tap playback, independent listeners, seeking, resume, shuffle, end/error handling, retry and empty-library refresh');
})().catch(error => { console.error(error); process.exitCode = 1; });
