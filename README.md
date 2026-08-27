# MP-Asteroids
![](mpasteroids.png)
- https://tracotr.itch.io/mpasteroids
- A simple 3D multiplayer asteroids game built in C++ using [raylib](www.raylib.com/) for rendering, running in the browser via WebAssembly and WebSockets.

The game is now web-only: players open the page, get an automatically generated
name, and join the shared server immediately. There is no menu and no desktop
client — a separate headless server binary hosts the session.

## Building

### Web client
Run inside `MPAsteroids/MPAsteroids` (the folder with the Makefile):

```bash
make SERVER_HOST=yourdomain.com
```

Outputs `mpasteroids.html`, `.js`, `.wasm` and `.data`. `SERVER_HOST` defaults to
`127.0.0.1` for local testing; set it to wherever your server runs. The client
uses `wss://` automatically when the page itself is served over HTTPS.

Behind a reverse proxy, the browser dials the public HTTPS port while the server
binds a private local one, so set all three:

```bash
make web SERVER_HOST=asteroids.example.com SERVER_PUBLIC_PORT=443 SERVER_PATH=/ws
```

### Pointing a build at a different server
The compile-time address is only a default. Without rebuilding, you can override
it per-page — handy when the server sits behind a tunnel whose hostname changes:

- `?server=example.com` — query string on the game URL
- `?server=example.com:9000` or `?server=wss://example.com/ws` — port or full URL
- `window.MPASTEROIDS_SERVER = "example.com"` — set in the page before the module loads

Requires the Emscripten SDK (expected at `C:/raylib/emsdk`, override with
`EMSDK_PATH`) and `lib/libraylib.web.a`.

### Dedicated server
```bash
make server
```

Outputs `mpasteroids-server`. It has no raylib or graphics dependency, so it
builds and runs headless on a small Linux box.

## Hosting
1. Run `mpasteroids-server` on your host (port `25665` by default, override with
   `SERVER_PORT`). Keep it running under systemd/supervisor.
2. Put a reverse proxy (nginx/Caddy) in front of it to terminate HTTPS and
   forward `wss://` traffic to the server's local `ws://` port. Browsers block
   insecure `ws://` connections from an HTTPS page, so this step is required
   once your site is on HTTPS.
3. Upload the four `mpasteroids.*` files to any static host (GitHub Pages, your
   existing site). Embed it with an `<iframe>` or link straight to the page —
   the canvas fills whatever space the embedding page gives it.

## Controls
- WASD : Tilt
- QE : ROLL
- R : Forwards
- F : Backwards
- Space : Shoot
- Shift : Slow turn rate
