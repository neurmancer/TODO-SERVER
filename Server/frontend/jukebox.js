(() => {
    const button = document.getElementById('musicBtn');
    const nextButton = document.getElementById('nextSongBtn');
    const infoButton = document.getElementById('musicInfoBtn');
    const panel = document.getElementById('music-player');
    const status = document.getElementById('music-status');
    if (!button || !nextButton || !infoButton || !panel || !status) return;

    // One element survives the site's page-content navigation. Every tab/device
    // owns its playback position and shuffled queue; the server only sends files.
    const audio = new Audio();
    audio.preload = 'none';
    panel.hidden = true;
    panel.innerHTML = '<div class="music-port-heading"><span>RADIOPORT</span><span aria-hidden="true">// FM_01</span></div>'
        + '<p class="music-port-label">// incoming transmission</p>'
        + '<p class="music-title music-glitch">No signal yet. Hit play.</p>'
        + '<output class="music-time music-glitch" for="music-seek">0:00 / 0:00</output>'
        + '<label for="music-seek">Seek through song</label>'
        + '<input id="music-seek" type="range" min="0" max="1000" value="0" disabled>';
    panel.append(audio);
    const title = panel.querySelector('.music-title');
    const seek = panel.querySelector('input');
    const time = panel.querySelector('output');
    let tracks = [];
    let queue = [];
    let current = null;
    let generation = 0;
    let stallTimer;

    function showPlaying(playing) {
        button.textContent = playing ? '❚❚' : '▶';
        button.classList.toggle('playing', playing);
        button.setAttribute('aria-pressed', String(playing));
        button.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
        button.title = playing ? 'Pause music' : 'Play music';
        if ('mediaSession' in navigator) navigator.mediaSession.playbackState = playing ? 'playing' : 'paused';
    }

    function watchPlayback() {
        clearTimeout(stallTimer);
        stallTimer = setTimeout(() => {
            generation++;
            audio.pause();
            status.textContent = 'Music stalled. Press play to retry, or choose next.';
        }, 30000);
    }

    function clock(seconds) {
        if (!Number.isFinite(seconds)) return '0:00';
        return `${Math.floor(seconds / 60)}:${String(Math.floor(seconds % 60)).padStart(2, '0')}`;
    }

    function updateProgress() {
        const duration = audio.duration;
        seek.disabled = !Number.isFinite(duration) || duration <= 0;
        seek.value = seek.disabled ? 0 : Math.round(audio.currentTime / duration * 1000);
        seek.setAttribute('aria-valuetext', `${clock(audio.currentTime)} of ${clock(duration)}`);
        time.textContent = `${clock(audio.currentTime)} / ${clock(duration)}`;
        time.setAttribute('data-text', time.textContent);
    }

    function play() {
        const attempt = ++generation;
        status.textContent = 'Loading music…';
        watchPlayback();
        if (audio.error) audio.load();
        // Called directly from the tap, with the catalogue already in memory.
        audio.play().catch(error => {
            if (attempt !== generation) return;
            clearTimeout(stallTimer);
            showPlaying(false);
            status.textContent = error.name === 'NotAllowedError'
                ? 'Tap play to allow audio on this device.'
                : 'Could not play this MP3. Retry, choose next, or check your connection.';
        });
    }

    function next() {
        if (!tracks.length) return;
        if (!queue.length) {
            queue = [...tracks];
            for (let i = queue.length - 1; i > 0; i--) {
                const j = Math.floor(Math.random() * (i + 1));
                [queue[i], queue[j]] = [queue[j], queue[i]];
            }
            if (queue.length > 1 && queue[queue.length - 1].url === current?.url) {
                [queue[0], queue[queue.length - 1]] = [queue[queue.length - 1], queue[0]];
            }
        }
        generation++;
        audio.pause();
        current = queue.pop();
        audio.src = current.url;
        title.textContent = current.title.replace(/ \[[\w-]{10,}\](?=\.mp3$)/i, '').replace(/\.mp3$/i, '');
        title.setAttribute('data-text', title.textContent);
        updateProgress();
        if ('mediaSession' in navigator && 'MediaMetadata' in window) {
            navigator.mediaSession.metadata = new MediaMetadata({ title: title.textContent, artist: 'Cyberspace jukebox' });
        }
        play();
    }

    async function loadCatalogue() {
        button.disabled = nextButton.disabled = true;
        button.textContent = '…';
        try {
            const response = await fetch('/jukebox/songs', { cache: 'no-store', signal: AbortSignal.timeout(10000) });
            if (response.redirected || response.status === 401) throw new Error('Sign in again to load music.');
            if (!response.ok) throw new Error('Could not load music. Press play to retry.');
            const data = await response.json();
            if (!Array.isArray(data)) throw new Error('Invalid music catalogue.');
            tracks = data.filter(track => typeof track.title === 'string'
                && typeof track.url === 'string' && /^\/jukebox\/audio\/[a-f0-9]{64}$/.test(track.url));
            status.textContent = tracks.length ? '' : 'No MP3s in the server’s music library yet. Press play to refresh.';
        } catch (error) {
            status.textContent = error.message || 'Could not load music. Press play to retry.';
        } finally {
            showPlaying(false);
            button.disabled = false;
            nextButton.disabled = !tracks.length;
        }
    }

    button.addEventListener('click', () => {
        if (!tracks.length) { loadCatalogue(); return; }
        if (!current) next();
        else if (audio.paused) play();
        else { generation++; audio.pause(); }
    });
    nextButton.addEventListener('click', next);
    infoButton.addEventListener('click', () => {
        panel.hidden = !panel.hidden;
        infoButton.setAttribute('aria-expanded', String(!panel.hidden));
        infoButton.setAttribute('aria-label', panel.hidden ? 'Show track info' : 'Hide track info');
        infoButton.title = panel.hidden ? 'Show track info' : 'Hide track info';
    });
    seek.addEventListener('input', () => {
        if (Number.isFinite(audio.duration)) audio.currentTime = Number(seek.value) / 1000 * audio.duration;
        updateProgress();
    });
    audio.addEventListener('playing', () => {
        clearTimeout(stallTimer);
        status.textContent = '';
        showPlaying(true);
    });
    audio.addEventListener('pause', () => {
        clearTimeout(stallTimer);
        showPlaying(false);
    });
    audio.addEventListener('waiting', () => { if (!audio.paused) watchPlayback(); });
    audio.addEventListener('ended', next);
    audio.addEventListener('error', () => {
        clearTimeout(stallTimer);
        showPlaying(false);
        status.textContent = 'Could not load this MP3. Retry, choose next, or sign in again.';
    });
    for (const event of ['timeupdate', 'durationchange', 'loadedmetadata', 'emptied']) {
        audio.addEventListener(event, updateProgress);
    }
    if ('mediaSession' in navigator) {
        const actions = {
            play: () => { if (current) play(); },
            pause: () => { generation++; audio.pause(); },
            nexttrack: next,
            seekto: event => {
                if (Number.isFinite(event.seekTime) && Number.isFinite(audio.duration)) {
                    audio.currentTime = Math.max(0, Math.min(event.seekTime, audio.duration));
                }
            },
        };
        for (const [action, handler] of Object.entries(actions)) {
            try { navigator.mediaSession.setActionHandler(action, handler); } catch (_) { /* Optional browser support. */ }
        }
    }
    loadCatalogue();
})();
