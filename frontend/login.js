(() => {
    const form = document.getElementById('login-form');
    const password = document.getElementById('password');
    const toggle = document.getElementById('toggle-password');
    const submit = document.getElementById('sign-in');
    const status = document.getElementById('login-status');
    toggle.hidden = false;
    toggle.addEventListener('click', () => {
        const visible = password.type === 'password';
        password.type = visible ? 'text' : 'password';
        toggle.textContent = visible ? 'Hide' : 'Show';
        toggle.setAttribute('aria-label', visible ? 'Hide password' : 'Show password');
        toggle.setAttribute('aria-pressed', String(visible));
    });
    form.addEventListener('submit', async event => {
        event.preventDefault();
        if (submit.disabled) return;
        submit.disabled = true;
        form.setAttribute('aria-busy', 'true');
        submit.textContent = 'Signing in…';
        status.textContent = '';
        try {
            const response = await fetch('/login', {
                method: 'POST', body: new URLSearchParams(new FormData(form)),
                credentials: 'same-origin', cache: 'no-store', signal: AbortSignal.timeout(15000),
            });
            const result = await response.json();
            if (response.ok && result.ok) {
                window.location.replace('/');
                return;
            }
            status.textContent = result.error || 'Could not sign in. Try again.';
            password.value = '';
            password.focus();
        } catch {
            status.textContent = 'Could not reach the server. Check your connection and try again.';
        } finally {
            submit.disabled = false;
            submit.textContent = 'Enter cyberspace →';
            form.removeAttribute('aria-busy');
        }
    });
})();
