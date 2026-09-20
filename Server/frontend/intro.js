(() => {
    const androidClient = /\bCyberspaceAndroid\/1\.0\b/.test(navigator.userAgent);
    const reducedMotion = matchMedia('(prefers-reduced-motion: reduce)');
    if (!androidClient || reducedMotion.matches) return;
    try {
        if (sessionStorage.getItem('cyberspace-intro')) return;
        sessionStorage.setItem('cyberspace-intro', 'seen');
    } catch (_) { /* The intro still works when storage is unavailable. */ }

    const root = document.documentElement;
    root.classList.add('intro-pending');
    let splash;
    let finished = false;
    let blocked = [];
    const finish = () => {
        finished = true;
        root.classList.remove('intro-pending', 'intro-playing');
        if (splash) splash.hidden = true;
        blocked.forEach(element => { element.inert = false; });
        blocked = [];
    };
    // Never leave the page covered if parsing or animation is interrupted.
    setTimeout(finish, 7000);
    reducedMotion.addEventListener('change', event => { if (event.matches) finish(); });
    document.addEventListener('DOMContentLoaded', () => {
        if (finished) return;
        splash = document.querySelector('.site-intro');
        if (!splash) { finish(); return; }
        blocked = [...document.body.children].filter(element => element !== splash && !element.inert);
        blocked.forEach(element => { element.inert = true; });
        splash.hidden = false;
        root.classList.add('intro-playing');
        splash.addEventListener('click', finish, { once: true });
        splash.addEventListener('animationend', event => {
            if (event.target === splash) finish();
        });
        setTimeout(finish, 4400);
    }, { once: true });
})();
