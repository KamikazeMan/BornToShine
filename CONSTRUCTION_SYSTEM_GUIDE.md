# Born To Shine - Construction System Architecture

## 🎯 Overview

This is a sophisticated board-by-board construction system for Unreal Engine 5.3+ where sockets drive everything. Players build realistically, starting with foundations and progressing through each construction phase just like real construction.

## 📐 Core Architecture

### 1. Socket System (`SocketManager.h/cpp`)

The heart of the construction system. Every piece connects through named, typed sockets with compatibility rules.

**Key Features:**
- Named sockets with specific types (Foundation_Corner, RimBoard_Bottom_End, etc.)
- Compatibility rules define what can connect to what
- Snap distance and alignment checking
- Phase-aware connections (can only connect during proper construction phase)

**Socket Types:**
```cpp
Foundation_Corner        → Accepts rim board bottom ends
Foundation_Side          → Accepts rim board mid-span connections
RimBoard_Bottom_End      → Connects to foundation corners
RimBoard_Top_Face        → Accepts joist ends (16" or 24" OC)
RimBoard_Side_Face       → Accepts perpendicular joists
RimBoard_End_Corner      → Connects to other rim corners, first plywood corner
Joist_End                → Connects to rim board faces
Joist_Top_Face           → Accepts plywood edges
Plywood_Corner           → ONLY connects to rim corners (first sheet)
Plywood_Edge             → Connects to joists and other plywood sheets
```

### 2. Construction Phase Manager (`ConstructionPhaseManager.h/cpp`)

Enforces build order and tracks construction progress.

**Phases:**
1. **Foundation** - Place foundation blocks at 8ft intervals
2. **Floor Frame** - Install rim boards and joists
3. **Floor Sheathing** - Install plywood starting from corners
4. **Wall Frame** - Frame walls (not yet implemented)
5. **Wall Sheathing** - Install wall panels (not yet implemented)
6. **Roof Frame** - Install rafters (not yet implemented)
7. **Roof Sheathing** - Install roof panels (not yet implemented)

**Rules:**
- Cannot place rim boards until foundation blocks exist
- Cannot place joists until rim boards form perimeter
- Cannot place plywood until joists are installed
- Phase only advances when prerequisites are met

### 3. Buildable Piece Base Class (`BuildablePiece.h/cpp`)

Parent class for all construction pieces. Handles:

**Piece States:**
- `Preview` - Ghost mode, translucent, shows green (valid) or red (invalid)
- `Placed` - Piece is placed but not secured
- `Nailed` - Piece is locked in place (cannot be moved)

**Features:**
- Socket management (occupy/free sockets when connecting)
- Snap detection (finds nearest valid socket)
- Visual feedback (color changes based on validity)
- Rotation controls (left, right, pitch, roll)
- Scaling with mouse wheel
- Placement validation

### 4. Foundation Block (`FoundationBlock.h/cpp`)

The first piece players place.

**Features:**
- **8ft Grid Snapping** - Automatically snaps to 243.84cm intervals
- **9 Sockets:**
  - 4 corner sockets (NE, NW, SE, SW)
  - 1 center socket
  - 4 side sockets (N, S, E, W)
- **14cm Socket Height** - All sockets at 14cm from ground
- **Always Valid** - Foundation can be placed anywhere on terrain

**Socket Placement:**
```
     NW ---- N ---- NE
     |               |
     W    Center     E
     |               |
     SW ---- S ---- SE
```

### 5. Character Controller (`MoonshineCharacter.h/cpp`)

Player controller with build system integration.

**Features:**
- **Camera Modes:**
  - Third-person with spring arm (default)
  - First-person toggle (V key)

- **Build Mode:**
  - Toggle build mode (B key)
  - Spawn preview piece
  - Place piece (Left Click)
  - Cycle piece types (Q key)
  - Rotate piece (R key)
  - Scale with mouse wheel
  - Nail piece (E key - locks it)

- **Movement:**
  - WASD movement
  - Sprint (Shift)
  - Jump (Space)
  - Mouse look

## 🎮 Controls (To Be Configured in Unreal)

| Action | Key | Description |
|--------|-----|-------------|
| Move | WASD | Character movement |
| Look | Mouse | Camera control |
| Jump | Space | Jump |
| Sprint | Shift | Sprint |
| Toggle Camera | V | Switch 1st/3rd person |
| Toggle Build Mode | B | Enter/exit build mode |
| Place Piece | Left Click | Place current preview piece |
| Cycle Piece | Q | Switch between piece types |
| Rotate Yaw | R | Rotate left/right |
| Rotate Pitch | T/G | Rotate front/back |
| Rotate Roll | Mouse X (in rotate mode) | Roll piece |
| Scale | Mouse Wheel | Scale piece up/down |
| Nail Piece | E | Lock piece in place |

## 🔧 What's Been Built

### ✅ Complete
1. **Core Type System** - All enums, structs, and data types
2. **Socket Manager** - Full compatibility system
3. **Phase Manager** - Build order enforcement
4. **Base Piece Class** - Complete with snapping, validation, visual feedback
5. **Foundation Block** - With 9 sockets and 8ft grid snapping
6. **Character Controller** - Third/first person, build mode integration
7. **Game Mode** - Manager initialization
8. **Visual Feedback** - Green (valid) / Red (invalid) / Yellow (placed) / Wood (nailed)

### 🚧 To Be Created in Unreal Editor

You'll need to set up these in the Unreal Editor:

1. **Materials**
   - Create a master material with BaseColor parameter
   - Create material instances for wood, concrete, etc.
   - Set up translucent materials for preview mode

2. **Static Meshes**
   - Foundation block mesh (2ft x 2ft x 1ft)
   - Rim board mesh (2x8 lumber)
   - Floor joist mesh (2x8 or 2x10)
   - Plywood mesh (4ft x 8ft sheet)

3. **Input Mapping**
   - Create Enhanced Input actions in Content Browser
   - Set up Input Mapping Context
   - Bind keys to actions
   - Assign to character in DefaultInput.ini or project settings

4. **Blueprints**
   - Create BP_FoundationBlock (parent: AFoundationBlock)
   - Assign static mesh and materials
   - Set socket positions visually
   - Create BP_Character (parent: AMoonshineCharacter)
   - Create BP_GameMode (parent: ABornToShineGameMode)

5. **Level Setup**
   - Create MainLevel
   - Place player start
   - Set world settings game mode to BP_GameMode
   - Add lighting and landscape

## 🔜 Next Steps - Rim Boards

When you're ready to implement rim boards, create:

### RimBoard.h/cpp
```cpp
class ARimBoard : public ABuildablePiece
{
    // Features:
    // - 2x8 lumber (actual size: 1.5" x 7.25")
    // - Bottom end sockets to connect to foundation
    // - Top face sockets at 16" intervals for joists
    // - Side face sockets for perpendicular joists
    // - End corner sockets for rim-to-rim and first plywood
    // - Must maintain flush corners with other rim boards
    // - Length scalable (8ft, 10ft, 12ft, 16ft)
};
```

**Critical Rim Board Rules:**
1. Rim boards MUST connect to foundation corners
2. Rim boards must form a closed perimeter (each connects to 2 others)
3. Corners must be flush (90-degree alignment)
4. Top face must be level for joist placement
5. Joist sockets should be evenly spaced (16" or 24" OC)

## 🏗️ Construction Workflow

### Phase 1: Foundation
1. Player enters build mode (B key)
2. Select foundation block (Q to cycle)
3. Position shows ghost preview
4. Green when valid, red when invalid
5. Click to place
6. Foundation snaps to 8ft grid automatically
7. Place minimum 4 blocks in rectangular pattern
8. System detects complete foundation
9. Phase advances to Floor Frame

### Phase 2: Floor Frame (Next to implement)
1. Select rim board
2. Snap first rim board to foundation corner
3. Snap second rim board to adjacent corner
4. Continue until perimeter is closed
5. Select floor joist
6. Snap joists between opposite rim boards
7. Joists auto-space at 16" intervals
8. Phase advances when frame complete

### Phase 3: Floor Sheathing (Future)
1. Select plywood
2. **CRITICAL:** First sheet MUST snap corner-to-corner with rim
3. Subsequent sheets snap edge-to-edge or to joists
4. Continue until floor is covered
5. Phase advances to Wall Frame

## 🎨 Visual Feedback System

The system provides real-time visual feedback:

| Color | State | Meaning |
|-------|-------|---------|
| Green (translucent) | Preview - Valid | Piece can be placed here |
| Red (translucent) | Preview - Invalid | Cannot place (wrong phase, no support, etc.) |
| Yellow | Placed | Piece placed but not nailed |
| Wood Color | Nailed | Piece locked in place |

**Validation Checks:**
- ✓ Correct construction phase
- ✓ Prerequisites met (e.g., foundation exists before rim boards)
- ✓ Valid socket connection found
- ✓ Proper alignment and snap distance
- ✓ Not floating (gravity check)
- ✓ No collision with other pieces

## 🔍 Debug/Testing

The system logs extensively for debugging:

```cpp
UE_LOG(LogTemp, Log, TEXT("SocketManager: Initialized %d compatibility rules"), ...);
UE_LOG(LogTemp, Log, TEXT("Advanced to Floor Frame phase"));
UE_LOG(LogTemp, Log, TEXT("Piece placed successfully"));
UE_LOG(LogTemp, Warning, TEXT("Cannot place rim board - no foundation blocks exist"));
```

Watch the Output Log in Unreal Editor for real-time feedback.

## 📋 Socket Compatibility Matrix

| Source Socket | Compatible With | Phase Required |
|--------------|----------------|----------------|
| Foundation_Corner | RimBoard_Bottom_End | FloorFrame |
| Foundation_Side | RimBoard_Bottom_End | FloorFrame |
| RimBoard_Bottom_End | Foundation_Corner, Foundation_Side | FloorFrame |
| RimBoard_Top_Face | Joist_End | FloorFrame |
| RimBoard_Side_Face | Joist_End | FloorFrame |
| RimBoard_End_Corner | RimBoard_End_Corner, Plywood_Corner | FloorFrame |
| Joist_End | RimBoard_Top_Face, RimBoard_Side_Face | FloorFrame |
| Joist_Top_Face | Plywood_Edge | FloorSheathing |
| Plywood_Corner | RimBoard_End_Corner | FloorSheathing |
| Plywood_Edge | Joist_Top_Face, Plywood_Edge | FloorSheathing |

## 🚀 Compiling the Project

1. **Open in Unreal Editor:**
   - Right-click `BornToShine.uproject`
   - Select "Generate Visual Studio project files"
   - Open the .sln file in Visual Studio or your IDE

2. **Build:**
   - Build > Build Solution (or Ctrl+Shift+B)
   - Or use Unreal Editor's "Compile" button

3. **Run:**
   - Press F5 in Visual Studio or
   - Click "Play" in Unreal Editor

## 🐛 Common Issues & Solutions

### "No piece types available for building"
- **Solution:** In Unreal Editor, open BP_Character, add BP_FoundationBlock to AvailablePieceTypes array

### "Cannot place rim board - no foundation blocks exist"
- **Solution:** Place foundation blocks first! Phase order is enforced.

### "Socket Manager not found"
- **Solution:** Ensure game mode is set correctly in World Settings

### Preview piece not showing
- **Solution:** Check material setup, ensure translucent materials are assigned

### Snapping not working
- **Solution:** Check snap search radius (default 500cm), ensure sockets are initialized

## 📚 Code Structure

```
BornToShine/
├── Source/BornToShine/
│   ├── ConstructionTypes.h          # All enums and structs
│   ├── SocketManager.h/cpp          # Socket compatibility system
│   ├── ConstructionPhaseManager.h/cpp  # Build order enforcement
│   ├── BuildablePiece.h/cpp         # Base class for all pieces
│   ├── FoundationBlock.h/cpp        # Foundation implementation
│   ├── MoonshineCharacter.h/cpp     # Player controller
│   ├── BornToShineGameMode.h/cpp    # Game initialization
│   └── BornToShine.Build.cs         # Build configuration
├── Config/
│   ├── DefaultEngine.ini            # Engine config
│   └── DefaultInput.ini             # Input mappings
└── Content/                         # (Create in Unreal Editor)
    ├── Blueprints/
    │   ├── BP_FoundationBlock
    │   ├── BP_Character
    │   └── BP_GameMode
    ├── Materials/
    ├── Meshes/
    └── Input/
```

## 💡 Design Philosophy

This system follows real construction principles:

1. **Socket-Driven:** Everything connects through named sockets
2. **Phase-Enforced:** Can't skip ahead in construction
3. **Prerequisite-Based:** Need foundations before walls
4. **Context-Aware:** What you can place depends on current phase
5. **Validation-First:** System prevents invalid placements
6. **Visual-Feedback:** Player always knows if placement is valid
7. **Board-by-Board:** No pre-fab buildings, everything is placed individually
8. **Realistic Constraints:** Must align, snap, and support like real construction

## 🎯 Project Goals

- ✅ Realistic construction simulation
- ✅ Socket-based connection system
- ✅ Phase-enforced build order
- ✅ Visual feedback for placement
- ✅ Grid snapping for foundations
- 🚧 Complete building types (rim boards, joists, plywood, walls, roof)
- 🚧 Material/inventory system
- 🚧 Nail animation and audio
- 🚧 Measuring tape tool
- 🚧 Undo/redo system
- 🚧 Save/load construction progress

---

**Ready to build your moonshine empire! 🏚️🥃**

Start with foundations, then move to rim boards when you're ready. Each phase builds on the last, just like real construction!
