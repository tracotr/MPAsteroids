# MPAsteroids

Browser game: a WebAssembly client talks to a headless Linux server over
WebSockets. Everything lives under `MPAsteroids/`, including the Makefile.

## Commits

Do not add `Co-Authored-By` trailers to commit messages.

## Building

The Makefile is in `MPAsteroids/`, not the repo root, so build with `-C`:

```
make -C MPAsteroids web        # WebAssembly client, needs emsdk on PATH
make -C MPAsteroids server     # headless server, plain g++
```

Three things that waste time otherwise:

- **Overriding `SERVER_PATH` from Git Bash needs `MSYS_NO_PATHCONV=1`.** Without
  it, `/ws` is rewritten to a Windows path and gets baked into the client, which
  then dials a nonsense URL. The Makefile mentions this too.
- **`SERVER_PORT` and `SERVER_PUBLIC_PORT` are different numbers in production.**
  The server binds 25665; the browser dials 443, where Caddy forwards `/ws` to
  it. Set both explicitly rather than letting one default from the other.
- **Stop the running server before rebuilding it.** Windows will not let the
  linker overwrite a running `.exe`, and the failure reads as a generic
  `collect2: ld returned 1 exit status`.

## Deploying

`.github/workflows/deploy.yml` builds the client, ships it to the host, rebuilds
the server there, and restarts the service. It needs the `DEPLOY_HOST`,
`DEPLOY_USER` and `DEPLOY_SSH_KEY` repo secrets.

**The client and server must deploy together.** They share a packet layout, so a
client built against a different version of `NetConstants.h` cannot read the
server's asteroid updates at all. Never ship one half on its own.

## Testing

There is no test suite. The server can be driven over WebSockets from a Node
script, which is the practical way to check protocol and scoring behaviour
without a browser: connect, send a `PlayerPacket`, and read what comes back.

The in-app browser pane cannot be screenshotted or clicked while it is hidden,
so anything that needs real input or a look at the screen has to be checked in a
normal browser window.
