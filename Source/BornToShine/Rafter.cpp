// Born To Shine - Rafter Implementation (Procedural Mesh with Dynamic Cuts)

#include "Rafter.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"

ARafter::ARafter()
{
	PieceType = EPieceType::Rafter;

	// 2x6 lumber cross-section
	RafterWidth = 3.81f;   // 1.5 inches
	RafterDepth = 13.97f;  // 5.5 inches

	// Default 6/12 pitch
	PitchRatio = 6.0f;

	// Default run = 4ft (half of 8ft building)
	RunDistanceCm = 121.92f;

	// Default 12" overhang
	OverhangCm = 30.48f;

	// Birdsmouth: seat cut = 2/3 of rafter depth
	BirdsmouthSeatFraction = 2.0f / 3.0f;

	// Scene root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Create procedural mesh component (replaces the static mesh for rafters)
	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	ProceduralMesh->SetupAttachment(SceneRoot);
	ProceduralMesh->bUseComplexAsSimpleCollision = false;

	// Hide the base class static mesh (we use procedural mesh instead)
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);

	RafterMaterial = nullptr;
}

void ARafter::BeginPlay()
{
	Super::BeginPlay();

	// Generate the initial mesh based on default pitch
	GenerateRafterMesh();

	UE_LOG(LogTemp, Log, TEXT("Rafter: BeginPlay - Pitch=%.1f/12 (%.1f deg), Run=%.1fcm, Slope=%.1fcm, Sockets=%d"),
		PitchRatio, GetPitchAngleDegrees(), RunDistanceCm, GetSlopeLengthCm(), Sockets.Num());
}

void ARafter::InitializeSockets()
{
	Sockets.Empty();

	CreateRidgeEndSocket();
	CreateBirdsmouthSocket();
	CreateTailEndSocket();

	UE_LOG(LogTemp, Log, TEXT("Rafter: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

float ARafter::GetPitchAngleRadians() const
{
	// pitch = rise/run where PitchRatio = rise per 12 units of run
	// tan(angle) = PitchRatio / 12
	return FMath::Atan2(PitchRatio, 12.0f);
}

float ARafter::GetPitchAngleDegrees() const
{
	return FMath::RadiansToDegrees(GetPitchAngleRadians());
}

float ARafter::GetSlopeLengthCm() const
{
	// Total slope length from ridge plumb cut to tail end
	float RiseTotal = (PitchRatio / 12.0f) * RunDistanceCm;
	float MainSlope = FMath::Sqrt(RunDistanceCm * RunDistanceCm + RiseTotal * RiseTotal);

	// Add overhang along the slope
	float OverhangSlope = OverhangCm / FMath::Cos(GetPitchAngleRadians());

	return MainSlope + OverhangSlope;
}

void ARafter::SetPitch(float NewPitchRatio, float NewRunCm)
{
	PitchRatio = FMath::Clamp(NewPitchRatio, 1.0f, 24.0f);
	if (NewRunCm > 0.0f) RunDistanceCm = NewRunCm;

	GenerateRafterMesh();
	RegenerateSockets();

	UE_LOG(LogTemp, Log, TEXT("Rafter: Pitch set to %.1f/12 (%.1f deg), Run=%.1fcm, Slope=%.1fcm"),
		PitchRatio, GetPitchAngleDegrees(), RunDistanceCm, GetSlopeLengthCm());
}

FString ARafter::GetRafterDisplayString() const
{
	return FString::Printf(TEXT("%.0f/12 pitch, %.0f\" run, %.1f\" slope"),
		PitchRatio, RunDistanceCm / 2.54f, GetSlopeLengthCm() / 2.54f);
}

void ARafter::ScalePiece(float ScaleDelta)
{
	// Rafters: scroll wheel disabled
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

FVector ARafter::GetTailEndWorldPosition() const
{
	// The tail end socket position in world space
	for (const FConstructionSocket& Socket : Sockets)
	{
		if (Socket.SocketName == FName("RafterTail"))
		{
			return GetActorTransform().TransformPosition(Socket.LocalPosition);
		}
	}
	return GetActorLocation();
}

void ARafter::CreateRidgeEndSocket()
{
	// Ridge end: at the top of the rafter where it meets the ridge board
	// Position: at the ridge board center line (the very top of the slope)
	float PitchAngle = GetPitchAngleRadians();

	FConstructionSocket RidgeSocket;
	RidgeSocket.SocketName = FName(TEXT("RafterRidge"));
	RidgeSocket.SocketType = EConstructionSocketType::Rafter_Ridge;
	// Ridge end is at the high point: local origin
	RidgeSocket.LocalPosition = FVector::ZeroVector;
	RidgeSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	RidgeSocket.Orientation = ESocketOrientation::Any;
	RidgeSocket.bIsOccupied = false;
	Sockets.Add(RidgeSocket);
}

void ARafter::CreateBirdsmouthSocket()
{
	// Birdsmouth: where the rafter sits on the top plate
	// Position along the slope at the run distance from ridge
	float PitchAngle = GetPitchAngleRadians();
	float CosA = FMath::Cos(PitchAngle);
	float SinA = FMath::Sin(PitchAngle);

	// Birdsmouth is at the run distance along horizontal from ridge
	// In local space (rafter aligned along its slope), the birdsmouth
	// is at the point where horizontal distance = RunDistanceCm
	float SlopeDistToWall = RunDistanceCm / CosA;

	// Local X = along the slope (from ridge toward tail)
	// Local Z = perpendicular to slope (toward roof surface)
	// We position the rafter so origin is at ridge end
	FConstructionSocket BirdsmouthSocket;
	BirdsmouthSocket.SocketName = FName(TEXT("RafterBirdsmouth"));
	BirdsmouthSocket.SocketType = EConstructionSocketType::Rafter_BirdsMouth;
	// The birdsmouth is RunDistanceCm horizontally and down by the rise
	float Rise = (PitchRatio / 12.0f) * RunDistanceCm;
	BirdsmouthSocket.LocalPosition = FVector(RunDistanceCm, 0.0f, -Rise);
	BirdsmouthSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	BirdsmouthSocket.Orientation = ESocketOrientation::Any;
	BirdsmouthSocket.bIsOccupied = false;
	Sockets.Add(BirdsmouthSocket);
}

void ARafter::CreateTailEndSocket()
{
	// Tail end: at the overhang end, past the wall line
	float PitchAngle = GetPitchAngleRadians();
	float CosA = FMath::Cos(PitchAngle);
	float SinA = FMath::Sin(PitchAngle);

	float Rise = (PitchRatio / 12.0f) * RunDistanceCm;
	float TailHorizDist = RunDistanceCm + OverhangCm;
	float TailRise = (PitchRatio / 12.0f) * TailHorizDist;

	FConstructionSocket TailSocket;
	TailSocket.SocketName = FName(TEXT("RafterTail"));
	TailSocket.SocketType = EConstructionSocketType::Rafter_Tail;
	TailSocket.LocalPosition = FVector(TailHorizDist, 0.0f, -TailRise);
	TailSocket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	TailSocket.Orientation = ESocketOrientation::Any;
	TailSocket.bIsOccupied = false;
	Sockets.Add(TailSocket);
}

void ARafter::RegenerateSockets()
{
	Sockets.Empty();

	CreateRidgeEndSocket();
	CreateBirdsmouthSocket();
	CreateTailEndSocket();

	UE_LOG(LogTemp, Log, TEXT("Rafter: Regenerated %d sockets"), Sockets.Num());
}

void ARafter::GenerateRafterMesh()
{
	if (!ProceduralMesh) return;

	ProceduralMesh->ClearAllMeshSections();

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;

	BuildRafterGeometry(Vertices, Triangles, Normals, UVs);

	if (Vertices.Num() == 0) return;

	// Create tangents (required for lighting)
	TArray<FProcMeshTangent> Tangents;
	Tangents.SetNum(Vertices.Num());
	for (int32 i = 0; i < Tangents.Num(); i++)
	{
		Tangents[i] = FProcMeshTangent(FVector(1.0f, 0.0f, 0.0f), false);
	}

	// Vertex colors
	TArray<FColor> VertexColors;
	VertexColors.SetNum(Vertices.Num());
	FColor WoodColor(200, 170, 120); // Light wood color
	for (int32 i = 0; i < VertexColors.Num(); i++)
	{
		VertexColors[i] = WoodColor;
	}

	ProceduralMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, true);

	// Apply material if available
	if (RafterMaterial)
	{
		ProceduralMesh->SetMaterial(0, RafterMaterial);
	}

	UE_LOG(LogTemp, Log, TEXT("Rafter: Generated mesh with %d verts, %d tris"),
		Vertices.Num(), Triangles.Num() / 3);
}

void ARafter::BuildRafterGeometry(
	TArray<FVector>& Vertices,
	TArray<int32>& Triangles,
	TArray<FVector>& Normals,
	TArray<FVector2D>& UVs)
{
	// ===================================================================
	// Rafter profile in local space:
	//
	// The rafter origin is at the RIDGE END (where it meets the ridge board).
	// The rafter slopes downward from origin toward positive X.
	//
	// Local X = horizontal distance from ridge (increasing toward tail)
	// Local Y = across the rafter thickness (width = 1.5")
	// Local Z = vertical (positive = up)
	//
	// The rafter is essentially a 2x6 board tilted at the pitch angle,
	// with three modifications:
	//   1. Plumb cut at ridge end (vertical cut face)
	//   2. Birdsmouth cut at the top plate (notch)
	//   3. Tail cut at overhang end (plumb or vertical cut)
	// ===================================================================

	float PitchAngle = GetPitchAngleRadians();
	float CosA = FMath::Cos(PitchAngle);
	float SinA = FMath::Sin(PitchAngle);

	float HalfWidth = RafterWidth / 2.0f;  // 1.905cm
	float Depth = RafterDepth;              // 13.97cm

	// Rise and run
	float TotalRise = (PitchRatio / 12.0f) * RunDistanceCm;
	float TotalHoriz = RunDistanceCm + OverhangCm;
	float TotalTailRise = (PitchRatio / 12.0f) * TotalHoriz;

	// Birdsmouth parameters
	float BirdsmouthSeatDepth = Depth * BirdsmouthSeatFraction; // How deep the seat cut is
	float BirdsmouthHeelHeight = Depth - BirdsmouthSeatDepth;    // Remaining heel height

	// Along-slope direction vectors
	FVector SlopeDir(CosA, 0.0f, -SinA);    // Direction along the slope (downhill)
	FVector SlopePerp(-SinA, 0.0f, -CosA);  // Perpendicular to slope (into the rafter depth)

	// Key points along the rafter (all in local space, origin at ridge):
	//
	// Point layout (side view, exaggerated):
	//
	//   Ridge  (plumb cut face)
	//     |\
	//     | \  <- rafter top edge
	//     |  \
	//     |   \  birdsmouth
	//     |    |_|  <- seat cut
	//     |    |     \
	//     |    |      \  <- tail
	//     plumb        tail cut
	//     cut

	// --- Ridge end (plumb cut) ---
	// Plumb cut is a vertical face at X=0. The rafter top edge and bottom edge
	// meet this vertical plane.
	// Top of rafter at ridge: origin + offset along slope perpendicular
	FVector RidgeTop = FVector(0.0f, 0.0f, 0.0f); // Top edge of rafter at ridge
	FVector RidgeBot = RidgeTop + SlopePerp * Depth; // Bottom edge at ridge

	// --- Birdsmouth location ---
	// Horizontal distance from ridge to wall line = RunDistanceCm
	// Along slope distance = RunDistanceCm / CosA
	float SlopeDistToWall = RunDistanceCm / CosA;
	FVector WallTopEdge = RidgeTop + SlopeDir * SlopeDistToWall;
	FVector WallBotEdge = WallTopEdge + SlopePerp * Depth;

	// Birdsmouth seat cut: a horizontal cut at the bottom of the rafter
	// where it sits on the top plate. The seat is horizontal, cutting into
	// the bottom edge by BirdsmouthSeatDepth (measured perpendicular to rafter).
	//
	// In world coordinates:
	// - Seat cut start: point on bottom edge at the wall line
	// - Seat cut goes horizontal (level) for the plate width
	// - Heel cut goes vertical from the seat up to the rafter bottom edge

	// The birdsmouth is at the wall line. The seat cut is horizontal.
	// Calculate the birdsmouth geometry:
	float BmSeatCutLen = BirdsmouthSeatDepth / SinA; // Horizontal length of the seat cut
	// Actually, the seat cut depth perpendicular to the rafter = BirdsmouthSeatDepth
	// This translates to a vertical drop = BirdsmouthSeatDepth * CosA
	// and a horizontal offset = BirdsmouthSeatDepth * SinA

	// Point where the heel cut meets the rafter bottom edge (before birdsmouth)
	FVector BmHeelTop = WallBotEdge; // Bottom edge of rafter at wall line

	// Point at the bottom of the birdsmouth (where it sits on the plate)
	// This is straight down (vertical) from the rafter bottom edge
	float BmVerticalDrop = BirdsmouthSeatDepth * CosA;
	float BmHorizontalShift = BirdsmouthSeatDepth * SinA;
	FVector BmSeatInner = BmHeelTop + FVector(BmHorizontalShift, 0.0f, -BmVerticalDrop);

	// Seat cut extends horizontally (level) toward the outside of the building
	float SeatHorizLen = BirdsmouthSeatDepth / FMath::Tan(PitchAngle);
	FVector BmSeatOuter = BmSeatInner + FVector(SeatHorizLen, 0.0f, 0.0f);

	// The outer seat point connects back to the rafter bottom edge
	// at a point further along the slope
	// Actually, for simplicity, the birdsmouth is:
	// 1. Heel cut: vertical line from rafter bottom at wall to seat level
	// 2. Seat cut: horizontal line from heel to where it meets rafter bottom again
	// The seat cut continues until it intersects the rafter's bottom edge line

	// --- Tail end ---
	float SlopeDistToTail = TotalHoriz / CosA;
	FVector TailTop = RidgeTop + SlopeDir * SlopeDistToTail;
	FVector TailBot = TailTop + SlopePerp * Depth;

	// Tail cut is a plumb (vertical) cut at the tail end
	// The tail cut face is vertical, similar to the ridge plumb cut

	// ===================================================================
	// Build the 3D mesh as an extruded profile along Y axis (width)
	// Each face of the rafter is a quad (2 triangles).
	// We define the 2D profile (side view) then extrude along Y.
	// ===================================================================

	// Define the 2D profile points (side view in XZ plane)
	// Going clockwise from the ridge top:
	TArray<FVector> Profile;

	// 1. Ridge top (plumb cut top)
	Profile.Add(RidgeTop);

	// 2. Rafter top edge at birdsmouth location (just before the wall)
	Profile.Add(WallTopEdge);

	// 3. Rafter top edge at tail
	Profile.Add(TailTop);

	// 4. Tail bottom (plumb cut at tail)
	Profile.Add(TailBot);

	// 5. Rafter bottom edge just past birdsmouth (after seat cut)
	// The rafter bottom resumes after the birdsmouth
	Profile.Add(BmSeatOuter);

	// 6. Birdsmouth seat inner point (horizontal seat)
	Profile.Add(BmSeatInner);

	// 7. Birdsmouth heel (bottom edge at wall, before heel cut)
	// Actually the heel is where the vertical cut meets the rafter bottom
	// coming from the ridge side
	Profile.Add(BmHeelTop);

	// 8. Ridge bottom (plumb cut bottom)
	Profile.Add(RidgeBot);

	int32 NumProfilePts = Profile.Num();

	// Extrude along Y for both faces (left face at -HalfWidth, right face at +HalfWidth)
	// Left face profile
	for (int32 i = 0; i < NumProfilePts; i++)
	{
		Vertices.Add(FVector(Profile[i].X, -HalfWidth, Profile[i].Z));
	}
	// Right face profile
	for (int32 i = 0; i < NumProfilePts; i++)
	{
		Vertices.Add(FVector(Profile[i].X, HalfWidth, Profile[i].Z));
	}

	// --- Generate triangles for each face ---

	// Helper lambda to add a quad (two triangles) from 4 vertex indices
	auto AddQuad = [&Triangles](int32 V0, int32 V1, int32 V2, int32 V3)
	{
		// Triangle 1: V0, V1, V2
		Triangles.Add(V0); Triangles.Add(V1); Triangles.Add(V2);
		// Triangle 2: V0, V2, V3
		Triangles.Add(V0); Triangles.Add(V2); Triangles.Add(V3);
	};

	int32 L = 0;           // Left face offset
	int32 R = NumProfilePts; // Right face offset

	// Side faces (connecting left and right profiles)
	for (int32 i = 0; i < NumProfilePts; i++)
	{
		int32 Next = (i + 1) % NumProfilePts;

		// Outer face (left side, viewed from left)
		AddQuad(L + i, L + Next, R + Next, R + i);
	}

	// Left face (cap) - triangulate as a fan from vertex 0
	for (int32 i = 1; i < NumProfilePts - 1; i++)
	{
		Triangles.Add(L + 0);
		Triangles.Add(L + i + 1);
		Triangles.Add(L + i);
	}

	// Right face (cap) - triangulate as a fan from vertex 0 (reverse winding)
	for (int32 i = 1; i < NumProfilePts - 1; i++)
	{
		Triangles.Add(R + 0);
		Triangles.Add(R + i);
		Triangles.Add(R + i + 1);
	}

	// --- Generate normals (flat shading approximation) ---
	Normals.SetNum(Vertices.Num());
	for (int32 i = 0; i < NumProfilePts; i++)
	{
		// Left side normals point left
		Normals[L + i] = FVector(0.0f, -1.0f, 0.0f);
		// Right side normals point right
		Normals[R + i] = FVector(0.0f, 1.0f, 0.0f);
	}

	// Recalculate normals from triangles for better results
	// For now, use flat normals (procedural mesh will auto-calculate if needed)

	// --- Generate UVs ---
	UVs.SetNum(Vertices.Num());
	float SlopeLen = GetSlopeLengthCm();
	for (int32 i = 0; i < NumProfilePts; i++)
	{
		float U = FVector::Dist(Profile[i], Profile[0]) / FMath::Max(SlopeLen, 1.0f);
		UVs[L + i] = FVector2D(U, 0.0f);
		UVs[R + i] = FVector2D(U, 1.0f);
	}
}
