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
	// 2x6 box with pitch BAKED into vertex positions.
	// The mesh slopes from origin down to (TotalHoriz, 0, -TotalRise).
	// This matches the socket coordinate system exactly:
	//   RidgeSocket at (0, 0, 0)
	//   BirdsmouthSocket at (RunDistanceCm, 0, -Rise)
	//   TailSocket at (TotalHoriz, 0, -TailRise)
	//
	// Local space:
	//   Origin = ridge end (center of cross-section at ridge)
	//   +X = horizontal toward wall/tail
	//   -Z = downward (gravity)
	//   Y  = rafter thickness (1.5" = 3.81cm)
	//
	// Cross-section is perpendicular to the slope direction.
	// ===================================================================

	float HalfW = RafterWidth / 2.0f;  // 1.905cm (thickness)
	float HalfD = RafterDepth / 2.0f;  // 6.985cm (depth)

	float PitchAngle = GetPitchAngleRadians();
	float CosA = FMath::Cos(PitchAngle);
	float SinA = FMath::Sin(PitchAngle);

	// Direction vectors in the XZ plane
	FVector SlopeDir(CosA, 0.0f, -SinA);    // Along the slope (ridge toward tail)
	FVector SlopePerp(SinA, 0.0f, CosA);    // Perpendicular to slope (toward roof surface)
	FVector YDir(0.0f, 1.0f, 0.0f);         // Thickness direction

	// Slope length from ridge to tail (including overhang)
	float TotalHoriz = RunDistanceCm + OverhangCm;
	float SlopeLen = TotalHoriz / CosA;

	// Helper: add a quad with correct outward normal
	auto AddFace = [&](FVector A, FVector B, FVector C, FVector Dpt, FVector Normal)
	{
		int32 Base = Vertices.Num();
		Vertices.Add(A); Normals.Add(Normal); UVs.Add(FVector2D(0, 0));
		Vertices.Add(B); Normals.Add(Normal); UVs.Add(FVector2D(1, 0));
		Vertices.Add(C); Normals.Add(Normal); UVs.Add(FVector2D(1, 1));
		Vertices.Add(Dpt); Normals.Add(Normal); UVs.Add(FVector2D(0, 1));
		Triangles.Add(Base + 0); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
		Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
	};

	// 8 box corners built from slope-aligned cross-section
	FVector RidgeCenter = FVector::ZeroVector;
	FVector TailCenter = SlopeDir * SlopeLen;

	// Ridge end corners
	FVector v0 = RidgeCenter - YDir * HalfW - SlopePerp * HalfD; // bottom-left
	FVector v3 = RidgeCenter + YDir * HalfW - SlopePerp * HalfD; // bottom-right
	FVector v4 = RidgeCenter - YDir * HalfW + SlopePerp * HalfD; // top-left
	FVector v7 = RidgeCenter + YDir * HalfW + SlopePerp * HalfD; // top-right

	// Tail end corners
	FVector v1 = TailCenter - YDir * HalfW - SlopePerp * HalfD;
	FVector v2 = TailCenter + YDir * HalfW - SlopePerp * HalfD;
	FVector v5 = TailCenter - YDir * HalfW + SlopePerp * HalfD;
	FVector v6 = TailCenter + YDir * HalfW + SlopePerp * HalfD;

	// 6 faces with slope-aligned normals
	AddFace(v4, v5, v6, v7, SlopePerp);      // Top (roof surface)
	AddFace(v1, v0, v3, v2, -SlopePerp);     // Bottom
	AddFace(v0, v1, v5, v4, -YDir);          // Left (-Y)
	AddFace(v2, v3, v7, v6, YDir);           // Right (+Y)
	AddFace(v3, v0, v4, v7, -SlopeDir);      // Ridge end
	AddFace(v1, v2, v6, v5, SlopeDir);       // Tail end

	// No component rotation needed — pitch is in the vertices
	if (ProceduralMesh)
	{
		ProceduralMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	UE_LOG(LogTemp, Log,
		TEXT("Rafter mesh: pitch=%.1f/12 (%.1fdeg) slopeLen=%.1fcm run=%.1fcm overhang=%.1fcm Verts=%d Tris=%d"),
		PitchRatio, GetPitchAngleDegrees(), SlopeLen, RunDistanceCm, OverhangCm,
		Vertices.Num(), Triangles.Num() / 3);
}
