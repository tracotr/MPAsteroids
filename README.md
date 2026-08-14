# MP-Asteroids

- https://tracotr.itch.io/mpasteroids
- A simple 3D multiplayer asteroids game built in C++ using [raylib](www.raylib.com/) for rendering and [zpl-c/enet](https://github.com/zpl-c/enet) for networking.

## Features
- A server and a client.
- It's like somehow simpler asteroids, but 3d and multiplayer.
![](https://github.com/tracotr/MPAsteroidsRaylib/blob/main/Examples/example.gif)

## Building
- Open project in VSCode with the makefile extension and hit f5.

## Playing with others
- Run the server first - it listens on UDP port 25665 on all interfaces.
- Launch the client. On the connect screen, type the server's address and press ENTER (defaults to `127.0.0.1` for solo/local testing).
- **Same network:** use the host's local IP (e.g. `192.168.x.x`).
- **Over the internet:** the host needs to forward UDP port 25665 to their machine, or you can skip that entirely by putting everyone on a VPN mesh like [Hamachi](https://vpn.net/) or [ZeroTier](https://www.zerotier.com/) and connecting to that IP instead.
- If it can't connect within a few seconds, you'll get bounced back to the menu with a reason - double check the address and that the server is actually running.

## Controls
- WASD : Tilt
- QE : ROLL
- R : Forwards
- F : Backwards
- Space : Shoot
