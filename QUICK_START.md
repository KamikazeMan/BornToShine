# Quick Start Guide - Born To Shine

## ⚡ Getting Started

### 1. Generate Project Files
```bash
# Right-click BornToShine.uproject
# Select "Generate Visual Studio project files"
```

### 2. Open in IDE
```bash
# Open BornToShine.sln in Visual Studio, Rider, or VS Code
```

### 3. Build the Project
```bash
# In Visual Studio: Build > Build Solution (Ctrl+Shift+B)
# Or compile directly in Unreal Editor
```

### 4. Open in Unreal Editor
```bash
# Double-click BornToShine.uproject
```

## 🎨 Essential Editor Setup

### Create Blueprints

1. **BP_FoundationBlock**
   - Create Blueprint Class based on `FoundationBlock`
   - Add a Static Mesh Component
   - Assign a cube mesh (2ft x 2ft x 1ft) or create your own
   - Create/assign a material with BaseColor parameter

2. **BP_Character**
   - Create Blueprint Class based on `MoonshineCharacter`
   - In "Available Piece Types" array, add BP_FoundationBlock
   - Set up Enhanced Input Actions (see below)

3. **BP_GameMode**
   - Create Blueprint Class based on `BornToShineGameMode`
   - Set Default Pawn Class to BP_Character

### Setup Enhanced Input

1. **Create Input Actions** (Content Browser > Right-click > Input > Input Action)
   - IA_Move (Value Type: Axis2D)
   - IA_Look (Value Type: Axis2D)
   - IA_Jump (Value Type: Digital)
   - IA_Sprint (Value Type: Digital)
   - IA_ToggleCamera (Value Type: Digital)
   - IA_ToggleBuildMode (Value Type: Digital)
   - IA_PlacePiece (Value Type: Digital)
   - IA_CyclePiece (Value Type: Digital)
   - IA_NailPiece (Value Type: Digital)

2. **Create Input Mapping Context** (Right-click > Input > Input Mapping Context)
   - Name it "IMC_Default"
   - Map keys:
     - IA_Move → W/A/S/D keys
     - IA_Look → Mouse XY 2D-Axis
     - IA_Jump → Space
     - IA_Sprint → Left Shift
     - IA_ToggleCamera → V
     - IA_ToggleBuildMode → B
     - IA_PlacePiece → Left Mouse Button
     - IA_CyclePiece → Q
     - IA_NailPiece → E

3. **Assign to Character**
   - Open BP_Character
   - Find "Default Mapping Context" variable
   - Set it to your IMC_Default
   - Assign all Input Action variables to the corresponding IA_ assets

### Create a Level

1. **Create MainLevel**
   - File > New Level > Basic
   - Save as "MainLevel"

2. **Set Game Mode**
   - World Settings > Game Mode Override > BP_GameMode

3. **Add Player Start**
   - Place a Player Start actor in the level

4. **Add Ground**
   - Add a Plane or Landscape for the ground

5. **Test It!**
   - Click Play
   - Press B to enter build mode
   - Place foundation blocks!

## 🎮 Test Controls

| Key | Action |
|-----|--------|
| WASD | Move |
| Mouse | Look |
| V | Toggle camera mode |
| B | Toggle build mode |
| Left Click | Place piece |
| Q | Cycle piece types |
| E | Nail piece (lock it) |

## 📋 Verification Checklist

After setup, verify:

- [ ] Project compiles without errors
- [ ] BP_FoundationBlock exists with mesh and material
- [ ] BP_Character has foundation block in AvailablePieceTypes
- [ ] BP_GameMode is set in World Settings
- [ ] Enhanced Input actions are created and mapped
- [ ] MainLevel has player start and ground
- [ ] Press Play - character spawns
- [ ] Press B - build mode activates
- [ ] Green ghost of foundation appears
- [ ] Left click - foundation places on grid
- [ ] Foundation snaps to 8ft (243.84cm) grid intervals

## 🐛 Troubleshooting

**"No piece types available for building"**
→ Add BP_FoundationBlock to BP_Character's AvailablePieceTypes array

**Input not working**
→ Assign Enhanced Input actions in BP_Character
→ Check Project Settings > Input > Default Mapping Context

**Preview piece not visible**
→ Check BP_FoundationBlock has a mesh assigned
→ Ensure material has opacity/translucency enabled

**Compilation errors**
→ Regenerate project files
→ Clean and rebuild solution

## 📖 Next Steps

1. ✅ Place foundation blocks in a rectangle pattern (minimum 4)
2. 🚧 Implement rim boards (see CONSTRUCTION_SYSTEM_GUIDE.md)
3. 🚧 Implement floor joists
4. 🚧 Implement plywood sheathing
5. 🚧 Continue with walls, roof, etc.

## 📚 Documentation

- **CONSTRUCTION_SYSTEM_GUIDE.md** - Complete architecture documentation
- **README.md** - Project overview

---

**Happy Building! 🏗️**
