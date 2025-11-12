# Setting Up Final Materials for Construction Pieces

## Overview

Foundation blocks are concrete - they don't get nailed! Wood pieces (rim boards, joists, etc.) get nailed and show wood materials.

Now you can set different materials for each piece type:
- **Foundation blocks**: Show concrete material immediately when placed
- **Wood pieces**: Show wood material when nailed

---

## Step 1: Create Your Concrete Material

### Basic Concrete Material:

1. **Right-click in Content Browser** → Material
2. Name it **M_Concrete**
3. Open it
4. Set **Base Color** to gray (R=0.5, G=0.5, B=0.5)
5. Set **Roughness** to 0.8 (rough surface)
6. **Save**

### Or Use Engine Content:

- Enable "Show Engine Content"
- Search for "concrete" materials
- Pick one you like

---

## Step 2: Create Your Wood Material

### Basic Wood Material:

1. **Right-click in Content Browser** → Material
2. Name it **M_Wood**
3. Open it
4. Set **Base Color** to brown (R=0.6, G=0.4, B=0.2)
5. Set **Roughness** to 0.7
6. **Save**

### Advanced (with texture):

- Find/download wood texture
- Add Texture Sample nodes
- Connect to Base Color, Normal, Roughness
- Looks much better!

---

## Step 3: Configure BP_FoundationBlock

1. **Open BP_FoundationBlock**
2. **Select MeshComponent** in Components panel
3. **In Details panel**, find **Materials** section:
   - **Element 0**: Keep as **M_PreviewPiece** (for ghost preview)

4. **Still in Details**, scroll to **Construction → Materials** section:
   - **Nailed Material**: Select **M_Concrete**
   - **Auto Nail On Place**: ✅ Checked (already set in C++)

5. **Compile and Save**

---

## Step 4: Test Foundation Blocks

1. **Play** the game
2. Press **B** to enter build mode
3. Foundation ghost appears (**green/translucent**)
4. Move around - snaps to 8ft grid
5. **Left Click** to place
6. **Foundation immediately shows concrete material!** ✅

No more yellow, no need to press E - it's instant!

---

## Step 5: Future Wood Pieces Setup

When you create rim boards, joists, plywood:

### In BP_RimBoard (example):

1. **Materials → Element 0**: M_PreviewPiece (for preview)
2. **Construction → Materials**:
   - **Nailed Material**: M_Wood
   - **Auto Nail On Place**: ❌ Unchecked (wood gets nailed manually)

3. Workflow:
   - Preview = green ghost
   - Place = yellow (waiting to be nailed)
   - Press E to nail = wood material shows!

---

## How It Works

### Foundation Blocks (Concrete):

```
Preview (B key) → Green ghost (M_PreviewPiece)
                        ↓
Place (Click)   → Concrete material instantly (M_Concrete)
                  [Auto-nailed, no E key needed]
```

### Wood Pieces (Rim Boards, Joists, etc.):

```
Preview (B key) → Green ghost (M_PreviewPiece)
                        ↓
Place (Click)   → Yellow (M_PreviewPiece with yellow color)
                        ↓
Nail (E key)    → Wood material (M_Wood)
                  [Locked in place]
```

---

## Material Properties Reference

### Preview Material (M_PreviewPiece):
- **Blend Mode**: Translucent
- **Parameter**: BaseColor (Vector)
- **Purpose**: Shows green/red/yellow feedback

### Concrete Material (M_Concrete):
- **Blend Mode**: Opaque
- **Base Color**: Gray
- **Roughness**: High (0.7-0.9)
- **Purpose**: Final foundation appearance

### Wood Material (M_Wood):
- **Blend Mode**: Opaque
- **Base Color**: Brown
- **Roughness**: Medium (0.6-0.8)
- **Optional**: Wood texture
- **Purpose**: Final wood piece appearance

---

## Blueprint Settings Summary

### For Foundation Blocks:
```
BP_FoundationBlock:
├─ MeshComponent
│  └─ Materials
│     └─ Element 0: M_PreviewPiece
└─ Details → Construction
   ├─ Nailed Material: M_Concrete
   └─ Auto Nail On Place: ✅ true
```

### For Wood Pieces (future):
```
BP_RimBoard:
├─ MeshComponent
│  └─ Materials
│     └─ Element 0: M_PreviewPiece
└─ Details → Construction
   ├─ Nailed Material: M_Wood
   └─ Auto Nail On Place: ❌ false
```

---

## Advanced: Multiple Materials

You can assign different materials to different piece types!

### Examples:

**BP_FoundationBlock**:
- Nailed Material: M_Concrete

**BP_RimBoard**:
- Nailed Material: M_Wood_Pine

**BP_FloorJoist**:
- Nailed Material: M_Wood_Spruce

**BP_Plywood**:
- Nailed Material: M_Plywood

Each piece can have its own unique final appearance!

---

## Troubleshooting

### Foundation still shows yellow:

- Check "Auto Nail On Place" is enabled in Blueprint
- Rebuild project in Visual Studio (C++ changes need recompiling)
- Verify M_Concrete is assigned to "Nailed Material"

### Material looks wrong:

- Make sure Nailed Material is assigned
- Check material is Opaque (not Translucent)
- Verify material has proper Base Color

### Preview not showing:

- Element 0 should be M_PreviewPiece (translucent)
- Nailed Material is separate setting

### Wood pieces auto-nail (shouldn't):

- Set "Auto Nail On Place" to false in Blueprint
- Only foundations should auto-nail

---

## Result

**Foundation blocks** = Realistic! Concrete appears immediately when placed.

**Wood pieces** = Realistic! You nail them in place with E key.

Just like real construction! 🏗️

---

**Next**: Create rim boards and see the full workflow in action!
