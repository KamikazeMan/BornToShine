# Inventory UI Setup — Editor Steps

This file documents exactly how to create the inventory UI widgets in the Unreal Editor.
The C++ code is already done — this is the Blueprint/UMG side.

---

## 1. Create Folders

- In Content Browser: right-click → New Folder → `Content/UI/Inventory`
- Also create: `Content/UI/Backgrounds` (for the background texture import later)

---

## 2. Create WBP_InventorySlot (Child Widget)

1. Right-click in `Content/UI/Inventory` → User Interface → Widget Blueprint
2. Name it `WBP_InventorySlot`
3. Open it. In the Designer tab, build this hierarchy:

```
[CanvasPanel] (root — size 80x100 or whatever looks good)
  └─ [Border] "SlotBorder"
       - Brush Color: Dark Gray (0.15, 0.15, 0.15, 1.0)
       - Padding: 2px all sides
       └─ [VerticalBox]
            ├─ [SizeBox] height=60
            │    └─ [Image] "IconPlaceholder"
            │         - Color: varies by category (set in code)
            │         - Stretch: Fill
            ├─ [TextBlock] "ItemNameText"
            │    - Font Size: 10
            │    - Color: White
            │    - Justification: Center
            │    - Text: "Empty"
            └─ [TextBlock] "QuantityText"
                 - Font Size: 12, Bold
                 - Color: Yellow
                 - Justification: Right
                 - Text: ""
```

4. **Variables** (check "Is Variable" on each, and add custom variables in the Graph):
   - `ItemID` — Name type, default: None
   - `Quantity` — Integer, default: 0
   - `bIsSelected` — Boolean, default: false

5. **Selection highlight**: In the Graph, create a function `SetSelected(bool bSelected)`:
   - If true: set SlotBorder brush color to Yellow (1.0, 0.8, 0.0, 1.0)
   - If false: set SlotBorder brush color to Dark Gray (0.15, 0.15, 0.15, 1.0)

6. **Populate function**: Create `SetSlotData(Name InItemID, int32 InQuantity, FString InDisplayName)`:
   - Store ItemID and Quantity in variables
   - Set ItemNameText to InDisplayName (truncate to ~8 chars if needed)
   - Set QuantityText to "x" + InQuantity (hide if quantity is 1)
   - Set IconPlaceholder color based on category:
     - "Still" → Orange (0.8, 0.4, 0.1)
     - "Ingredient" → Green (0.2, 0.7, 0.2)
     - "Tool" → Blue (0.2, 0.4, 0.8)
     - default → Gray (0.5, 0.5, 0.5)

7. **Tooltip**: Select the root CanvasPanel → Details → check "Is Tool Tip" = false.
   Instead, override `OnMouseEnter` to show a simple tooltip text:
   - Right-click root → Add Event → OnMouseEnter
   - In the event, call "Set Tooltip Text" with: DisplayName + " (" + Category + ")"

8. **Click event**: Override `OnMouseButtonDown`:
   - Call a custom event dispatcher `OnSlotClicked` that passes `ItemID`
   - Parent grid will bind to this

---

## 3. Create WBP_InventoryGrid (Main Inventory UI)

1. Right-click in `Content/UI/Inventory` → User Interface → Widget Blueprint
2. Name it `WBP_InventoryGrid`
3. Open it. In the Designer tab:

```
[CanvasPanel] (root — anchored center, size 700x500)
  └─ [Border] "Background"
       - Brush: set to imported background image (or solid dark color for now)
       - Color: (0.1, 0.1, 0.12, 0.95)
       └─ [VerticalBox]
            ├─ [TextBlock] "TitleText"
            │    - Text: "SUPPLIES"
            │    - Font Size: 24, Bold
            │    - Color: Gold/Yellow
            │    - Justification: Center
            │    - Padding: top=10, bottom=10
            ├─ [UniformGridPanel] "SlotGrid"
            │    - Slot Padding: 4
            │    - Min Desired Slot Width: 100
            │    - Min Desired Slot Height: 110
            │    (leave EMPTY — slots are added at runtime)
            └─ [TextBlock] "StatusText"
                 - Text: "" (shows "Press Tab to close" or item count)
                 - Font Size: 12
                 - Justification: Center
                 - Padding: top=10
```

4. **Variables**:
   - `PlayerInventoryRef` — type: Inventory Component (Object Reference), Expose on Spawn: YES
   - `SlotWidgets` — type: Array of WBP_InventorySlot (Object Reference)
   - `SelectedSlotIndex` — Integer, default: -1

5. **Event Construct** (or "On Initialized"):
   ```
   // Get player pawn
   Get Player Pawn → Cast to MoonshineCharacter_Simple → Get Inventory → Store in PlayerInventoryRef
   
   // Create 24 slots (6 columns × 4 rows)
   For i = 0 to 23:
       Create Widget (class = WBP_InventorySlot)
       Add Child to UniformGridPanel:
           Row = i / 6
           Column = i % 6
       Add to SlotWidgets array
       Bind OnSlotClicked to a handler function
   
   // Bind inventory change event
   PlayerInventoryRef → OnInventoryChanged → Bind → call RefreshGrid
   
   // Initial populate
   Call RefreshGrid
   ```

6. **RefreshGrid function**:
   ```
   Get Items from PlayerInventoryRef
   For i = 0 to 23:
       If i < Items.Num():
           SlotWidgets[i].SetSlotData(Items[i].ItemID, Items[i].Quantity, Items[i].ItemID as String)
       Else:
           SlotWidgets[i].SetSlotData(NAME_None, 0, "Empty")
   ```

7. **OnSlotClicked handler**:
   ```
   // Deselect old
   If SelectedSlotIndex >= 0:
       SlotWidgets[SelectedSlotIndex].SetSelected(false)
   
   // Select new
   Find index of clicked slot
   SlotWidgets[index].SetSelected(true)
   SelectedSlotIndex = index
   
   // Log selection
   Print String: "Selected: " + ItemID
   ```

---

## 4. Player Blueprint Setup

1. Open `Content/Blueprints/BP_MoonshineCharacter_Simple` (or wherever the player BP lives)
2. In Class Defaults → Details panel → search "Inventory Widget Class"
3. Set it to `WBP_InventoryGrid`
4. Compile and Save

---

## 5. Test

1. PIE (Play In Editor)
2. Press `\` (backslash) to grant still parts
3. Press `Tab` to open inventory — should see 10 items in the grid
4. Press `Tab` again to close
5. Press `P` to dump inventory to log (verify counts)

---

## 6. Data Table (Optional — for display names and icons)

1. Right-click in `Content/Data` → Miscellaneous → Data Table
2. Pick Row Structure: `FItemDataRow`
3. Name it `DT_Items`
4. Add rows for each item (CinderBlockStand, Pot, Cap, etc.)
5. On BP_MoonshineCharacter_Simple → Inventory component → set Item Data Table to `DT_Items`

Without the data table, items still work — they just use their ItemID as the display name.
