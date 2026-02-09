# Troubleshooting: Rim Board Not Snapping

## Error Message
```
LogTemp: Warning: Cannot place rim board - no foundation blocks nearby
```

## Most Likely Cause
**The ConstructionPhaseManager is not being spawned/initialized!**

Without the ConstructionPhaseManager, foundation blocks aren't registered, so rim boards can't find them.

---

## Quick Fix - Check Console Logs

When you start playing, check the Output Log for these messages:

**✅ Should see:**
```
LogTemp: GameMode: Socket Manager initialized
LogTemp: GameMode: Construction Phase Manager initialized
```

**❌ If you see:**
```
LogTemp: Error: ConstructionPhaseManager not found! Add BP_ConstructionPhaseManager to your level!
```

Then the manager isn't being created!

---

## Solution Steps

### Step 1: Check Your GameMode

1. **Open Project Settings** → Maps & Modes
2. **Default GameMode** should be set to **BornToShineGameMode** (or your custom game mode based on it)
3. **OR** in your level, **World Settings** → GameMode Override → **BornToShineGameMode**

### Step 2: Verify GameMode is Active

Play the game and check Output Log:
- Should see: `LogTemp: GameMode: Construction Phase Manager initialized`
- If you DON'T see this, GameMode isn't running properly

### Step 3: Test Foundation Registration

1. **Place a foundation block**
2. **Check Output Log**, should see:
   ```
   LogTemp: Registered BP_FoundationBlock_C_X with PhaseManager
   ```
3. **If you see**: `LogTemp: Error: ConstructionPhaseManager not found!`
   - GameMode didn't spawn the manager!

### Step 4: Try Rim Board Again

1. **After placing foundation**, try placing rim board
2. **Check Output Log**:
   ```
   LogTemp: RimBoard prerequisite check: 1 total foundations registered
   LogTemp: Found X pieces within 500cm of location (...)
   LogTemp: Found nearby foundation at distance: XX cm
   ```
3. **If you see**: `RimBoard prerequisite check: 0 total foundations registered`
   - Foundations aren't being registered!

---

## Manual Fix (If GameMode Doesn't Work)

If the GameMode isn't spawning managers:

1. **In your level**, add **BP_SocketManager** actor
2. **In your level**, add **BP_ConstructionPhaseManager** actor
3. These actors will auto-initialize their singletons

---

## Common Issues

### Issue 1: Wrong GameMode
**Problem**: Using default GameMode, not BornToShineGameMode
**Solution**:
- Project Settings → Maps & Modes → Default GameMode: **BornToShineGameMode**
- OR Level → World Settings → GameMode Override: **BornToShineGameMode**

### Issue 2: GameMode Not Finding Character
**Problem**: BornToShineGameMode uses AMoonshineCharacter, but you're using BP_Character_Simple
**Solution**: This is fine! The managers spawn regardless of which character you use.

### Issue 3: Distance Too Far
**Problem**: Rim board is >500cm (5 meters) from foundation
**Solution**: Move rim board closer to foundation block

### Issue 4: Foundation Not Placed Successfully
**Problem**: Foundation was spawned but not "placed" (clicked)
**Solution**: Make sure you LEFT-CLICK to place foundations (not just spawning preview)

---

## Debug with New Logging

After compiling with the new changes, you'll see detailed logs:

**When placing foundation:**
```
LogTemp: Registered BP_FoundationBlock_C_0 with PhaseManager
```
OR
```
LogTemp: Error: ConstructionPhaseManager not found! Add BP_ConstructionPhaseManager to your level!
```

**When trying to place rim board:**
```
LogTemp: RimBoard prerequisite check: 1 total foundations registered
LogTemp: Found 1 pieces within 500cm of location (0.0, 0.0, 14.0)
LogTemp: Found nearby foundation at distance: 50.3 cm
```
OR
```
LogTemp: Warning: Cannot place rim board - no foundation blocks exist (PhaseManager has 0 foundations registered)
```

---

## Expected Workflow

1. **Start game** → See manager initialization logs
2. **Enter build mode (B)** → See build mode active
3. **Place foundation (click)** → See "Registered BP_FoundationBlock..."
4. **Cycle to rim board (Q)** → Rim board preview appears
5. **Move near foundation** → Rim board turns GREEN (snaps)
6. **Place rim board (click)** → Should place successfully

---

## Socket Snapping vs Prerequisites

Two separate systems:

**Prerequisites** (what's failing):
- Checks if foundations exist in PhaseManager
- Checks if foundation within 500cm
- Makes piece RED/invalid if fails

**Socket Snapping**:
- Only works if prerequisites pass (piece must be GREEN)
- Aligns rim board to foundation groove at 14cm
- Uses SocketManager to find best snap point

**Your issue is Prerequisites, not socket snapping!**

---

## Next Steps

1. **Compile** the project (new debug logging added)
2. **Start game** and check Output Log for initialization messages
3. **Place foundation** and verify registration log
4. **Try rim board** and check detailed prerequisite logs
5. **Report back** what you see in the logs

The new logging will tell us exactly where the problem is:
- Is GameMode running?
- Is PhaseManager created?
- Are foundations being registered?
- Is distance calculation working?

Check your logs and let me know what you see!
