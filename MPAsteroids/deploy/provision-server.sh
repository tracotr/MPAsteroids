#!/usr/bin/env bash

# Usage to host (ubuntu)
#   sudo apt update && sudo apt install -y git
#   git clone https://github.com/tracotr/MPAsteroids.git
#   cd MPAsteroids/MPAsteroids/deploy
#   chmod +x provision-server.sh
#   sudo ./provision-server.sh
#   open TCP 80 + 443 
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVICE_USER="${SUDO_USER:-$(whoami)}"

echo "==> Installing build tools"
apt-get update
apt-get install -y build-essential git curl debian-keyring debian-archive-keyring apt-transport-https

echo "==> Installing Caddy (if not already present)"
if ! command -v caddy >/dev/null 2>&1; then
    curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/gpg.key' | gpg --dearmor -o /usr/share/keyrings/caddy-stable-archive-keyring.gpg
    curl -1sLf 'https://dl.cloudsmith.io/public/caddy/stable/debian.deb.txt' | tee /etc/apt/sources.list.d/caddy-stable.list
    apt-get update
    apt-get install -y caddy
fi

echo "==> Building the dedicated server (headless, no graphics deps)"
cd "$REPO_DIR"

# SERVER_PORT is the port the game server binds, and has to match the
# reverse_proxy line in deploy/Caddyfile. The Makefile default is the public
# HTTPS port, which Caddy needs for itself, so it is overridden here.
make server SERVER_PORT=25665

echo "==> Opening ports 80/443 in the OS firewall"
for PORT in 80 443; do
    if ! iptables -C INPUT -p tcp --dport "$PORT" -j ACCEPT 2>/dev/null; then
        iptables -I INPUT -p tcp --dport "$PORT" -j ACCEPT
    fi
done
netfilter-persistent save 2>/dev/null || true

echo "==> Installing systemd service for the game server"
cat > /etc/systemd/system/mpasteroids.service <<SERVICE
[Unit]
Description=MPAsteroids dedicated game server
After=network.target

[Service]
ExecStart=$REPO_DIR/mpasteroids-server
WorkingDirectory=$REPO_DIR
Restart=always
RestartSec=3
User=$SERVICE_USER

[Install]
WantedBy=multi-user.target
SERVICE

systemctl daemon-reload
systemctl enable --now mpasteroids

echo "==> Setting up the static web root and Caddy config"
mkdir -p /var/www/mpasteroids
chown -R "$SERVICE_USER":"$SERVICE_USER" /var/www/mpasteroids
chmod -R o+rX /var/www/mpasteroids
cp "$REPO_DIR/deploy/Caddyfile" /etc/caddy/Caddyfile
systemctl reload caddy 2>/dev/null || systemctl restart caddy
