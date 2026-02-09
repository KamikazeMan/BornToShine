# Born To Shine 🏚️🥃

A realistic construction simulation game built in Unreal Engine 5 with C++.

## Overview

Build your moonshine empire from the ground up - literally! This game features a sophisticated board-by-board construction system where every piece connects through named sockets, just like real construction.

## Features

- ✅ **Socket-Driven Construction** - Everything connects through compatibility-checked sockets
- ✅ **Phase-Enforced Building** - Foundation → Floor Frame → Floor Sheathing → Walls → Roof
- ✅ **8ft Grid Snapping** - Foundation blocks snap to realistic grid intervals
- ✅ **Visual Feedback** - Green (valid) / Red (invalid) placement indicators
- ✅ **Third/First Person Camera** - Toggle between camera modes
- ✅ **Piece Manipulation** - Rotate and scale pieces before placing
- 🚧 **Rim Boards** - Coming next!
- 🚧 **Floor Joists** - Coming soon!
- 🚧 **Plywood Sheathing** - Coming soon!

## Quick Start

See **[QUICK_START.md](QUICK_START.md)** for setup instructions.

## Documentation

- **[CONSTRUCTION_SYSTEM_GUIDE.md](CONSTRUCTION_SYSTEM_GUIDE.md)** - Complete architecture and implementation guide
- **[QUICK_START.md](QUICK_START.md)** - Setup and testing guide

## Current Status

**Phase 1 Complete:** Foundation System
- ✅ Core socket architecture
- ✅ Construction phase manager
- ✅ Foundation blocks with 9 sockets
- ✅ Grid snapping system
- ✅ Character controller with build mode
- ✅ Visual feedback system

**Next Up:** Rim Boards (2x8 lumber with socket connections)

## Tech Stack

- Unreal Engine 5.3+
- C++ with Blueprint integration
- Enhanced Input System
- Custom socket-based construction system

## Controls (Default)

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look around |
| V | Toggle camera (1st/3rd person) |
| B | Toggle build mode |
| Left Click | Place piece |
| Q | Cycle piece types |
| E | Nail piece (lock it) |
| Mouse Wheel | Scale piece |

## Building From Source

1. Generate Visual Studio project files (right-click .uproject)
2. Open BornToShine.sln
3. Build solution (Ctrl+Shift+B)
4. Open BornToShine.uproject in Unreal Editor
5. Follow QUICK_START.md for editor setup

## License

All rights reserved.

---

**Let's build something special! 🏗️**
