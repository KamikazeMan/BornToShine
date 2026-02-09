# Quick Fix: Rim Board Setup

## The Problem
You're seeing a red line because:
1. No mesh is assigned to the RimBoard
2. Red = invalid placement color
3. It's stuck to player because it's in preview mode

## Quick Setup (5 minutes)

### Step 1: Open Unreal Editor
- Let the project compile (Hot Reload)
- Wait for "Compilation Successful" message

### Step 2: Create BP_RimBoard Blueprint

1. **In Content Browser**, right-click in your Content folder
2. **Blueprint Class** → Search for "RimBoard"
3. Select **RimBoard** (the C++ class)
4. Name it: **BP_RimBoard**
5. **Double-click** to open it

### Step 3: Add a Temporary Mesh

Since you don't have a 2x6 lumber mesh yet, use a cube:

1. **Select "MeshComponent"** in the Components panel (left side)
2. **In Details panel** (right side), find "Static Mesh"
3. **Click the dropdown** → Search: "cube"
4. **Select: "Engine/BasicShapes/Cube"** (or SM_Cube)

### Step 4: Scale the Mesh Correctly

Still in MeshComponent details:

1. Find **"Transform" → "Scale"**
2. Set scale to match 2x6 dimensions:
   - **X: 2.438** (8 feet long)
   - **Y: 0.038** (1.5 inches wide)
   - **Z: 0.140** (5.5 inches tall)

### Step 5: Set Up Materials

In MeshComponent Details:

1. **Materials section** → Element 0
2. For now, create a simple material:
   - **Right-click in Content** → Material → Name it "M_RimBoardPreview"
   - **Open the material**
   - Set **Blend Mode: Translucent**
   - Add a **Vector Parameter** named "BaseColor"
   - **Connect BaseColor** to Base Color and Opacity pins
   - **Save and close**
3. **Back in BP_RimBoard**, assign M_RimBoardPreview to Element 0

### Step 6: Configure RimBoard Properties

In BP_RimBoard Details (with Blueprint selected, not MeshComponent):

1. Find **"Construction" category**:
   - **Auto Nail On Place**: ❌ **UNCHECK** (wood needs manual nailing)

2. Find **"Construction | Materials" category**:
   - **Nailed Material**: Can leave empty for now (or assign a wood material if you have one)

### Step 7: Add to Character

1. **Open BP_Character_Simple** (or your character blueprint)
2. **Select BuildingComponent** in Components panel
3. **In Details**, find **"Available Piece Types"** array
4. **Add new element** (+ button):
   - **Element 0**: BP_FoundationBlock (should already be there)
   - **Element 1**: BP_RimBoard (add this)
5. **Compile and Save**

### Step 8: Test in Game

1. **Play (Alt+P)**
2. **Press B** to enter build mode
3. **Press Q** to cycle to rim board (piece type 1)
4. **Should now see a green translucent cube** (not a red line!)
5. **Left-click** to place
6. **Press E** to nail

## Why Was It a Red Line?

- **Red line** = The mesh component bounds without an actual mesh
- **Red color** = Invalid placement (no valid snap point)
- **Stuck to player** = Preview mode following your camera

## If It's Still Red (Not Green):

If you see the cube but it's red:
- **Red means invalid placement**
- You need **foundation blocks placed first**
- Rim boards snap to foundation corners
- **Place some foundation blocks**, then try rim board again

## Proper Mesh (Later):

Eventually you'll want a real 2x6 lumber mesh:
1. Model it in Blender (1.5" x 5.5" x 8')
2. Export as FBX
3. Import to Unreal
4. Assign in BP_RimBoard instead of cube

## Colors Explained:

- **Green translucent** = Valid placement, will snap
- **Red translucent** = Invalid placement, can't place here
- **Yellow semi-transparent** = Placed, waiting to be nailed (press E)
- **Opaque wood material** = Nailed, permanently locked

## Common Issues:

### "No pieces available for building"
- Make sure BP_RimBoard is added to BuildingComponent's Available Piece Types
- Check you're editing BP_Character_Simple (not the old MoonshineCharacter)

### Mesh is too big/small
- Adjust the scale in Transform section
- Use the values I provided above for correct 2x6 dimensions

### Can't see preview at all
- Make sure material has BaseColor parameter
- Check material is set to Translucent blend mode
- Verify mesh is assigned to MeshComponent

### Preview is solid, not translucent
- Material must be set to **Translucent** (not Opaque)
- Make sure you're using the material on Element 0

## Next Steps:

Once this works:
1. Test snapping to foundation blocks
2. Rotate with arrow keys
3. Press E to nail after placing
4. Later: Add proper lumber mesh and wood material
