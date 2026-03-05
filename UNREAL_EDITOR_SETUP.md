# Unreal Editor Setup Guide - Born To Shine

Complete step-by-step guide to set up your construction game in Unreal Engine.

## 📋 Prerequisites

- Unreal Engine 5.3+ (you're using 5.6, which is perfect!)
- Project compiled successfully in Visual Studio
- Basic knowledge of Unreal Editor

---

## Step 1: Add Third Person Content Pack

This gives you a character mesh, animations, and input setup.

### Method 1: Add Content Pack (Recommended)

1. Open Unreal Editor (double-click `BornToShine.uproject`)
2. Go to **Edit → Add Feature or Content Pack...**
3. Select **Third Person** under "Content Packs"
4. Click **Add to Project**
5. Content will be added to `Content/ThirdPerson/`

### Method 2: If Content Pack Unavailable

Create a new Third Person project separately, then migrate the assets:
1. File → New Project → Third Person
2. In the new project: Right-click `Content/ThirdPerson` → Migrate
3. Select your BornToShine project's Content folder
4. Close the temporary project

---

## Step 2: Create Blueprint Classes

### A. Create BP_GameMode

1. In Content Browser, right-click → **Blueprint Class**
2. Search for **BornToShineGameMode** (your C++ class)
3. Name it `BP_GameMode`
4. **Don't need to change anything** - C++ already sets up defaults

### B. Create BP_PlayerController

1. Right-click → **Blueprint Class**
2. Search for **MoonshinePlayerController**
3. Name it `BP_PlayerController`
4. Open it (optional - already configured in C++)

### C. Create BP_Character

1. Right-click → **Blueprint Class**
2. Search for **MoonshineCharacter**
3. Name it `BP_Character`
4. **Open it** - we need to configure this one

#### Configure BP_Character:

1. **Add Mesh Component:**
   - Select the `Mesh` component (inherited from Character)
   - In Details panel:
     - **Skeletal Mesh**: Select `SKM_Manny` or `SKM_Quinn` (from ThirdPerson content)
     - **Location**: X=0, Y=0, Z=-90
     - **Rotation**: X=0, Y=0, Z=-90
     - **Animation Class**: `ABP_Manny` (from ThirdPerson)

2. **Configure Cameras:**
   - Select `ThirdPersonArm` component
     - **Target Arm Length**: 400.0
     - **Socket Offset**: X=0, Y=50, Z=50
     - **Use Pawn Control Rotation**: ✅ Enabled
   - Select `FirstPersonCamera`
     - **Location**: X=0, Y=0, Z=64

3. **Set Available Piece Types:**
   - In Details panel, find **Available Piece Types** array
   - Click **+** to add an element
   - Select `BP_FoundationBlock` (we'll create this next)

4. **Configure Enhanced Input:**
   - **Default Mapping Context**: Set to `IMC_Default` (we'll create this)
   - Assign all Input Action references (we'll create these)

5. **Compile and Save**

### D. Create BP_FoundationBlock

1. Right-click → **Blueprint Class**
2. Search for **FoundationBlock**
3. Name it `BP_FoundationBlock`
4. **Open it**

#### Configure BP_FoundationBlock:

1. **Add Static Mesh:**
   - Select `MeshComponent`
   - **Static Mesh**: Use `Cube` (Engine Content → Basic Shapes)
     - Or create a custom mesh later
   - **Scale**: X=0.6096, Y=0.6096, Z=0.3048 (2ft x 2ft x 1ft in meters)
   - **Material**: Create/assign a concrete material

2. **Create Material (Optional):**
   - Right-click → Material → Name it `M_Foundation`
   - Open it, add a **Constant3Vector** (gray color)
   - Connect to Base Color
   - Set **Blend Mode**: Translucent (for preview mode)
   - Save and assign to mesh

3. **Compile and Save**

---

## Step 3: Set Up Enhanced Input

### A. Create Input Actions

1. Right-click → **Input → Input Action**
2. Create these Input Actions:

| Name | Value Type | Description |
|------|-----------|-------------|
| `IA_Move` | Axis2D (Vector 2D) | WASD movement |
| `IA_Look` | Axis2D (Vector 2D) | Mouse look |
| `IA_Jump` | Digital (Boolean) | Jump |
| `IA_Sprint` | Digital (Boolean) | Sprint |
| `IA_ToggleCamera` | Digital (Boolean) | Toggle camera mode |
| `IA_ToggleBuildMode` | Digital (Boolean) | Toggle build mode |
| `IA_PlacePiece` | Digital (Boolean) | Place piece |
| `IA_CyclePiece` | Digital (Boolean) | Cycle piece types |
| `IA_NailPiece` | Digital (Boolean) | Nail piece |
| `IA_Rotate` | Digital (Boolean) | Rotate piece |
| `IA_Scale` | Axis (float) | Scale with mouse wheel |

### B. Create Input Mapping Context

1. Right-click → **Input → Input Mapping Context**
2. Name it `IMC_Default`
3. **Open it** and add mappings:

#### Movement
- **IA_Move**
  - Keyboard: **W** → Modifiers: Swizzle Input Axis Values (YXZ)
  - Keyboard: **S** → Modifiers: Negate, Swizzle (YXZ)
  - Keyboard: **A** → Modifiers: Negate
  - Keyboard: **D** → (no modifiers)

#### Look
- **IA_Look**
  - Mouse: **Mouse XY 2D-Axis**

#### Actions
- **IA_Jump**: Keyboard **Spacebar**
- **IA_Sprint**: Keyboard **Left Shift**
- **IA_ToggleCamera**: Keyboard **V**
- **IA_ToggleBuildMode**: Keyboard **B**
- **IA_PlacePiece**: Mouse **Left Mouse Button**
- **IA_CyclePiece**: Keyboard **Q**
- **IA_NailPiece**: Keyboard **E**
- **IA_Rotate**: Keyboard **R**
- **IA_Scale**: Mouse **Mouse Wheel Axis**

### C. Assign to BP_Character

1. Open `BP_Character`
2. In Details panel:
   - **Default Mapping Context**: `IMC_Default`
   - **Move Action**: `IA_Move`
   - **Look Action**: `IA_Look`
   - **Jump Action**: `IA_Jump`
   - **Sprint Action**: `IA_Sprint`
   - **Toggle Camera Action**: `IA_ToggleCamera`
   - **Toggle Build Mode Action**: `IA_ToggleBuildMode`
   - **Place Piece Action**: `IA_PlacePiece`
   - **Cycle Piece Action**: `IA_CyclePiece`
   - **Nail Piece Action**: `IA_NailPiece`
   - **Scale Action**: `IA_Scale`
3. **Compile and Save**

---

## Step 4: Create Main Level

### A. Create the Level

1. **File → New Level → Empty Level**
2. Save as `MainLevel` in `Content/Maps/`

### B. Add Essential Actors

1. **Add Directional Light:**
   - Search in Place Actors: "Directional Light"
   - Drag into level
   - Rotation: X=-45, Y=0, Z=0

2. **Add Sky Light:**
   - Search: "Sky Light"
   - Drag into level
   - Real Time Capture: ✅ Enabled

3. **Add Sky Atmosphere:**
   - Search: "Sky Atmosphere"
   - Drag into level

4. **Add Post Process Volume:**
   - Search: "Post Process Volume"
   - Drag into level
   - **Infinite Extent (Unbound)**: ✅ Enabled

5. **Add Floor:**
   - Create → **Basic → Plane** (or Landscape for terrain)
   - **Scale**: X=100, Y=100, Z=1
   - **Location**: X=0, Y=0, Z=0
   - Create a simple material for ground

6. **Add Player Start:**
   - Search: "Player Start"
   - Drag into level
   - **Location**: X=0, Y=0, Z=200 (above the floor)

### C. Set World Settings

1. Click **Settings** button in toolbar → **World Settings**
2. In World Settings panel:
   - **Game Mode Override**: `BP_GameMode`
3. **Save All**

---

## Step 5: Configure Project Settings

### A. Default Maps

1. **Edit → Project Settings**
2. **Maps & Modes** section:
   - **Editor Startup Map**: `MainLevel`
   - **Game Default Map**: `MainLevel`
   - **Default GameMode**: `BP_GameMode`

### B. Input Settings

1. In Project Settings → **Input**
2. **Default Classes:**
   - **Default Player Input Class**: `EnhancedPlayerInput`
   - **Default Input Component Class**: `EnhancedInputComponent`
3. **Default Mapping Contexts** (optional):
   - Add `IMC_Default` with Priority 0

---

## Step 6: Test the Game

### Quick Test Checklist

1. Click **Play** button (Alt+P)
2. **Expected Results:**
   - ✅ Character spawns at Player Start
   - ✅ WASD moves character
   - ✅ Mouse looks around
   - ✅ Spacebar jumps
   - ✅ Shift sprints
   - ✅ V toggles camera (1st/3rd person)
   - ✅ B enters build mode
   - ✅ Ghost foundation block appears
   - ✅ Foundation snaps to 8ft grid
   - ✅ Block turns green (valid) or red (invalid)
   - ✅ Left click places foundation

### Common Issues

**"No piece types available for building"**
- Open BP_Character
- Add BP_FoundationBlock to Available Piece Types array

**Input not working**
- Check BP_Character has all Input Actions assigned
- Verify IMC_Default is set as Default Mapping Context
- Check Project Settings → Input → Default Classes

**Character falls through floor**
- Add collision to floor mesh
- Check Player Start is above ground (Z > 0)

**Can't see character**
- Check BP_Character → Mesh has skeletal mesh assigned
- Verify mesh is visible (not hidden)
- Check camera positions

**Preview piece not visible**
- Create material with translucency
- Assign material to BP_FoundationBlock mesh
- Check BaseColor parameter exists in material

---

## Step 7: Advanced Setup (Optional)

### Create Materials

1. **M_Foundation_Preview** (for ghost preview):
   ```
   - Blend Mode: Translucent
   - Parameter: BaseColor (Vector)
   - Parameter: Opacity (Scalar) = 0.5
   ```

2. **M_Wood** (for wood pieces):
   ```
   - Base Color: Brown
   - Roughness: 0.7
   - Normal Map: Optional wood texture
   ```

### Create UI Widget

1. Right-click → **User Interface → Widget Blueprint**
2. Name it `WBP_BuildMode`
3. Add text elements showing:
   - Current construction phase
   - Current piece type
   - Instructions

4. Assign to BP_PlayerController:
   - **Build Mode Widget Class**: `WBP_BuildMode`

---

## 🎉 You're Ready!

Your construction game is now set up! Press **Play** and start building:

1. Press **B** to enter build mode
2. Move around - foundation ghost follows
3. Foundation **snaps to 8ft grid automatically**!
4. **Green** = valid placement
5. **Red** = invalid (wrong phase, no support, etc.)
6. **Left Click** to place
7. Place 4+ foundations in a rectangle
8. System will advance to Floor Frame phase!

---

## Next Steps

Once you're comfortable with foundations:

1. **Create BP_RimBoard** (coming soon!)
2. **Create BP_FloorJoist**
3. **Create BP_Plywood**
4. Build your moonshine empire! 🏚️🥃

---

## 📖 Additional Resources

- **CONSTRUCTION_SYSTEM_GUIDE.md** - Full architecture documentation
- **QUICK_START.md** - Quick reference guide
- Unreal Engine Documentation: Enhanced Input System
- YouTube: Unreal Engine 5 Third Person Character Setup

**Happy Building!** 🏗️
