# x11 mouse funnel
**x11 mouse funnel** is a lightweight, high-performance background utility for X11 that solves the "dead corner" problem on multi-monitor setups with different resolutions or physical sizes.

If your mouse gets stuck on the edge of a larger screen because the adjacent screen is smaller, this tool will "funnel" your cursor to the correct relative position on the neighbor screen.

## Features
*   **Seamless Transitions**: Automatically warps the mouse to the adjacent screen when you push against a "dead" edge.
*   **Proportional Mapping**: Intelligently scales your cursor position. If you are at the bottom 10% of a 4K screen, you will arrive at the bottom 10% of a 1080p screen (instead of hitting a wall or landing in the middle).
*   **Zero Polling Lag**: Uses `XInput2` raw motion events instead of polling, ensuring instant response times and near-zero CPU usage when idle.
*   **Automatic Detection**: Uses XRandR to detect your monitor layout and updates automatically if you change resolutions.

## Prerequisites
You need a Linux system running X11 (this does not work on Wayland) and the standard X11 development libraries.

On **Arch Linux**:
```bash
sudo pacman -S gcc libx11 libxi libxrandr
```
On **Debian/Ubuntu**:
```bash
sudo apt install build-essential libx11-dev libxi-dev libxrandr-dev
```

## Compilation
Clone this repository or download the `mouse_funnel.c` file, then compile it with `gcc`:
```bash
gcc -O2 -o mouse_funnel mouse_funnel.c -lX11 -lXi -lXrandr
```

## Usage
### Test it
Run the binary manually to test it out:
```bash
./mouse_funnel
```
Try moving your mouse between your mismatched screens. You should now be able to cross boundaries that were previously blocked walls.

### Auto-start
To have it run automatically in the background when you log in, add it to your startup script (e.g., `.xinitrc`, `.xprofile`, or your Window Manager config).

#### For .xinitrc / .xprofile:
```Bash
/path/to/mouse_funnel &
```

#### For i3 / Sway (X11 backend):
```config
exec_always --no-startup-id /path/to/mouse_funnel
```

## How it works
Standard X11 configuration creates a rectangular bounding box around all monitors. If you have a large monitor next to a small one, this creates "voids" where no monitor exists. X11 normally stops the cursor from entering these voids, effectively creating an invisible wall.

**x11 mouse funnel** listens for raw mouse input. When it detects you are pushing against a screen edge:
1. It calculates which monitor you are trying to reach.
2. It calculates your relative position (e.g., "80% down the screen").
3. It instantly warps your pointer to the corresponding relative position (80% down) on the target monitor, bypassing the void.