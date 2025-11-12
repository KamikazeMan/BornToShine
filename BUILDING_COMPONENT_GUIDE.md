<!-- Building Component Architecture Guide -->

# Building Component - Modular Construction System

## 🎯 Overview

The **BuildingComponent** is a modular, reusable component that handles all construction logic. Attach it to any character to give them building abilities!

## ✅ Benefits

- **Separation of Concerns**: Building logic separate from character movement
- **Reusable**: Attach to any actor (player, AI, NPC)
- **Cleaner Code**: Character class is much simpler
- **Blueprint Friendly**: All functions exposed to Blueprints
- **Easy Testing**: Test building system independently

## 📦 What's Included

### BuildingComponent.h/cpp
The main component with all building functionality:
- Build mode toggle
- Piece spawning/placement
- Piece rotation and scaling
- Piece cycling
- Nail/remove placed pieces

### MoonshineCharacter_Simple.h/cpp
Simplified character that uses the component:
- Movement and camera only
- Delegates building inputs to component
- Much cleaner than original

## 🔧 How to Use

### Option 1: Use the Simplified Character (Recommended)

1. **Update GameMode**:
   ```cpp
   // In BornToShineGameMode.cpp
   DefaultPawnClass = AMoonshineCharacter_Simple::StaticClass();
   ```

2. **Create BP_Character_Simple**:
   - Right-click → Blueprint Class
   - Search for **MoonshineCharacter_Simple**
   - Name it `BP_Character_Simple`

3. **Configure Building Component**:
   - Open `BP_Character_Simple`
   - Select **BuildingComponent** in Components panel
   - In Details, find **Available Piece Types**
   - Add `BP_FoundationBlock`

4. **Done!** Character now has building abilities

### Option 2: Add Component to Existing Character

If you want to keep your current character:

1. **Open your BP_Character**
2. **Add Component** → Search for "BuildingComponent"
3. **Configure Available Piece Types** array
4. **Update Input Bindings**:
   ```cpp
   // Instead of calling character functions directly:
   BuildingComponent->ToggleBuildMode();
   BuildingComponent->PlaceCurrentPiece();
   // etc.
   ```

### Option 3: Add to ANY Actor

You can add building to NPCs, vehicles, etc:

```cpp
// In your actor's constructor
BuildingComp = CreateDefaultSubobject<UBuildingComponent>(TEXT("BuildingComponent"));
```

## 🎮 Blueprint Usage

All functions are BlueprintCallable:

```cpp
// Toggle build mode
BuildingComponent->ToggleBuildMode()

// Place piece
BuildingComponent->PlaceCurrentPiece()

// Cycle to next piece type
BuildingComponent->CyclePieceType()

// Rotate preview
BuildingComponent->RotatePreviewLeft()
BuildingComponent->RotatePreviewRight()

// Scale preview
BuildingComponent->ScalePreview(0.1f) // Scale up by 0.1

// Nail last placed piece
BuildingComponent->NailLastPlacedPiece()

// Remove last placed piece
BuildingComponent->RemoveLastPlacedPiece()

// Get info
bool InBuildMode = BuildingComponent->IsInBuildMode()
EPieceType Type = BuildingComponent->GetCurrentPieceType()
FString Name = BuildingComponent->GetCurrentPieceName()
int32 Count = BuildingComponent->GetPlacedPieceCount()
```

## 📋 Configuration

### In Blueprint:

Select BuildingComponent → Details panel:

| Property | Description | Default |
|----------|-------------|---------|
| **Available Piece Types** | Array of buildable piece classes | Empty |
| **Build Raycast Distance** | How far to raycast for placement (cm) | 2000 (20m) |
| **Preview Distance** | Default preview distance (cm) | 300 (3m) |
| **Snap Search Radius** | Socket snap detection radius (cm) | 500 (5m) |

### Example Setup:

1. **Available Piece Types**:
   - Element 0: `BP_FoundationBlock`
   - Element 1: `BP_RimBoard` (when ready)
   - Element 2: `BP_FloorJoist` (when ready)

2. **Distances**:
   - Build Raycast Distance: `2000.0` (can place up to 20m away)
   - Preview Distance: `300.0` (preview 3m from camera)
   - Snap Search Radius: `500.0` (detect sockets within 5m)

## 🔄 Architecture Comparison

### Before (Old MoonshineCharacter):
```
Character
├── Movement Logic ✅
├── Camera Logic ✅
├── Building Logic ❌ (too much responsibility!)
│   ├── Preview piece spawning
│   ├── Placement validation
│   ├── Rotation/scaling
│   ├── Piece tracking
│   └── Input handling
```

### After (With BuildingComponent):
```
Character_Simple
├── Movement Logic ✅
├── Camera Logic ✅
└── Input Delegation ✅ (just forwards to component)

BuildingComponent (Reusable!)
├── Preview piece spawning ✅
├── Placement validation ✅
├── Rotation/scaling ✅
├── Piece tracking ✅
└── Building state management ✅
```

## 🎯 Features

### Build Mode
- Toggle on/off
- Spawns preview piece
- Updates preview position every frame
- Destroys preview when exiting

### Piece Management
- **Cycle Types**: Q key to cycle through available pieces
- **Place**: Left-click to place current preview
- **Nail**: E key to lock last placed piece
- **Remove**: Remove last placed piece (undo)

### Preview Manipulation
- **Rotate Left/Right**: R key (can bind)
- **Rotate Pitch**: T/G keys (can bind)
- **Rotate Roll**: Mouse movement (can bind)
- **Scale**: Mouse wheel

### Smart Placement
- Raycasts from camera to find placement location
- Falls back to preview distance if no hit
- Integrates with socket snapping system
- Visual feedback (green/red from BuildablePiece)

## 🧪 Testing

### Quick Test in Editor:

1. Open `BP_Character_Simple` (or your character with component)
2. Add `BP_FoundationBlock` to Available Piece Types
3. Play (PIE)
4. Press **B** - should enter build mode
5. Move mouse - foundation preview follows
6. Preview snaps to 8ft grid
7. Left-click - places foundation
8. Press **E** - nails piece in place

### Component Testing:

You can test the component independently:

1. Create empty actor
2. Add BuildingComponent
3. Call functions from Blueprint/C++
4. No character needed!

## 🔮 Future Enhancements

The component architecture makes these easy to add:

- **Build UI Widget**: Component can broadcast events
- **Inventory System**: Track available materials
- **Build Permissions**: Check if player can build here
- **Networked Building**: Replicate component state
- **AI Building**: NPCs can use same component
- **Building Templates**: Save/load building designs

## 📚 Code Example

### C++ Usage:
```cpp
// Get the building component
UBuildingComponent* BuildComp = Character->FindComponentByClass<UBuildingComponent>();

// Toggle build mode
if (BuildComp)
{
    BuildComp->ToggleBuildMode();
}

// Check if in build mode
if (BuildComp && BuildComp->IsInBuildMode())
{
    // Do something
}
```

### Blueprint Usage:
```
Event Graph:
  [B Key Pressed]
    └─> Get Building Component
        └─> Toggle Build Mode

  [Left Mouse Button]
    └─> Get Building Component
        └─> Place Current Piece
```

## 🎉 Summary

The BuildingComponent is a **clean, modular architecture** for your construction system:

- ✅ Reusable on any actor
- ✅ Separates concerns (character vs building)
- ✅ Easier to maintain and test
- ✅ Blueprint friendly
- ✅ Follows SOLID principles

Use **MoonshineCharacter_Simple** for a clean example of how to integrate it!

---

**Happy Building!** 🏗️
