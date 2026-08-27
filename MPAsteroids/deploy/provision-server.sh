#!/usr/bin/env bash
# Provisions an Ubuntu VM (e.g. Oracle Cloud Always Free) to host the
# MPAsteroids dedicated server + web client behind Caddy (automatic HTTPS).
#
# Usage, on the VM:
#   git clone https://github.com/tracotr/MPAsteroids.git
#   cd MPAsteroids/MPAsteroids/deploy
#   chmod +x provision-server.sh
#   sudo ./provision-server.sh
#
# Before running: point your subdomain's DNS A record at this VM's public
# IP, and open TCP 80 + 443 in the Oracle Cloud console's Security
# List/Network Security Group for this VM (the OS firewall is handled
# below, but Oracle also filters at the cloud network level separately).

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
make server

echo "==> Opening ports 80/443 in the OS firewall"
# Oracle's stock Ubuntu images use iptables (not ufw) with a REJECT rule at
# the end of INPUT. Inserting at the top guarantees these ACCEPT rules are
# evaluated before it, regardless of what else is in the chain.
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

cat <<DONE

==> Done. Remaining steps:
  1. DNS: point an A record for asteroids.jaxon-king.com at this VM's public IP.
  2. Oracle Cloud console: open ingress TCP 80 and 443 (source 0.0.0.0/0) on
     this VM's Security List / Network Security Group.
  3. On your dev machine, rebuild the web client pointed at this domain:
       make SERVER_HOST=asteroids.jaxon-king.com SERVER_PUBLIC_PORT=443 SERVER_PATH=/ws
     then copy the 4 output files here:
       scp mpasteroids.html mpasteroids.js mpasteroids.wasm mpasteroids.data \\
           $SERVICE_USER@<this-vm-ip>:/var/www/mpasteroids/
  4. Visit https://asteroids.jaxon-king.com — Caddy issues the HTTPS cert
     automatically on first request once DNS + firewall are in place.

Check the server logs anytime with: sudo systemctl status mpasteroids
                                     sudo journalctl -u mpasteroids -f
DONE
