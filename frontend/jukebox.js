(() => {
    const button = document.getElementById('musicBtn');
    const nextButton = document.getElementById('nextSongBtn');
    const panel = document.getElementById('music-player');
    const status = document.getElementById('music-status');
    if (!button || !nextButton || !panel || !status) return;

    let player = null;
    let apiPromise = null;
    let readyTimer;
    let playbackTimer;
    let currentVideoId = null;

    const youtubeErrors = {
        2: 'YouTube rejected the video ID or player parameters.',
        5: 'The browser could not play this video in the YouTube player.',
        100: 'This video was removed, is private, or could not be found.',
        101: 'The video owner does not allow playback on other websites.',
        150: 'The video owner does not allow playback on other websites.',
        153: 'YouTube could not identify this website. Check browser referrer settings.',
    };

    function watchPlayback() {
        clearTimeout(playbackTimer);
        playbackTimer = setTimeout(() => {
            fail('Playback stalled for 30 seconds. Try opening the song on YouTube or choose next.');
        }, 30000);
    }

    function showPlaying(playing) {
        button.textContent = playing ? '❚❚' : '▶';
        button.classList.toggle('playing', playing);
        button.setAttribute('aria-pressed', String(playing));
        button.setAttribute('aria-label', playing ? 'Pause music' : 'Play music');
        button.title = playing ? 'Pause music' : 'Play music';
    }

    function fail(message = 'Music could not load. Press play to try again.', code = null) {
        clearTimeout(readyTimer);
        clearTimeout(playbackTimer);
        const failedPlayer = player;
        player = null;
        if (failedPlayer) failedPlayer.destroy();
        panel.hidden = true;
        button.disabled = nextButton.disabled = false;
        showPlaying(false);
        status.textContent = message;
        if (currentVideoId) {
            const link = document.createElement('a');
            link.href = `https://www.youtube.com/watch?v=${currentVideoId}`;
            link.target = '_blank';
            link.rel = 'noopener noreferrer';
            link.textContent = `Open song on YouTube (${currentVideoId})`;
            status.append(' ', link);
        }
        console.warn('Jukebox playback failed', { videoId: currentVideoId, code, message });
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
        if (button.disabled) return;
        button.disabled = nextButton.disabled = true;
        clearTimeout(playbackTimer);
        currentVideoId = null;
        button.textContent = '…';
        status.textContent = 'Loading music…';
        try {
            const [videoId] = await Promise.all([randomVideo(), loadYouTube()]);
            currentVideoId = videoId;
            if (player) {
                watchPlayback();
                player.loadVideoById(videoId);
                button.disabled = nextButton.disabled = false;
                status.textContent = '';
                return;
            }
            const mount = document.createElement('div');
            panel.replaceChildren(mount);
 
            readyTimer = setTimeout(() => fail('The YouTube player did not become ready. Try again.'), 15000);
            player = new YT.Player(mount, {
                width: '0', height: '0', videoId,
                playerVars: { playsinline: 1, origin: window.location.origin },
                events: {
                    onReady(event) {
                        if (event.target !== player) return;
                        clearTimeout(readyTimer);
                        button.disabled = nextButton.disabled = false;
                        status.textContent = '';
                        showPlaying(false);
                        watchPlayback();
                        event.target.playVideo();
                    },
                    onStateChange(event) {
                        if (event.target !== player) return;
                        showPlaying(event.data === YT.PlayerState.PLAYING);
                        if (event.data === YT.PlayerState.BUFFERING) {
                            watchPlayback();
                        } else if ([YT.PlayerState.PLAYING, YT.PlayerState.PAUSED,
                            YT.PlayerState.ENDED].includes(event.data)) {
                            clearTimeout(playbackTimer);
                        }
                        if (event.data === YT.PlayerState.ENDED) startSong();
                    },
                    onAutoplayBlocked(event) {
                        if (event.target !== player) return;
                        clearTimeout(playbackTimer);
                        showPlaying(false);
                        status.textContent = 'Press play to start music.';
                    },
                    onError(event) {
                        if (event.target !== player) return;
                        const reason = youtubeErrors[event.data] || 'YouTube reported an unknown playback error.';
                        fail(`${reason} (YouTube error ${event.data}) Choose next to try another song.`, event.data);
                    },
                },
            });
        } catch (error) {
            console.warn('Jukebox loading error', error);
            fail('Could not fetch a song or load YouTube. Check your connection and try again.');
        }
    }

    nextButton.addEventListener('click', startSong);

    button.addEventListener('click', () => {
        if (button.disabled) return;
        status.textContent = '';
        if (!player) {
            startSong();
        } else if (player.getPlayerState() === YT.PlayerState.PLAYING) {
            player.pauseVideo();
        } else {
            watchPlayback();
            player.playVideo();
        }
    });
})();
