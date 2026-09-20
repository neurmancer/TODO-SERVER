#!/bin/bash
set -euo pipefail
umask 077

# Additional SANs can be supplied as one OpenSSL list, e.g. IP:192.168.1.20,DNS:todo.local.
# (Again...I'll make this public but won't give you the whole implementation you can figure it out lol)
[[ $# -le 1 ]] || { echo "Usage: $0 [additional-subject-alt-names]" >&2; exit 1; }
[[ ${HOME:-} == /* ]] || { echo "HOME must be an absolute path." >&2; exit 1; }
RUNTIME_DIR=${TODO_SERVER_PATH:-"$HOME/.server"}
[[ $RUNTIME_DIR == /* ]] || { echo "TODO_SERVER_PATH must be an absolute path." >&2; exit 1; }
TLS_DIR="$RUNTIME_DIR/tls"
CERT="$TLS_DIR/cert.pem"
KEY="$TLS_DIR/key.pem"

# Well I've handled the certifcate but opera is still bitching tho...so no idea about what to do

if [[ -e $CERT || -e $KEY || -L $CERT || -L $KEY ]]; then
    [[ -f $CERT && -f $KEY ]] || {
        echo "Incomplete certificate pair in $TLS_DIR; restore it or move it aside before regenerating." >&2
        exit 1
    }
    openssl x509 -in "$CERT" -noout -checkend 0 || {
        echo "Certificate is expired or invalid; move the old pair aside and regenerate." >&2
        exit 1
    }
    echo "Preserving existing certificate and key in $TLS_DIR."
    exit 0
fi

SANS="DNS:localhost,IP:127.0.0.1"
if command -v ip >/dev/null; then
    ADDRESSES=$(ip -o -4 addr show up) || {
        echo "Could not discover LAN addresses; supply them explicitly or retry with network access." >&2
        exit 1
    }
    while read -r ADDRESS; do
        [[ -z $ADDRESS || $ADDRESS == 127.* ]] || SANS+=",IP:$ADDRESS"
    done < <(awk '{split($4, address, "/"); print address[1]}' <<< "$ADDRESSES")
else
    echo "ip command unavailable; add LAN IPs explicitly if needed." >&2
fi
[[ -z ${1:-} ]] || SANS+=",$1"

mkdir -p -- "$TLS_DIR"
chmod 700 -- "$TLS_DIR"
TEMP_DIR=$(mktemp -d "$TLS_DIR/.generate.XXXXXX")
trap 'rm -rf -- "$TEMP_DIR"' EXIT
openssl req -x509 -newkey rsa:3072 -sha256 -nodes -days 365 \
    -subj '/CN=localhost' \
    -addext "subjectAltName=$SANS" \
    -addext 'basicConstraints=critical,CA:FALSE' \
    -addext 'keyUsage=critical,digitalSignature,keyEncipherment' \
    -addext 'extendedKeyUsage=serverAuth' \
    -keyout "$TEMP_DIR/key.pem" -out "$TEMP_DIR/cert.pem"
# Hard links fail rather than overwrite a certificate created concurrently.
ln -- "$TEMP_DIR/key.pem" "$KEY"
ln -- "$TEMP_DIR/cert.pem" "$CERT"
chmod 600 -- "$KEY"
chmod 644 -- "$CERT"
echo "Created self-signed HTTPS certificate in $TLS_DIR (valid for 365 days)."
openssl x509 -in "$CERT" -noout -ext subjectAltName -fingerprint -sha256
echo "Trust cert.pem on your client devices; keep key.pem private."
