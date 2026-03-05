// Born To Shine - Plywood Sheet Implementation

#include "PlywoodSheet.h"
#include "Components/StaticMeshComponent.h"

APlywoodSheet::APlywoodSheet()
{
	PieceType = EPieceType::Plywood;

	// 4'x8' sheet, 3/4" thick
	SheetWidth = 121.92f;   // 4 feet in cm
	SheetLength = 243.84f;  // 8 feet in cm
	SheetThickness = 1.905f; // 3/4 inch in cm

	// Scene root for clean actor transform (same pattern as RimBoard)
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	// Plywood is auto-nailed (glued/nailed in real framing)
	bAutoNailOnPlace = false;
}

void APlywoodSheet::BeginPlay()
{
	Super::BeginPlay();

	// Scale mesh to cover rim board overhang on frame edges.
	// The frame outer dimension = board center-to-center + one rim board width,
	// so standard 4x8 sheets are slightly too small. Extending the mesh by
	// RimBoardWidth across both axes covers the overhang on all frame edges.
	if (MeshComponent)
	{
		const float RimBoardWidth = 3.81f; // 1.5 inch = 2x6 lumber width
		float OverhangScale = (SheetLength + RimBoardWidth) / SheetLength;
		FVector MeshScale = MeshComponent->GetRelativeScale3D();
		MeshComponent->SetRelativeScale3D(MeshScale * FVector(OverhangScale, OverhangScale, 1.0f));

		UE_LOG(LogTemp, Log, TEXT("PLYWOOD: Mesh scaled by %.4f to cover rim board overhang (%.1fcm -> %.1fcm x %.1fcm -> %.1fcm)"),
			OverhangScale, SheetLength, SheetLength + RimBoardWidth, SheetWidth, SheetWidth * OverhangScale);
	}

	// Diagnostic: Print mesh bounds to verify centering
	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		UE_LOG(LogTemp, Log, TEXT("PLYWOOD MESH BOUNDS: Origin=(%.2f, %.2f, %.2f) Extent=(%.2f, %.2f, %.2f) Min.Z=%.2f Max.Z=%.2f"),
			Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z,
			Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z,
			Bounds.Origin.Z - Bounds.BoxExtent.Z,
			Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}
}

void APlywoodSheet::InitializeSockets()
{
	Sockets.Empty();

	CreateCornerSockets();
	CreateEdgeSockets();

	UE_LOG(LogTemp, Log, TEXT("PlywoodSheet: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void APlywoodSheet::CreateCornerSockets()
{
	float HalfLength = SheetLength / 2.0f;  // 121.92
	float HalfWidth = SheetWidth / 2.0f;    // 60.96

	// All corner sockets at the bottom face of the plywood (Z = -SheetThickness/2)
	// so they align with the top of joists/rim boards below
	float SocketZ = -SheetThickness / 2.0f;

	struct CornerDef {
		FName Name;
		float X;
		float Y;
	};

	CornerDef Corners[] = {
		{ FName(TEXT("PlywoodCorner_NE")), HalfLength,  HalfWidth  },
		{ FName(TEXT("PlywoodCorner_NW")), -HalfLength, HalfWidth  },
		{ FName(TEXT("PlywoodCorner_SE")), HalfLength,  -HalfWidth },
		{ FName(TEXT("PlywoodCorner_SW")), -HalfLength, -HalfWidth },
	};

	for (const auto& C : Corners)
	{
		FConstructionSocket Socket;
		Socket.SocketName = C.Name;
		Socket.SocketType = EConstructionSocketType::Plywood_Corner;
		Socket.LocalPosition = FVector(C.X, C.Y, SocketZ);
		Socket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f); // Facing downward
		Socket.bIsOccupied = false;
		Sockets.Add(Socket);
	}

	UE_LOG(LogTemp, Log, TEXT("PlywoodSheet: Created 4 corner sockets at Z=%.2f"), SocketZ);
}

void APlywoodSheet::CreateEdgeSockets()
{
	float HalfLength = SheetLength / 2.0f;
	float HalfWidth = SheetWidth / 2.0f;
	float SocketZ = -SheetThickness / 2.0f;

	// Edge sockets along the bottom face at 16" OC spacing.
	// These snap to Joist_Top_Face, RimBoard_Top_Face, or adjacent Plywood_Edge.
	float Spacing = 40.64f; // 16" OC

	int32 Count = 0;

	// Long edges (along X axis, at Y = ±HalfWidth)
	// These rest on rim boards (boards 1 and 3)
	for (int32 Side = 0; Side < 2; Side++)
	{
		float Y = (Side == 0) ? -HalfWidth : HalfWidth;
		FString SideName = (Side == 0) ? TEXT("South") : TEXT("North");

		float CurrentX = -HalfLength + Spacing;
		while (CurrentX < HalfLength - Spacing / 2.0f)
		{
			FConstructionSocket Socket;
			Socket.SocketName = FName(*FString::Printf(TEXT("PlywoodEdge_%s_%d"), *SideName, Count));
			Socket.SocketType = EConstructionSocketType::Plywood_Edge;
			Socket.LocalPosition = FVector(CurrentX, Y, SocketZ);
			Socket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
			Socket.bIsOccupied = false;
			Sockets.Add(Socket);

			CurrentX += Spacing;
			Count++;
		}
	}

	// Short edges (along Y axis, at X = ±HalfLength)
	// These rest on the end boards (boards 2 and 4) or on sheet-to-sheet joints
	for (int32 Side = 0; Side < 2; Side++)
	{
		float X = (Side == 0) ? -HalfLength : HalfLength;
		FString SideName = (Side == 0) ? TEXT("West") : TEXT("East");

		float CurrentY = -HalfWidth + Spacing;
		while (CurrentY < HalfWidth - Spacing / 2.0f)
		{
			FConstructionSocket Socket;
			Socket.SocketName = FName(*FString::Printf(TEXT("PlywoodEdge_%s_%d"), *SideName, Count));
			Socket.SocketType = EConstructionSocketType::Plywood_Edge;
			Socket.LocalPosition = FVector(X, CurrentY, SocketZ);
			Socket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
			Socket.bIsOccupied = false;
			Sockets.Add(Socket);

			CurrentY += Spacing;
			Count++;
		}
	}

	// Interior sockets at joist crossing points (where joists cross under the sheet)
	// These ensure plywood snaps are detected over each joist
	{
		float CurrentX = -HalfLength + Spacing;
		while (CurrentX < HalfLength - Spacing / 2.0f)
		{
			FConstructionSocket Socket;
			Socket.SocketName = FName(*FString::Printf(TEXT("PlywoodEdge_Center_%d"), Count));
			Socket.SocketType = EConstructionSocketType::Plywood_Edge;
			Socket.LocalPosition = FVector(CurrentX, 0.0f, SocketZ);
			Socket.LocalRotation = FRotator(90.0f, 0.0f, 0.0f);
			Socket.bIsOccupied = false;
			Sockets.Add(Socket);

			CurrentX += Spacing;
			Count++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PlywoodSheet: Created %d edge sockets at 16\" OC spacing"), Count);
}
