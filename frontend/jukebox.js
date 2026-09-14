(() => {
    const button = document.getElementById('musicBtn');
    const panel = document.getElementById('music-player');
    const status = document.getElementById('music-status');
    if (!button || !panel || !status) return;

    let player = null;
    let apiPromise = null;
    let readyTimer;

    function showPlaying(playing) {
        button.textContent = playing ? '❚❚' : '▶';
        button.classList.toggle('playing', playing);
        button.setAttribute('aria-pressed', String(playing));
        button.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
        button.title = playing ? 'Pause music' : 'Play music';
    }

    function fail() {
        clearTimeout(readyTimer);
        if (player) player.destroy();
        player = null;
        panel.hidden = true;
        button.disabled = false;
        showPlaying(false);
        status.textContent = 'Music could not load. Press play to try again.';
    }

    function loadYouTube() {
        if (window.YT?.Player) return(Promise.resolve());
        if (apiPromise) return(apiPromise);
        apiPromise = new Promise((resolve, reject) => {
            const script = document.createElement('script');
            const timer = setTimeout(unavailable, 15000);
            function unavailable() {
                clearTimeout(timer);
                script.remove();
                apiPromise = null;
                reject(new Error('YouTube did not load'));
            }
            window.onYouTubeIframeAPIReady = () => {
                clearTimeout(timer);
                resolve();
            };
            script.src = 'https://www.youtube.com/iframe_api';
            script.onerror = unavailable;
            document.head.appendChild(script);
        });
        return(apiPromise);
    }

    async function randomVideo() {
        //Fuck...now I am a JS guy?
        const response = await fetch('/jukebox/song', {
            cache: 'no-store', signal: AbortSignal.timeout(10000),
        });
        if (!response.ok) throw new Error('Song selection failed');
        const song = (await response.text()).trim();
        if (/^[\w-]{11}$/.test(song)) return(song);
        const url = new URL(song);
        const hosts = ['youtube.com', 'www.youtube.com', 'm.youtube.com', 'youtu.be'];
        if (!hosts.includes(url.hostname) || url.protocol !== 'https:') {
            throw new Error('Expected a YouTube URL');
        }
        const id = url.hostname === 'youtu.be' ? url.pathname.slice(1)
            : url.pathname.startsWith('/embed/') ? url.pathname.slice(7)
            : url.searchParams.get('v');
        if (!/^[\w-]{11}$/.test(id || '')) throw new Error('Invalid video ID');
        return(id);
    }

    async function startSong() {
        button.disabled = true;
        button.textContent = '…';
        status.textContent = 'Loading music…';
        try {
            const [videoId] = await Promise.all([randomVideo(), loadYouTube()]);
            if (player) {
                player.loadVideoById(videoId);
                button.disabled = false;
                status.textContent = '';
                return;
            }
            const mount = document.createElement('div');
            panel.replaceChildren(mount);
 
            readyTimer = setTimeout(fail, 15000);
            player = new YT.Player(mount, {
                width: '0', height: '0', videoId,
                playerVars: { playsinline: 1, origin: window.location.origin },
                events: {
                    onReady(event) {
                        if (event.target !== player) return;
                        clearTimeout(readyTimer);
                        button.disabled = false;
                        status.textContent = '';
                        showPlaying(false);
                        event.target.playVideo();
                    },
                    onStateChange(event) {
                        if (event.target !== player) return;
                        showPlaying(event.data === YT.PlayerState.PLAYING);
                        if (event.data === YT.PlayerState.ENDED) startSong();
                    },
                    onAutoplayBlocked(event) {
                        if (event.target !== player) return;
                        showPlaying(false);
                        status.textContent = 'Press play to start music.';
                    },
                    onError(event) {
                        if (event.target === player) fail();
                    },
                },
            });
        } catch {
            fail();
        }
    }

    button.addEventListener('click', () => {
        if (button.disabled) return;
        status.textContent = '';
        if (!player) {
            startSong();
        } else if (player.getPlayerState() === YT.PlayerState.PLAYING) {
            player.pauseVideo();
        } else {
            player.playVideo();
        }
    });
})();
