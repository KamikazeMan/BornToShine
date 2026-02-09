# Creating Ghost Preview Material

## Problem
No ghost preview showing when in build mode - piece is invisible or not showing green/red feedback.

## Solution
Create a translucent material with BaseColor parameter that the C++ code can modify.

---

## Step-by-Step: Create Preview Material

### 1. Create Base Material

1. **Right-click in Content Browser** → Material
2. Name it **M_PreviewPiece**
3. **Double-click** to open Material Editor

### 2. Configure Material Settings

In the **Details Panel** (left side when material is open):

1. **Blend Mode**: Change from "Opaque" to **Translucent**
2. **Shading Model**: Keep as **Default Lit**

### 3. Add Material Nodes

In the Material Graph:

#### Add BaseColor Parameter:
1. **Right-click** → Parameter → **Vector Parameter**
2. **Name it EXACTLY**: `BaseColor` (case-sensitive!)
3. **Default Value**: Set to green (R=0, G=1, B=0, A=0.5)
4. **Connect** the output to **Base Color** pin on main node

#### Add Opacity:
1. Use the **Alpha (A)** output from the BaseColor parameter
2. **Connect** to **Opacity** pin on main node

#### Optional - Add Fresnel (Edge Glow):
1. Right-click → Utility → **Fresnel**
2. Connect Fresnel output → **Emissive Color**
3. Multiply by BaseColor for colored glow

### 4. Final Node Setup

```
[Vector Parameter: BaseColor]
  ├─ RGB → Base Color pin
  └─ A → Opacity pin

[Fresnel] (optional)
  └─ Multiply with BaseColor → Emissive Color
```

### 5. Save Material

Click **Save** button (or Ctrl+S)

---

## Assign Material to Foundation Block

### 1. Open BP_FoundationBlock

1. Open your **BP_FoundationBlock** blueprint
2. Select **MeshComponent** in Components panel

### 2. Assign Material

In Details panel:
1. Find **Materials** section
2. **Element 0**: Select **M_PreviewPiece**
3. **Compile and Save**

---

## Test the Preview

1. **Play** the game
2. Press **B** to enter build mode
3. You should now see:
   - ✅ Translucent foundation block (ghost)
   - ✅ Green when placement is valid
   - ✅ Red when placement is invalid
   - ✅ Snaps to 8ft grid

---

## Alternative: Quick Material Setup

If you don't want to create a material from scratch:

### Engine Content Material (Quick Test):

1. Open BP_FoundationBlock
2. Enable "Show Engine Content" in Content Browser
3. Search for: **M_Basic_Transparent**
4. Assign to mesh
5. Test (won't have color feedback but will be visible)

---

## Troubleshooting Preview Issues

### Preview spawns but is invisible:

**Cause**: Material is opaque or opacity is 0

**Fix**:
- Set material Blend Mode to Translucent
- Ensure Opacity is > 0 (try 0.5)

### Preview shows but no color change:

**Cause**: Material doesn't have BaseColor parameter

**Fix**:
- Parameter MUST be named "BaseColor" exactly
- Parameter MUST be Vector (not Scalar)
- Must be connected to Base Color pin

### Preview is solid black:

**Cause**: Emissive is too high or Base Color is black

**Fix**:
- Set BaseColor default to green: (0, 1, 0, 0.5)
- Reduce or remove Emissive

### Can't see through preview:

**Cause**: Opacity is 1.0 (fully opaque)

**Fix**:
- Use Alpha channel of BaseColor for opacity
- Set default alpha to 0.3-0.7 for translucency

---

## Advanced: Material Instance (Recommended)

For easier tweaking without recompiling:

1. **Right-click M_PreviewPiece** → Create Material Instance
2. Name it **MI_PreviewPiece**
3. Open the instance
4. Check the box next to **BaseColor** to enable editing
5. Adjust color and opacity here
6. Assign **MI_PreviewPiece** to BP_FoundationBlock instead

This lets you tweak colors in real-time!

---

## Expected Visual Result

When working correctly:

**Preview Mode (Ghost)**:
- Semi-transparent
- Green tint when valid placement
- Red tint when invalid placement
- Visible through terrain/objects

**Placed Mode**:
- Yellow tint
- More opaque
- Still translucent

**Nailed Mode**:
- Wood/concrete color
- Fully opaque
- Solid material

---

## Code Reference

The C++ code sets colors automatically:

```cpp
// In BuildablePiece.cpp
ValidPlacementColor = FLinearColor(0.0f, 1.0f, 0.0f, 0.5f);    // Green
InvalidPlacementColor = FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);  // Red
PlacedColor = FLinearColor(1.0f, 1.0f, 0.0f, 0.8f);            // Yellow
NailedColor = FLinearColor(0.8f, 0.6f, 0.4f, 1.0f);            // Wood
```

The material parameter "BaseColor" gets these values dynamically!

---

**Your preview should now work perfectly!** 👻✅
