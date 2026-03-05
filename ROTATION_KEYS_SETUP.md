# Rotation Keys Setup (R and T)

The C++ code for rotation is already implemented in `MoonshineCharacter_Simple`. You just need to configure the Input Actions in Unreal Editor.

## Quick Setup (5 minutes)

### 1. Create Input Actions

In **Content Browser**:

1. Navigate to your Input folder (usually `Content/Input/` or `Content/Characters/Input/`)
2. Right-click → **Input → Input Action**
3. Create two Input Actions:
   - **IA_RotateLeft** (Input Action Value Type: **Digital (bool)**)
   - **IA_RotateRight** (Input Action Value Type: **Digital (bool)**)

### 2. Add to Input Mapping Context

1. Open your **IMC_Default** (or whatever Input Mapping Context you're using)
2. Click **+ Add** to add new mappings:

   **IA_RotateLeft:**
   - Key: **R**
   - Modifiers: None
   - Triggers: Pressed

   **IA_RotateRight:**
   - Key: **T**
   - Modifiers: None
   - Triggers: Pressed

3. Save the Input Mapping Context

### 3. Assign to Character Blueprint

1. Open **BP_Character_Simple** (or your character blueprint)
2. In the **Details** panel, find the **Input** section
3. Assign:
   - **Rotate Left Action** → IA_RotateLeft
   - **Rotate Right Action** → IA_RotateRight
4. Compile and Save

## Testing

1. Press **B** to enter build mode
2. Press **Q** to cycle to rim board
3. Press **R** to rotate left (counter-clockwise)
4. Press **T** to rotate right (clockwise)

Each rotation is 15 degrees by default.

## Notes

- The rotation code is already bound in `MoonshineCharacter_Simple.cpp` lines 147-155
- `RotateLeftAction` and `RotateRightAction` properties already exist
- Each press rotates 15° (configurable in `BuildablePiece::RotationStep`)
