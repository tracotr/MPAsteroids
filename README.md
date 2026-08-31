# MP-Asteroids
![](mpasteroids.png)
- https://tracotr.itch.io/mpasteroids
- A simple 3D multiplayer asteroids game built in C++ using [raylib](www.raylib.com/) for rendering, running in the browser via WebAssembly and WebSockets.

The game is web now, go play it on the [itchio](https://tracotr.itch.io/mpasteroids) or my [website](www.jaxon-king.com) 

## Controls

Mouse and keyboard both work at the same time. Click once to capture the
pointer, and press Escape to release it.

- Mouse : Pitch and yaw
- Left click : Shoot
- W / S : Thrust forwards / backwards
- A / D : Yaw
- R / F : Pitch
- Q / E : Roll
- Space : Shoot
- Shift : Fine turn rate, for both mouse and keys
- Y : Respawn


## Project structure

```
MPAsteroids/
├── Makefile                    make web  /  make server
├── shell.html                  page template the client is embedded into
├── lib/libraylib.web.a         prebuilt raylib for the WebAssembly build
│
├── src/
│   ├── main.cpp                client entry point
│   ├── GameApp.cpp             window, camera, connect/retry, main loop
│   ├── World.cpp               game loop: projectiles, collisions, drawing
│   ├── Player.cpp              local ship input, movement and respawning
│   ├── Entity.cpp              shared position/rotation base
│   ├── Models.cpp              model and shader loading, HUD drawing
│   ├── Sounds.cpp              positional audio
│   ├── Names.cpp               random player names, read from resources
│   ├── NetClient.cpp           client half of the protocol
│   │
│   ├── server_main.cpp         server entry point
│   ├── ServerHost.cpp          asteroid simulation, scoring, world regions
│   ├── WebSocketServer.cpp     minimal RFC 6455 implementation
│   │
│   └── include/
│       ├── networking/NetConstants.h   packet layouts and tuning constants
│       └── raylib/                     raylib headers
│
├── resources/                  bundled into the client at build time
│   ├── models/                 ship and asteroid meshes and textures
│   ├── shaders/                skybox and cubemap, GLSL 100 and 330
│   ├── sounds/                 lasers, explosions, thrusters
│   ├── skybox/
│   └── names.txt               word lists the random names are built from
│
└── deploy/
    ├── Caddyfile               serves the client, proxies /ws to the server
    └── provision-server.sh     one-time host setup
```
