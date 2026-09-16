#!/bin/bash
set -euo pipefail
umask 077

die() { echo "ERROR: $*" >&2; exit 1; }
usage() {
    echo "Usage: $0 --domain HOSTNAME [--user USERNAME]"
    echo "Install a dedicated todo-caddy systemd service. Requires Caddy 2.8+ and ./build.sh install."
    echo "The login username defaults to todo; the password is prompted securely."
    echo "Runtime configuration is stored outside the repository in /etc/todo-caddy."
}

DOMAIN=""
LOGIN_USER=todo
while (( $# )); do
    case "$1" in
        --domain) [[ $# -ge 2 && -z $DOMAIN ]] || die "Supply --domain exactly once with a hostname."; DOMAIN=$2; shift 2 ;;
        --user) [[ $# -ge 2 ]] || die "Missing username."; LOGIN_USER=$2; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) die "Unknown option. Use --help." ;;
    esac
done

# Restrict interpolation to DNS labels, excluding Caddyfile syntax and URLs.
[[ ${#DOMAIN} -le 253 && $DOMAIN =~ ^[a-zA-Z0-9.-]+$ && $DOMAIN == *.* && $DOMAIN != *..* ]] || die "Supply a public DNS hostname, without scheme, port, or path."
IFS=. read -r -a LABELS <<< "$DOMAIN"
[[ $DOMAIN != *. && ! $DOMAIN =~ ^[0-9.]+$ ]] || die "Use a DNS hostname, not an IP address."
for LABEL in "${LABELS[@]}"; do
    [[ ${#LABEL} -le 63 && $LABEL =~ ^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$ ]] || die "Invalid DNS label."
done
[[ $LOGIN_USER =~ ^[a-zA-Z0-9_-]{1,64}$ ]] || die "Username must contain 1–64 letters, numbers, underscores, or hyphens."
[[ $(id -u) != 0 ]] || die "Run as the same regular user who installed the backend; sudo is used where needed."
for COMMAND in caddy sudo systemctl curl openssl python3; do
    command -v "$COMMAND" >/dev/null || die "Missing dependency: $COMMAND."
done
[[ -d /run/systemd/system ]] || die "systemd must be running."
id caddy >/dev/null 2>&1 || die "Install Caddy with its system caddy user first."
[[ ${HOME:-} == /* ]] || die "HOME must be an absolute path."
CERT="$HOME/.server/tls/cert.pem"
[[ -f $CERT ]] || die "Run ./build.sh install first."
systemctl is-active --quiet server.service || die "Start the installed server.service first."
if systemctl is-active --quiet caddy.service || systemctl is-active --quiet caddy-api.service; then
    die "Another Caddy service is running. Stop it if unused, or integrate this site into that installation instead."
fi
curl --cacert "$CERT" --fail --silent --show-error --max-time 5 \
    --output /dev/null https://127.0.0.1:8080/ || die "Backend HTTPS check failed."

STAGING=$(mktemp -d /tmp/todo-deploy.XXXXXX)
trap 'rm -rf -- "$STAGING"' EXIT
cat > "$STAGING/Caddyfile" <<EOF
{
    admin off
    persist_config off
}

$DOMAIN {
    reverse_proxy https://127.0.0.1:8080 {
        header_up -Authorization
        header_up Host {hostport}
        transport http {
            tls_trust_pool file /etc/todo-caddy/backend-cert.pem
            tls_server_name localhost
            versions 1.1
            keepalive off
        }
    }
}
EOF

cat > "$STAGING/todo-caddy.service" <<'EOF'
[Unit]
Description=Todo HTTPS gateway
Wants=network-online.target server.service
After=network-online.target server.service

[Service]
Type=simple
User=caddy
Group=caddy
Environment=XDG_DATA_HOME=/var/lib/todo-caddy
Environment=XDG_CONFIG_HOME=/var/lib/todo-caddy
ExecStart=/usr/bin/caddy run --config /etc/todo-caddy/Caddyfile --adapter caddyfile
Restart=on-failure
RestartSec=5
StateDirectory=todo-caddy
StateDirectoryMode=0700
AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
UMask=0077

[Install]
WantedBy=multi-user.target
EOF
[[ $(command -v caddy) == /usr/bin/caddy ]] || die "This service expects the packaged Caddy binary at /usr/bin/caddy."

# Validate using the source certificate before changing any installed configuration.
cp -- "$STAGING/Caddyfile" "$STAGING/validate.Caddyfile"
# Fixed temporary path contains no user-controlled Caddyfile syntax.
cp -- "$CERT" "$STAGING/backend-cert.pem"
sed -i "s|/etc/todo-caddy/backend-cert.pem|$STAGING/backend-cert.pem|" "$STAGING/validate.Caddyfile"
caddy validate --config "$STAGING/validate.Caddyfile" --adapter caddyfile
# Keep an existing gateway configuration until the backend confirms app login.
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
python3 "$SCRIPT_DIR/setup-auth.py" --user "$LOGIN_USER"
sudo systemctl restart server.service
AUTH_STATUS=$(curl --cacert "$CERT" --fail --silent --show-error --retry 5 --retry-connrefused \
    --retry-delay 1 --max-time 5 https://127.0.0.1:8080/auth/status)
[[ $AUTH_STATUS == '{"enabled":true}' ]] || die "Backend login is unavailable. Run ./build.sh update before deploying."
sudo install -d -o root -g caddy -m 0750 /etc/todo-caddy
sudo install -o root -g caddy -m 0640 "$CERT" /etc/todo-caddy/backend-cert.pem
sudo install -o root -g caddy -m 0640 "$STAGING/Caddyfile" /etc/todo-caddy/Caddyfile
sudo install -o root -g root -m 0644 "$STAGING/todo-caddy.service" /etc/systemd/system/todo-caddy.service
sudo systemctl daemon-reload
sudo systemctl enable todo-caddy.service
sudo systemctl restart todo-caddy.service
sudo systemctl is-active --quiet todo-caddy.service || die "Caddy did not start; inspect journalctl -u todo-caddy."
echo "Gateway started. Public HTTPS becomes available after DNS, ports 80/443, and certificate issuance succeed."
echo "Keep port 8080 private. Public-site certificates, private keys, and account data are managed in /var/lib/todo-caddy."
echo "Use sudo journalctl -u todo-caddy for certificate issuance diagnostics."
