// Born To Shine - Construction System Type Definitions

#pragma once

#include "CoreMinimal.h"
#include "ConstructionTypes.generated.h"

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
	Header				UMETA(DisplayName = "Header"),
	Rafter				UMETA(DisplayName = "Rafter")
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

	// Wall Sockets
	Wall_Bottom_Plate			UMETA(DisplayName = "Wall Bottom Plate"),
	Wall_Top_Plate				UMETA(DisplayName = "Wall Top Plate"),
	Wall_Stud_Bottom			UMETA(DisplayName = "Wall Stud Bottom"),
	Wall_Stud_Top				UMETA(DisplayName = "Wall Stud Top"),

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
