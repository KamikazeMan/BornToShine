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
	// SIMPLE RECTANGULAR 2x6 PRISM tilted at pitch angle.
	// No birdsmouth or plumb cuts — get the basic shape rendering first.
	// Cuts will be added incrementally once this renders correctly.
	//
	// Local space:
	//   Origin = ridge end (top of slope)
	//   Slope runs from origin downward toward +X
	//   Y = rafter thickness (+-HalfWidth)
	//   Z = vertical
	// ===================================================================

	float PitchAngle = GetPitchAngleRadians();
	float CosA = FMath::Cos(PitchAngle);
	float SinA = FMath::Sin(PitchAngle);

	float HalfW = RafterWidth / 2.0f;  // 1.905cm
	float D     = RafterDepth;          // 13.97cm

	// Along-slope direction vectors
	FVector SlopeDir(CosA, 0.0f, -SinA);     // Downhill along the slope
	FVector SlopePerp(-SinA, 0.0f, -CosA);   // Perpendicular into rafter depth

	// Total slope length from ridge to tail (including overhang)
	float TotalHoriz = RunDistanceCm + OverhangCm;
	float SlopeLen = TotalHoriz / CosA;

	// Four profile corners (side view, XZ plane):
	//   RidgeTop ---slope---> TailTop
	//      |                     |
	//   RidgeBot --slope---> TailBot
	FVector RidgeTop = FVector::ZeroVector;
	FVector RidgeBot = RidgeTop + SlopePerp * D;
	FVector TailTop  = RidgeTop + SlopeDir * SlopeLen;
	FVector TailBot  = RidgeBot + SlopeDir * SlopeLen;

	// ===================================================================
	// Build the box: 6 faces, each with 4 unique vertices (for correct
	// per-face flat normals).  Total = 24 vertices, 12 triangles.
	// ===================================================================

	// Helper: add a quad with correct outward normal.
	// Vertices A-B-C-D in CCW order when viewed from outside (UE5 front face).
	auto AddFace = [&](FVector A, FVector B, FVector C, FVector Dpt, FVector Normal)
	{
		int32 Base = Vertices.Num();
		Vertices.Add(A); Normals.Add(Normal); UVs.Add(FVector2D(0, 0));
		Vertices.Add(B); Normals.Add(Normal); UVs.Add(FVector2D(1, 0));
		Vertices.Add(C); Normals.Add(Normal); UVs.Add(FVector2D(1, 1));
		Vertices.Add(Dpt); Normals.Add(Normal); UVs.Add(FVector2D(0, 1));
		// Two CCW triangles: A-B-C and A-C-D
		Triangles.Add(Base + 0); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
		Triangles.Add(Base + 0); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
	};

	FVector Y = FVector(0.0f, HalfW, 0.0f);

	// 8 box corners (L = -Y side, R = +Y side)
	FVector RTL = RidgeTop - Y;  // Ridge Top Left
	FVector RTR = RidgeTop + Y;  // Ridge Top Right
	FVector RBL = RidgeBot - Y;  // Ridge Bottom Left
	FVector RBR = RidgeBot + Y;  // Ridge Bottom Right
	FVector TTL = TailTop - Y;   // Tail Top Left
	FVector TTR = TailTop + Y;   // Tail Top Right
	FVector TBL = TailBot - Y;   // Tail Bottom Left
	FVector TBR = TailBot + Y;   // Tail Bottom Right

	// Face normals
	FVector NormTop    = -SlopePerp;                  // Top face (roof surface side)
	FVector NormBot    = SlopePerp;                   // Bottom face (ceiling side)
	FVector NormLeft   = FVector(0.0f, -1.0f, 0.0f); // Left face (-Y)
	FVector NormRight  = FVector(0.0f, 1.0f, 0.0f);  // Right face (+Y)
	FVector NormRidge  = -SlopeDir;                   // Ridge end face
	FVector NormTail   = SlopeDir;                    // Tail end face

	// Top face: RTL -> RTR -> TTR -> TTL  (viewed from above the roof)
	AddFace(RTL, RTR, TTR, TTL, NormTop);

	// Bottom face: TBL -> TBR -> RBR -> RBL  (viewed from below)
	AddFace(TBL, TBR, RBR, RBL, NormBot);

	// Left face (-Y): RTL -> TTL -> TBL -> RBL
	AddFace(RTL, TTL, TBL, RBL, NormLeft);

	// Right face (+Y): TTR -> RTR -> RBR -> TBR
	AddFace(TTR, RTR, RBR, TBR, NormRight);

	// Ridge end face: RTR -> RTL -> RBL -> RBR
	AddFace(RTR, RTL, RBL, RBR, NormRidge);

	// Tail end face: TTL -> TTR -> TBR -> TBL
	AddFace(TTL, TTR, TBR, TBL, NormTail);

	UE_LOG(LogTemp, Warning,
		TEXT("Rafter MESH DEBUG:"
		     "\n  pitch=%.1f/12 (%.1fdeg) slopeLen=%.1fcm run=%.1fcm overhang=%.1fcm"
		     "\n  SlopeDir=(%.3f, %.3f, %.3f)  SlopePerp=(%.3f, %.3f, %.3f)"
		     "\n  RidgeTop=(%.1f, %.1f, %.1f)  RidgeBot=(%.1f, %.1f, %.1f)"
		     "\n  TailTop=(%.1f, %.1f, %.1f)   TailBot=(%.1f, %.1f, %.1f)"
		     "\n  Verts=%d  Tris=%d"),
		PitchRatio, GetPitchAngleDegrees(), SlopeLen, RunDistanceCm, OverhangCm,
		SlopeDir.X, SlopeDir.Y, SlopeDir.Z,
		SlopePerp.X, SlopePerp.Y, SlopePerp.Z,
		RidgeTop.X, RidgeTop.Y, RidgeTop.Z,
		RidgeBot.X, RidgeBot.Y, RidgeBot.Z,
		TailTop.X, TailTop.Y, TailTop.Z,
		TailBot.X, TailBot.Y, TailBot.Z,
		Vertices.Num(), Triangles.Num() / 3);
}
