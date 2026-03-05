# Rim Board (2x6) Setup Guide

## Overview

The **RimBoard** class is now implemented! This represents 2x6 lumber (actual dimensions: 1.5" x 5.5") that forms the perimeter of your floor frame.

---

## What's Been Created

### Files Added:
- **RimBoard.h** - Rim board class definition with socket system
- **RimBoard.cpp** - Implementation with 2x6 dimensions and socket generation

### Files Updated:
- **ConstructionTypes.h** - Updated display name to "Rim Board (2x6)"

---

## Rim Board Specifications

### Dimensions:
- **Width**: 3.81 cm (1.5 inches)
- **Height**: 13.97 cm (5.5 inches)
- **Length**: 243.84 cm (8 feet) - default, configurable

### Socket System:

The rim board generates sockets automatically:

1. **Bottom End Sockets (2)**:
   - One at each end of the board
   - Connect to foundation corner sockets
   - Named: `BottomEnd_Left`, `BottomEnd_Right`

2. **Top Face Sockets (Variable)**:
   - At 16" or 24" intervals along the top
   - For floor joists to connect
   - Named: `TopFace_0`, `TopFace_1`, etc.
   - Default: 16" OC (40.64 cm)

3. **Side Face Sockets (Variable)**:
   - Match the top face socket positions
   - For perpendicular joists that butt into the rim
   - Named: `SideFace_Left_0`, `SideFace_Right_0`, etc.

4. **End Corner Sockets (8)**:
   - Four corners at each end (top-left, top-right, bottom-left, bottom-right)
   - For 90-degree rim-to-rim connections
   - For first plywood sheet corner snapping
   - Named: `EndCorner_Left_TopLeft`, `EndCorner_Right_BottomRight`, etc.

---

## How to Use

### Step 1: Open Unreal Editor

1. Open your BornToShine project in Unreal Editor
2. The project will automatically compile the new C++ classes
3. Wait for compilation to complete (Hot Reload)

### Step 2: Create BP_RimBoard Blueprint

1. **Right-click in Content Browser** → Blueprint Class
2. **Search for**: `RimBoard`
3. **Select**: RimBoard (C++ class)
4. **Name it**: `BP_RimBoard`
5. **Open** the blueprint

### Step 3: Configure the Blueprint

1. **Select the MeshComponent** in Components panel

2. **Assign a mesh**:
   - Use Engine Content cube for now
   - Scale will be auto-adjusted to 2x6 dimensions
   - Later: Create/import proper 2x6 lumber mesh

3. **In Details panel, find Materials**:
   - **Element 0**: Set to `M_PreviewPiece` (for ghost preview)

4. **In Details, find Construction → Materials**:
   - **Nailed Material**: Select `M_Wood` (or your wood material)
   - **Auto Nail On Place**: ❌ **Leave unchecked** (wood gets nailed manually)

5. **Compile and Save**

### Step 4: Add to Available Pieces

1. **Open BP_Character_Simple** (or your character blueprint)

2. **Select BuildingComponent** in Components panel

3. **In Details, find Available Piece Types**:
   - **Element 0**: BP_FoundationBlock (already there)
   - **Element 1**: BP_RimBoard (add this)

4. **Compile and Save**

### Step 5: Test in Game

1. **Play** the game

2. **Press B** to enter build mode

3. **Press Q** to cycle to rim board (will be piece type 2)

4. **Position rim board near foundation corner**:
   - Should snap to foundation top
   - Bottom should sit flush at 44.48 cm height (foundation top)
   - End should align with foundation corner socket

5. **Left-click** to place

6. **Piece turns yellow** (waiting to be nailed)

7. **Press E** to nail - shows wood material!

---

## Socket Compatibility

The rim board automatically connects to:

✅ **Foundation corners** - bottom end sockets snap to foundation top corners
✅ **Foundation sides** - for mid-span support
✅ **Other rim boards** - end corners connect at 90 degrees for flush corners
✅ **Floor joists** (when implemented) - top and side sockets accept joist ends
✅ **Plywood corners** (when implemented) - end corners guide first plywood sheet

---

## Configuration Options

### In Blueprint or C++:

| Property | Description | Default |
|----------|-------------|---------|
| **Board Width** | 1.5" actual | 3.81 cm |
| **Board Height** | 5.5" actual | 13.97 cm |
| **Board Length** | Configurable | 243.84 cm (8ft) |
| **Joist Spacing** | OC spacing | 40.64 cm (16") |
| **Use 24 Inch Spacing** | Switch to 24" OC | false (16" default) |

### To Change Joist Spacing:

In BP_RimBoard Details:
- **Use 24 Inch Spacing**: ✅ Check for 24" OC
- **Joist Spacing**: Auto-updates (40.64 for 16", 60.96 for 24")

---

## Typical Construction Workflow

### Building a Floor Frame:

1. **Place foundation blocks** (8ft x 8ft grid)
2. **Cycle to rim boards** (Q key)
3. **Place first rim board** - snaps to foundation corner
4. **Rotate and place perpendicular rim board** - creates 90-degree corner
5. **Continue around perimeter** - all four sides
6. **Nail rim boards** (E key) - shows wood material
7. **Later: Add floor joists** between rim boards at 16" OC
8. **Later: Add plywood** - first corner snaps to rim board corner

---

## Material Setup

### Ghost Preview (Before Placing):
- Material: **M_PreviewPiece**
- Color: Green (valid) or Red (invalid)
- Translucent

### Placed (After Clicking):
- Material: **M_PreviewPiece** with yellow color
- Waiting to be nailed
- Semi-transparent

### Nailed (After E Key):
- Material: **M_Wood** (or your custom wood material)
- Locked in place
- Fully opaque
- Can't be removed

---

## Socket Generation Details

### How Top Face Sockets Are Calculated:

For an 8ft (243.84 cm) rim board with 16" spacing:
- Start offset: 40.64 cm from left end
- Socket positions: Every 40.64 cm
- Example: -121.92, -81.28, -40.64, 0.0, 40.64, 81.28, 121.92 cm

This ensures joists are centered at proper intervals!

### Corner Socket Alignment:

End corner sockets are positioned at exact corners:
- Top-left: (-BoardLength/2, -BoardWidth/2, BoardHeight/2)
- Top-right: (-BoardLength/2, BoardWidth/2, BoardHeight/2)
- Bottom-left: (-BoardLength/2, -BoardWidth/2, -BoardHeight/2)
- Bottom-right: (-BoardLength/2, BoardWidth/2, -BoardHeight/2)

Same pattern for right end, but positive X coordinate.

---

## Smart Snapping Behavior

The rim board uses intelligent placement:

1. **Near foundation**: Snaps to foundation top (44.48 cm height)
2. **Near other rim boards**: Aligns for flush 90-degree corners
3. **Free placement**: Falls back to 44.48 cm height if no snap point found

This ensures rim boards always sit at the correct height on the foundation!

---

## Next Steps

After testing rim boards:

1. **Create floor joists** (2x10 or 2x12)
   - Connect to rim board top sockets
   - Span between parallel rim boards
   - 16" or 24" on-center spacing

2. **Create plywood sheathing** (4x8 sheets)
   - First corner snaps to rim board corner
   - Subsequent sheets snap edge-to-edge
   - Cover entire floor frame

3. **Wall framing** (2x4 or 2x6 studs)
   - Bottom plate connects to floor
   - Build up from there

---

## Troubleshooting

### Rim board won't snap to foundation:
- Make sure you're in **FloorFrame** construction phase
- Foundation must be **nailed** (permanent)
- Socket compatibility requires correct phase
- Try getting closer (snap distance is 50 cm)

### Ghost preview not showing:
- Make sure M_PreviewPiece is assigned to Element 0
- Check material has BaseColor parameter
- Verify material is Translucent

### Board shows concrete instead of wood when nailed:
- Uncheck "Auto Nail On Place" in Blueprint
- Set Nailed Material to M_Wood (not M_Concrete)
- Recompile blueprint

### Wrong height (floating or underground):
- Rim board auto-adjusts to 44.48 cm (foundation top height)
- Check UpdatePreviewPosition() if issues persist

---

## Technical Details

### Socket Compatibility Rules (from SocketManager):

**RimBoard_Bottom_End** connects to:
- Foundation_Corner ✅
- Foundation_Side ✅
- Required phase: FloorFrame
- Snap distance: 50 cm

**RimBoard_Top_Face** connects to:
- Joist_End ✅
- Required phase: FloorFrame
- Snap distance: 40 cm

**RimBoard_Side_Face** connects to:
- Joist_End ✅
- Required phase: FloorFrame
- Snap distance: 40 cm

**RimBoard_End_Corner** connects to:
- RimBoard_End_Corner ✅ (rim-to-rim)
- Plywood_Corner ✅ (first plywood sheet)
- Required phase: FloorFrame
- Snap distance: 30 cm

---

## Summary

✅ 2x6 rim boards implemented
✅ Correct lumber dimensions (1.5" x 5.5")
✅ Full socket system (bottom, top, side, corners)
✅ Snaps to foundation
✅ Connects to other rim boards at 90 degrees
✅ Ready for floor joists (when implemented)
✅ Material system (ghost → yellow → wood)
✅ Manual nailing (press E key)

**Your floor framing system is taking shape!** 🏗️

Next: Floor joists spanning between rim boards!
