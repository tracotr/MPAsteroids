# MP-Asteroids

- https://tracotr.itch.io/mpasteroids
- A simple 3D multiplayer asteroids game built in C++ using [raylib](www.raylib.com/) for rendering and [zpl-c/enet](https://github.com/zpl-c/enet) for networking.

## Building
- Download the Makefile [make](https://stackoverflow.com/questions/32127524/how-can-i-install-and-use-make-in-windows) command
- run ```make``` inside of ```MPAsteroids/MPAsteroids``` (in the folder with the Makefile)  

## Playing with others
- Run the game, and click host.
- You will need to port forward port ```25665``` for UDP connections
- Use the host computers local IP to connect on the join screen.
- It's possible to use hamachi or zero tier as well.

## Controls
- WASD : Tilt
- QE : ROLL
- R : Forwards
- F : Backwards
- Space : Shoot
