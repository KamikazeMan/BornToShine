// Born To Shine - Construction System Type Definitions

#pragma once

#include "CoreMinimal.h"
#include "ConstructionTypes.generated.h"

// Forward declarations
class ABuildablePiece;
class UTexture2D;

/**
 * Defines the type of construction piece
 */
UENUM(BlueprintType)
enum class EPieceType : uint8
{
	None				UMETA(DisplayName = "None"),
	Foundation			UMETA(DisplayName = "Foundation Block"),
	RimBoard			UMETA(DisplayName = "Rim Board (2x6)"),
	FloorJoist			UMETA(DisplayName = "Floor Joist"),
	Plywood				UMETA(DisplayName = "Plywood Sheathing"),
	WallStud			UMETA(DisplayName = "Wall Stud"),
	WallPlate			UMETA(DisplayName = "Wall Plate"),
	CornerPost			UMETA(DisplayName = "Corner Post"),
	Header				UMETA(DisplayName = "Header"),
	Rafter				UMETA(DisplayName = "Rafter"),
	DoorFrame			UMETA(DisplayName = "Door Frame"),
	TopPlate			UMETA(DisplayName = "Top Plate"),
	DoubleTopPlate		UMETA(DisplayName = "Double Top Plate"),
	RidgePost			UMETA(DisplayName = "Ridge Post"),
	RidgeBoard			UMETA(DisplayName = "Ridge Board"),
	FasciaBoard			UMETA(DisplayName = "Fascia Board")
};

/**
 * Defines the construction phase - enforces build order
 */
UENUM(BlueprintType)
enum class EConstructionPhase : uint8
{
	Foundation			UMETA(DisplayName = "Foundation Phase"),
	FloorFrame			UMETA(DisplayName = "Floor Frame Phase"),
	FloorSheathing		UMETA(DisplayName = "Floor Sheathing Phase"),
	WallFrame			UMETA(DisplayName = "Wall Frame Phase"),
	WallSheathing		UMETA(DisplayName = "Wall Sheathing Phase"),
	RoofFrame			UMETA(DisplayName = "Roof Frame Phase"),
	RoofSheathing		UMETA(DisplayName = "Roof Sheathing Phase")
};

/**
 * Defines socket type for compatibility checking
 */
UENUM(BlueprintType)
enum class EConstructionSocketType : uint8
{
	// Foundation Sockets
	Foundation_Corner			UMETA(DisplayName = "Foundation Corner (Top)"),
	Foundation_Side				UMETA(DisplayName = "Foundation Side"),

	// Rim Board Sockets
	RimBoard_Bottom_End			UMETA(DisplayName = "Rim Board Bottom End"),
	RimBoard_Top_Face			UMETA(DisplayName = "Rim Board Top Face"),
	RimBoard_Side_Face			UMETA(DisplayName = "Rim Board Side Face"),
	RimBoard_End_Corner			UMETA(DisplayName = "Rim Board End Corner"),

	// Joist Sockets
	Joist_End					UMETA(DisplayName = "Joist End"),
	Joist_Top_Face				UMETA(DisplayName = "Joist Top Face"),

	// Plywood Sockets
	Plywood_Corner				UMETA(DisplayName = "Plywood Corner"),
	Plywood_Edge				UMETA(DisplayName = "Plywood Edge"),

	// Bottom Plate Sockets (2x4 sole plate on top of subfloor)
	BottomPlate_Bottom			UMETA(DisplayName = "Bottom Plate Bottom"),
	BottomPlate_End				UMETA(DisplayName = "Bottom Plate End"),

	// Wall Sockets
	Wall_Bottom_Plate			UMETA(DisplayName = "Wall Bottom Plate"),
	Wall_Top_Plate				UMETA(DisplayName = "Wall Top Plate"),
	Wall_Stud_Bottom			UMETA(DisplayName = "Wall Stud Bottom"),
	Wall_Stud_Top				UMETA(DisplayName = "Wall Stud Top"),

	// Corner Post Sockets
	CornerPost_Bottom			UMETA(DisplayName = "Corner Post Bottom"),
	CornerPost_Top				UMETA(DisplayName = "Corner Post Top"),
	CornerPost_Seat				UMETA(DisplayName = "Corner Post Seat"),

	// Door Frame Sockets
	DoorFrame_Bottom			UMETA(DisplayName = "Door Frame Bottom"),
	DoorFrame_Top				UMETA(DisplayName = "Door Frame Top"),

	// Top Plate Sockets (first top plate, sits on studs/posts)
	TopPlate_Bottom				UMETA(DisplayName = "Top Plate Bottom"),
	TopPlate_End				UMETA(DisplayName = "Top Plate End"),
	TopPlate_Top				UMETA(DisplayName = "Top Plate Top Face"),

	// Double Top Plate Sockets (second plate, stacks on first)
	DoubleTopPlate_Bottom		UMETA(DisplayName = "Double Top Plate Bottom"),
	DoubleTopPlate_End			UMETA(DisplayName = "Double Top Plate End"),

	// Ridge Post Sockets (3-laminated 2x6 with pocket)
	RidgePost_Bottom			UMETA(DisplayName = "Ridge Post Bottom"),
	RidgePost_Pocket			UMETA(DisplayName = "Ridge Post Pocket (Top)"),

	// Ridge Board Sockets (horizontal beam at peak)
	RidgeBoard_End				UMETA(DisplayName = "Ridge Board End"),
	RidgeBoard_Side				UMETA(DisplayName = "Ridge Board Side (Rafter Attach)"),

	// Rafter Sockets (angled roof members)
	Rafter_Ridge				UMETA(DisplayName = "Rafter Ridge End"),
	Rafter_BirdsMouth			UMETA(DisplayName = "Rafter Birdsmouth"),
	Rafter_Tail					UMETA(DisplayName = "Rafter Tail End"),

	// Fascia Board Sockets
	Fascia_End					UMETA(DisplayName = "Fascia Board End"),
	Fascia_RafterTail			UMETA(DisplayName = "Fascia Rafter Tail Attach"),

	None						UMETA(DisplayName = "None")
};

/**
 * Defines the state of a buildable piece
 */
UENUM(BlueprintType)
enum class EPieceState : uint8
{
	Preview				UMETA(DisplayName = "Preview (Ghost)"),
	Placed				UMETA(DisplayName = "Placed (Not Secured)"),
	Nailed				UMETA(DisplayName = "Nailed (Locked)")
};

/**
 * Defines socket orientation for alignment
 */
UENUM(BlueprintType)
enum class ESocketOrientation : uint8
{
	Horizontal			UMETA(DisplayName = "Horizontal"),
	Vertical			UMETA(DisplayName = "Vertical"),
	Any					UMETA(DisplayName = "Any")
};

/**
 * Individual socket definition with position and metadata
 */
USTRUCT(BlueprintType)
struct FConstructionSocket
{
	GENERATED_BODY()

	// Unique identifier for this socket (e.g., "Foundation_Corner_NE")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FName SocketName;

	// Type of this socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	EConstructionSocketType SocketType;

	// Local position relative to piece origin
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FVector LocalPosition;

	// Local rotation relative to piece origin
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	FRotator LocalRotation;

	// Socket orientation for alignment checking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Socket")
	ESocketOrientation Orientation;

	// Is this socket currently occupied?
	UPROPERTY(BlueprintReadWrite, Category = "Socket")
	bool bIsOccupied;

	// Reference to the piece connected to this socket
	UPROPERTY(BlueprintReadWrite, Category = "Socket")
	TWeakObjectPtr<class ABuildablePiece> ConnectedPiece;

	FConstructionSocket()
		: SocketName(NAME_None)
		, SocketType(EConstructionSocketType::None)
		, LocalPosition(FVector::ZeroVector)
		, LocalRotation(FRotator::ZeroRotator)
		, Orientation(ESocketOrientation::Any)
		, bIsOccupied(false)
		, ConnectedPiece(nullptr)
	{
	}
};

/**
 * Socket compatibility rule - defines which sockets can connect
 */
USTRUCT(BlueprintType)
struct FSocketCompatibilityRule
{
	GENERATED_BODY()

	// The socket type that is looking for a connection
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	EConstructionSocketType SourceSocketType;

	// List of socket types that can connect to the source
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	TArray<EConstructionSocketType> CompatibleSocketTypes;

	// Required construction phase for this connection to be valid
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	EConstructionPhase RequiredPhase;

	// Maximum distance for snap detection (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	float SnapDistance;

	// Should alignment be checked (rotation)?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	bool bCheckAlignment;

	// Maximum angle difference for alignment (degrees)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Compatibility")
	float MaxAlignmentAngle;

	FSocketCompatibilityRule()
		: SourceSocketType(EConstructionSocketType::None)
		, RequiredPhase(EConstructionPhase::Foundation)
		, SnapDistance(100.0f)
		, bCheckAlignment(true)
		, MaxAlignmentAngle(5.0f)
	{
	}
};

/**
 * Pure data struct holding all snap detection results for a single candidate.
 * Used by the phase-separated snap pipeline: Detect -> Select -> Apply
 */
USTRUCT(BlueprintType)
struct FSnapCandidate
{
	GENERATED_BODY()

	// Which socket on the source piece matched
	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	FName SourceSocketName;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	EConstructionSocketType SourceSocketType;

	// Which socket on the target piece matched
	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	FName TargetSocketName;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	EConstructionSocketType TargetSocketType;

	// The target piece
	UPROPERTY()
	ABuildablePiece* TargetPiece;

	// Computed final position/rotation for the source actor
	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	FVector SnapLocation;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	FRotator SnapRotation;

	// Scoring
	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	float Score;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	int32 Priority;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	float Distance;

	// Connection type info
	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	bool bIsCornerSnap;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	bool bIsInlineSnap;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	bool bIsDualEndSnap;

	// For dual-end snaps
	UPROPERTY()
	ABuildablePiece* SecondTargetPiece;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	FName SecondTargetSocketName;

	UPROPERTY(BlueprintReadOnly, Category = "Snap")
	int32 AutoResizeLengthFeet; // 0 = no resize needed

	FSnapCandidate()
		: SourceSocketType(EConstructionSocketType::None)
		, TargetSocketType(EConstructionSocketType::None)
		, TargetPiece(nullptr)
		, SnapLocation(FVector::ZeroVector)
		, SnapRotation(FRotator::ZeroRotator)
		, Score(0.f)
		, Priority(0)
		, Distance(FLT_MAX)
		, bIsCornerSnap(false)
		, bIsInlineSnap(false)
		, bIsDualEndSnap(false)
		, SecondTargetPiece(nullptr)
		, AutoResizeLengthFeet(0)
	{
	}

	bool IsValid() const { return TargetPiece != nullptr; }
};

/**
 * Rich info for each piece type in the radial menu.
 * Configure in the editor on BuildingComponent::PieceTypeInfos.
 */
USTRUCT(BlueprintType)
struct FPieceTypeInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece Info")
	EPieceType PieceType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece Info")
	FString DisplayName;

	// Short label shown in segment (e.g. "8ft", "16in OC", "4x8")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece Info")
	FString Subtitle;

	// Icon texture — assign in the editor via soft object path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece Info")
	TSoftObjectPtr<UTexture2D> Icon;

	// If false, segment is drawn grayed out
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Piece Info")
	bool bAvailable;

	FPieceTypeInfo()
		: PieceType(EPieceType::None)
		, bAvailable(true)
	{
	}
};
