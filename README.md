# Diamond Spiral (q2-zuma)

Original, lightweight Zuma-style marble-chain shooter for the Shanling Q2 running DiamondBond's Q2 Pod firmware. No original Zuma artwork, code, audio, or trademarks are used in gameplay. **Working title in the device menu: Diamond Spiral.**

## How to play

A colorful marble chain travels along a winding track toward the exit. Turn the wheel to aim the launcher in the center, then press Center to fire a marble. Create three or more adjacent marbles of the same color to remove them, earn points, and push the chain back. Clear the chain to win; if it reaches the exit, the game ends.

* Wheel: rotate launcher.
* Center: start, fire, resume, or retry.
* Touch: tap to aim; tap again in the same direction to fire.
* Return/Back: pause; press Back again while paused to exit.
* Best score saved on the SD card.

This is an initial playable vertical slice. Planned refinements include multiple levels, chain smoothing, combo effects, color-selection assistance, and physical-device usability adjustments.

## Install

Download **q2-zuma.zip** from [Releases](https://github.com/lampsonlnguyen/q2-zuma/releases) or a successful Actions build. Extract the ZIP *directly* to the microSD root. Confirm:

```
.zuma/q2app
.zuma/q2-zuma
```

Open **Library → Games → Diamond Spiral**. Requires Q2 Pod firmware supporting external games, not stock Shanling firmware.

## Build

Ubuntu/WSL: install `clang lld make gcc zip unzip`. Then:

```sh
make check
make
```

The output is `dist/q2-zuma.zip`. CI tests the core and cross-compiles the MIPS executable on push and PR. Main-branch updates automatically publish a `latest` Release, and version tags `v*` create versioned releases.

## Design

`src/app.c` provides original chain behavior, projectile movement, collisions, line/ball drawing, simple UI, and score handling; no malloc, libm, texture decoding or audio device takeover. `src/platform_q2.c` adapts framebuffer, input socket, clock, and persistent-score operations from the GPL Q2 Diamond Says port, which derives its Q2 runtime from DiamondBond's portable game examples.

The target framebuffer is 375×320, and the renderer accounts for Q2's unusual rotated memory layout. The game is a freestanding native little-endian MIPS32 card app, not an emulator.

**Status:** CI build and unit tests are necessary but not sufficient. Verify wheel direction, touch coordinate orientation, performance, and display layout on a physical Q2.
