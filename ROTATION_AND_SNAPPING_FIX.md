# Fix: Rim Board Snapping and Rotation Issues

## Problems Fixed

### 1. ✅ Rotation Not Working
**Problem**: Arrow keys didn't rotate the rim board preview
**Cause**: Rotation input binding was missing from SetupPlayerInputComponent
**Fix**: Added rotation input binding and OnRotate() method

### 2. ✅ Rim Board Not Snapping to Foundation
**Problem**: Rim board stayed red and wouldn't snap to foundation blocks
**Cause**: Game starts in "Foundation" phase - only foundation blocks can be placed
**Fix**: Added phase advancement system to transition from Foundation → FloorFrame phase

---

## Files Modified

- `MoonshineCharacter_Simple.h` - Added OnRotate() and OnAdvancePhase() methods + input action properties
- `MoonshineCharacter_Simple.cpp` - Added rotation binding, phase advancement logic

---

## Setup in Unreal Editor

### Step 1: Create Input Actions (if not already created)

You need two new Input Actions:

#### **IA_Rotate** (for rotating pieces):
1. Content Browser → Right-click → Input → Input Action
2. Name: **IA_Rotate**
3. Open it → **Value Type: Axis2D (Vector 2D)**
4. Save

#### **IA_AdvancePhase** (for advancing construction phase):
1. Content Browser → Right-click → Input → Input Action
2. Name: **IA_AdvancePhase**
3. Open it → **Value Type: Digital (bool)**
4. Save

### Step 2: Add to Input Mapping Context

1. **Open your Input Mapping Context** (IMC_Default or whatever you're using)
2. **Add IA_Rotate**:
   - Click **+ Mappings**
   - Select **IA_Rotate**
   - Add key binding: **Left Arrow** → Modifiers: **Negate** (for left rotation)
   - Add key binding: **Right Arrow** → (no modifiers, for right rotation)
   - Or use a **Swizzle Input Axis Values** modifier with **Order: YXZ** for proper 2D handling
3. **Add IA_AdvancePhase**:
   - Click **+ Mappings**
   - Select **IA_AdvancePhase**
   - Add key binding: **P** (or any key you prefer)
4. **Save**

### Step 3: Assign Input Actions to Character

1. **Open BP_Character_Simple** (or your character blueprint)
2. **Class Defaults** (click the BP icon in toolbar)
3. **In Details panel**, find **Input** category:
   - **Rotate Action**: Assign **IA_Rotate**
   - **Advance Phase Action**: Assign **IA_AdvancePhase**
4. **Compile and Save**

---

## How to Use

### Workflow: Foundation → Rim Boards

1. **Start Game** (game begins in "Foundation" phase)

2. **Press B** to enter build mode

3. **Place Foundation Blocks**:
   - Foundation blocks should be on 8ft grid
   - Place at least 4 blocks (corners of your floor)
   - **Left-click** to place each block
   - Foundation blocks auto-nail (concrete)

4. **Press P** to advance to "FloorFrame" phase:
   - You'll see on-screen message: **"Phase: Floor Frame Phase"**
   - If it says "Cannot advance phase", place more foundation blocks
   - Need enough foundation for floor frame requirements

5. **Press Q** to cycle to Rim Board (piece type 1)

6. **Now the rim board should snap!**:
   - Move near a foundation corner
   - Rim board should **turn green** when near valid snap point
   - It will snap to the top of the foundation block
   - **Left/Right arrows** to rotate
   - **Left-click** to place
   - **Press E** to nail (shows wood material)

### Controls Summary:

| Key | Action |
|-----|--------|
| **B** | Toggle build mode |
| **Q** | Cycle piece types |
| **Left Arrow** | Rotate left |
| **Right Arrow** | Rotate right |
| **Left Click** | Place piece |
| **E** | Nail piece (lock it permanently) |
| **P** | Advance construction phase |

---

## Why Phase System?

Real construction follows a sequence:
1. **Foundation Phase** - Lay concrete foundation blocks
2. **Floor Frame Phase** - Add rim boards and floor joists
3. **Floor Sheathing Phase** - Add plywood subfloor
4. **Wall Frame Phase** - Build wall studs and plates
5. **Wall Sheathing Phase** - Add wall sheathing
6. **Roof Frame Phase** - Add rafters
7. **Roof Sheathing Phase** - Add roof sheathing

**You can't skip ahead!** Can't add rim boards without foundation first.

---

## Phase Requirements

### Foundation → FloorFrame:
- Need at least 4 foundation blocks placed
- Blocks must be nailed (auto-nails for foundation)

### FloorFrame → FloorSheathing:
- Need rim boards forming a perimeter
- Need floor joists connecting rim boards

### (Other phases to be implemented...)

---

## Troubleshooting

### Rotation still not working:
- **Check IA_Rotate is assigned** in BP_Character_Simple → Input → Rotate Action
- **Check IMC has arrow key bindings** for IA_Rotate
- Make sure you're **in build mode** (press B first)
- Rotation only works when **preview piece is active** (not when build mode is off)

### Rim board still won't snap (stays red):
- **Check current phase**: Press P - if it says "Cannot advance phase", you need more foundation blocks
- **Once in FloorFrame phase**, get close to a foundation corner (within 50cm)
- Rim board should **turn green** when near valid socket
- Make sure foundation blocks are **nailed** (they auto-nail on placement)

### "No pieces available for building":
- Open BP_Character_Simple
- BuildingComponent → Available Piece Types
- Element 0: BP_FoundationBlock
- Element 1: BP_RimBoard (not the C++ class, the BLUEPRINT)

### Arrow keys move camera instead of rotating piece:
- Make sure you're **in build mode** (B key)
- Check **IMC priority** - building context should be active
- Try different keys if arrow keys are bound elsewhere

### Phase won't advance (yellow message):
- You need **at least 4 foundation blocks** placed
- Blocks must be **nailed** (foundation auto-nails)
- Check console log for specific requirements

---

## Visual Feedback

### Rim Board Colors:
- **Green translucent** = Valid placement, will snap to foundation
- **Red translucent** = Invalid placement (wrong phase or no snap point nearby)
- **Yellow semi-transparent** = Placed, waiting to be nailed (press E)
- **Wood material** = Nailed, permanently locked

### On-Screen Messages:
- Green text = Phase advanced successfully
- Yellow text = Cannot advance (need more pieces)

---

## Advanced: Alternative Input Setup

If you want different rotation controls:

### Option A: Q/E for Rotation (instead of arrow keys)
- Modify IMC: Unbind Q from cycle (or use different key)
- Bind Q to IA_Rotate (negative X)
- Bind E to IA_Rotate (positive X)

### Option B: Mouse Wheel for Rotation
- Set IA_Rotate binding to **Mouse Wheel Axis**
- Up = rotate right, Down = rotate left

### Option C: Gamepad Support
- Bind **Right Stick X-Axis** to IA_Rotate
- Works automatically with controllers

---

## Next Steps

Once rim boards are snapping and rotating:

1. **Build a floor frame**:
   - Place rim boards around foundation perimeter
   - Connect corners at 90 degrees
   - Rim boards should form a complete rectangle

2. **Later: Floor joists** (next implementation):
   - Span between opposite rim boards
   - Snap to top face sockets at 16" intervals

3. **Later: Plywood sheathing**:
   - First corner snaps to rim board corner
   - Subsequent sheets snap edge-to-edge

---

## Summary of Changes

✅ **Rotation input binding added** - Arrow keys now rotate pieces
✅ **Phase advancement system added** - P key advances through construction phases
✅ **On-screen feedback** - Shows current phase and warnings
✅ **Phase validation** - Prevents placing pieces out of order

**The rim board should now snap to foundation corners once you advance to FloorFrame phase!**

Compile the project and test:
1. Place 4+ foundation blocks
2. Press P to advance to FloorFrame
3. Press Q to cycle to rim board
4. Move near foundation corner - should turn GREEN
5. Use arrow keys to rotate
6. Click to place, E to nail
