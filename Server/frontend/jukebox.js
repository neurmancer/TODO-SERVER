(() => {
    const button = document.getElementById('musicBtn');
    const nextButton = document.getElementById('nextSongBtn');
    const status = document.getElementById('music-status');
    if (!button || !nextButton || !status) return;

    let player = null;
    let selection = null;
    let playbackTimer;
    let currentUrl = null;

    function showPlaying(playing) {
        button.textContent = playing ? '❚❚' : '▶';
        button.classList.toggle('playing', playing);
        button.setAttribute('aria-pressed', String(playing));
        button.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
        button.title = playing ? 'Pause music' : 'Play music';
    }

    function stopPlayer() {
        clearTimeout(playbackTimer);
        const previous = player;
        player = null;
        if (previous) {
            previous.pause();
            previous.removeAttribute('src');
            previous.load();
        }
        showPlaying(false);
    }

    function fail(message) {
        stopPlayer();
        button.disabled = false;
        status.textContent = message;
        if (currentUrl) {
            const link = document.createElement('a');
            link.href = currentUrl;
            link.target = '_blank';
            link.rel = 'noopener noreferrer';
            link.textContent = 'Open audio source';
            status.append(' ', link);
        }
    }

    function watchPlayback(audio) {
        clearTimeout(playbackTimer);
        playbackTimer = setTimeout(() => {
            if (player === audio) {
                fail('Music stalled for 30 seconds. Press play to retry or choose next.');
            }
        }, 30000);
    }

    async function play(audio) {
        status.textContent = 'Loading music…';
        watchPlayback(audio);
        try {
            await audio.play();
        } catch (error) {
            if (player !== audio) return;
            clearTimeout(playbackTimer);
            showPlaying(false);
            if (error.name === 'NotAllowedError') {
                status.textContent = 'Press play to start music.';
            } else if (error.name !== 'AbortError') {
                fail('Could not play this audio. Check that the link opens an audio file without signing in, or choose next.');
            }
        }
    }

    async function startSong() {
        selection?.abort();
        const request = new AbortController();
        selection = request;
        stopPlayer();
        currentUrl = null;
        button.disabled = true;
        button.textContent = '…';
        status.textContent = 'Loading music…';
        const timeout = setTimeout(() => request.abort(), 10000);
        try {
            const response = await fetch('/jukebox/song', {
                cache: 'no-store', credentials: 'same-origin', signal: request.signal,
            });
            if (response.status === 401 || new URL(response.url).pathname === '/login') {
                throw new Error('Your session expired. Sign in again to load music.');
            }
            if (!response.ok) throw new Error('Could not fetch a song. Check your connection and try again.');
            const song = (await response.text()).trim();
            let url;
            try {
                url = new URL(song);
            } catch {
                throw new Error('The song list needs full HTTPS audio URLs.');
            }
            if (url.protocol !== 'https:' || url.username || url.password) {
                throw new Error('The song list needs HTTPS audio URLs without embedded credentials.');
            }
            if (selection !== request || request.signal.aborted) return;
            currentUrl = url.href;
            const audio = new Audio();
            player = audio;
            audio.preload = 'none';
            // Let the media element follow redirects and stream directly from the source.
            audio.src = currentUrl;
            audio.addEventListener('playing', () => {
                if (player !== audio) return;
                clearTimeout(playbackTimer);
                status.textContent = '';
                showPlaying(true);
            });
            audio.addEventListener('pause', () => {
                if (player !== audio) return;
                clearTimeout(playbackTimer);
                status.textContent = '';
                showPlaying(false);
            });
            audio.addEventListener('waiting', () => {
                if (player !== audio || audio.paused) return;
                status.textContent = 'Buffering music…';
                watchPlayback(audio);
            });
            audio.addEventListener('ended', () => {
                if (player === audio) startSong();
            });
            audio.addEventListener('error', () => {
                if (player !== audio) return;
                fail('Could not load this audio. The link may be unavailable, require sign-in, or use an unsupported format. Choose next to try another song.');
            });
            button.disabled = false;
            showPlaying(false);
            play(audio);
        } catch (error) {
            if (selection !== request) return;
            fail(error.name === 'AbortError'
                ? 'Song selection timed out. Press play to retry.' : error.message);
        } finally {
            clearTimeout(timeout);
            if (selection === request) selection = null;
        }
    }

    nextButton.addEventListener('click', startSong);
    button.addEventListener('click', () => {
        if (button.disabled) return;
        if (!player) startSong();
        else if (!player.paused) player.pause();
        else play(player);
    });
})();
