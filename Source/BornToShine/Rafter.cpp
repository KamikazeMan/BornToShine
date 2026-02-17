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
	// 2x6 rafter with THREE CUTS, all vertex positions computed directly.
	//
	// Local space (world-oriented):
	//   Origin = ridge plumb cut top (where top edge meets ridge board face)
	//   +X = horizontal toward wall/tail
	//   -Z = downward (gravity)
	//   Y  = rafter thickness (+-HalfW)
	//
	// The rafter centerline slopes from (0,0) down to (TotalHoriz, -TotalRise).
	// Cross-section is perpendicular to the slope.
	//
	// CUT 1 - PLUMB CUT (ridge end): vertical plane at X=0.
	//   The top edge of the rafter meets the ridge board face.
	//   The bottom edge is offset horizontally by D*SinA (rafter depth
	//   projected onto horizontal).
	//
	// CUT 2 - BIRDSMOUTH (at top plate, X=RunDistanceCm):
	//   Seat cut: horizontal, depth = top plate width (3.5" = 8.89cm)
	//   Heel cut: vertical, from inner edge of seat up to bottom of rafter
	//   The seat bears on the top plate surface.
	//
	// CUT 3 - TAIL PLUMB CUT (at overhang end):
	//   Same angle as ridge plumb cut. Fascia attaches to this face.
	// ===================================================================

	float A = GetPitchAngleRadians();
	float CosA = FMath::Cos(A);
	float SinA = FMath::Sin(A);
	float TanA = SinA / CosA;

	float HalfW = RafterWidth / 2.0f;  // 1.905cm
	float D     = RafterDepth;          // 13.97cm

	// Birdsmouth dimensions
	float SeatDepthCm = 8.89f; // 3.5" = top plate width (2x4 actual)
	float HeelHeight = D * BirdsmouthSeatFraction; // 2/3 of rafter depth

	// Key horizontal distances
	float RunH = RunDistanceCm;
	float TotalH = RunH + OverhangCm;

	// ===================================================================
	// SIDE PROFILE POINTS (in XZ plane, top edge and bottom edge)
	//
	// The rafter top edge runs along a line from the ridge plumb cut
	// downward at pitch angle. The bottom edge is offset perpendicular
	// to the slope by the rafter depth D.
	//
	// Top edge line:    Z_top(x) = -x * TanA
	// Bottom edge line: Z_bot(x) = -x * TanA - D * CosA
	//                   (offset perpendicular to slope = D, vertical component = D*CosA)
	// Bottom edge X offset from top at same slope-distance:
	//   X_bot(x) = x + D * SinA  ... NO, the bottom edge directly below
	//   a point on the top edge at horizontal x is at the SAME x, just
	//   lower by D*CosA.  But the perpendicular offset also has a
	//   horizontal component of D*SinA toward the tail.
	//
	// Actually: from any point P on the top edge, going perpendicular
	// into the rafter (SlopePerp direction) by distance D gives the
	// corresponding bottom edge point:
	//   P_bot = P_top + D * SlopePerp
	//   where SlopePerp = (-SinA, 0, -CosA)
	//   so P_bot.X = P_top.X - D*SinA
	//      P_bot.Z = P_top.Z - D*CosA
	// ===================================================================

	// Direction vectors
	FVector SlopeDir(CosA, 0.0f, -SinA);
	FVector SlopePerp(-SinA, 0.0f, -CosA); // Into rafter depth (top -> bottom)
	FVector Y(0.0f, HalfW, 0.0f);

	// Plumb cut direction (vertical plane): normal is horizontal along slope
	// A plumb cut face is VERTICAL. Its normal in the XZ plane is (CosA, 0, -SinA)
	// projected to just the X component... no, a plumb cut is simply a vertical
	// line. On the rafter cross-section, the plumb line intersects the top and
	// bottom edges at different X positions because the rafter is tilted.

	// --- RIDGE PLUMB CUT (CUT 1) ---
	// Top edge at X=0: this is the origin, the ridge board face
	FVector P0_top(0.0f, 0.0f, 0.0f); // Ridge top (where top edge meets ridge face)
	// Bottom edge at the ridge plumb cut: go perpendicular from top edge
	// BUT a plumb cut is vertical, so at X=0, the bottom is straight down.
	// The rafter bottom edge at the same vertical line (X=0) is at:
	//   The bottom edge point directly below P0_top:
	//   Going perp from P0_top: P0_bot = P0_top + D * SlopePerp
	//   = (0 - D*SinA, 0, 0 - D*CosA) = (-D*SinA, 0, -D*CosA)
	//   But a PLUMB cut is at a fixed X position. The plumb cut at X=0
	//   intersects the bottom edge where the bottom edge reaches X=0.
	//
	// Bottom edge as a function of slope parameter t:
	//   BotEdge(t) = SlopeDir*t + D*SlopePerp
	//   BotEdge(t).X = CosA*t - D*SinA
	//   Set BotEdge.X = 0: t = D*SinA/CosA = D*TanA
	//   BotEdge.Z = -SinA * D*TanA - D*CosA = -D*Sin²A/CosA - D*CosA
	//             = -D*(Sin²A + Cos²A)/CosA = -D/CosA
	float PlumbBotZ_ridge = -D / CosA;
	FVector P0_bot(0.0f, 0.0f, PlumbBotZ_ridge);

	// --- BIRDSMOUTH (CUT 2) at X = RunH ---
	// The rafter crosses over the top plate at horizontal distance = RunH.
	// Top edge at X=RunH:
	float TopZ_atWall = -RunH * TanA;
	FVector Bm_top(RunH, 0.0f, TopZ_atWall);

	// Bottom edge at X=RunH (perpendicular from top edge):
	FVector Bm_bot = Bm_top + SlopePerp * D;
	// = (RunH - D*SinA, 0, TopZ_atWall - D*CosA)

	// Birdsmouth seat cut: horizontal surface where rafter bears on plate.
	// The seat cut starts at the bottom of the rafter at the wall line
	// and extends inward (toward ridge) by SeatDepthCm horizontally.
	//
	// Seat outer point (directly above the plate outer edge): The bottom
	// edge of the rafter at horizontal position X=RunH. But we use the
	// PLUMB intersection at X=RunH on the bottom edge:
	float BotEdgeZ_atRunH = -RunH * TanA - D * CosA + D * SinA * TanA;
	// Simplify: the bottom edge at parameter where X=RunH:
	//   CosA*t - D*SinA = RunH => t = (RunH + D*SinA)/CosA
	//   Z = -SinA*t - D*CosA = -SinA*(RunH + D*SinA)/CosA - D*CosA
	//     = -(RunH*SinA + D*Sin²A)/CosA - D*CosA
	//     = -(RunH*SinA)/CosA - D*(Sin²A/CosA + CosA)
	//     = -RunH*TanA - D/CosA
	float PlumbBotZ_atWall = -RunH * TanA - D / CosA;

	// The seat cut is HORIZONTAL at the Z level of the plate top surface.
	// The birdsmouth seat point is where the bottom of the rafter meets
	// the wall line. For the seat to bear on the plate, we use:
	// Seat Z = bottom edge Z at the wall (where heel cut meets the seat)
	//
	// Heel cut: vertical line at X = RunH - SeatDepthCm
	// going from the seat level up to the bottom edge of the rafter.
	float HeelX = RunH;  // Heel cut is at the outer edge of the plate
	float SeatZ = PlumbBotZ_atWall; // Bottom of rafter at the wall line

	// The seat cut goes INWARD (toward ridge) from the heel by SeatDepthCm
	float SeatInnerX = HeelX - SeatDepthCm;

	// At SeatInnerX, the bottom edge Z is:
	float PlumbBotZ_atSeatInner = -SeatInnerX * TanA - D / CosA;

	// Heel cut top: where the vertical line at HeelX meets the bottom edge
	// This is PlumbBotZ_atWall (already computed)
	// Heel cut bottom: the seat level
	// These are the same point for the outer edge. The heel cut is actually
	// at the INNER edge of the seat (closer to ridge):
	//
	// Corrected birdsmouth geometry:
	// - Seat cut: horizontal from (SeatInnerX, SeatZ) to (HeelX, SeatZ)
	// - Heel cut: vertical from (SeatInnerX, SeatZ) up to bottom edge at SeatInnerX
	//
	// Wait, the standard birdsmouth has:
	// - The SEAT cut (horizontal) sits on the top plate. It's at the BOTTOM
	//   of the notch.
	// - The HEEL cut (vertical) goes UP from the inner end of the seat to
	//   meet the bottom edge of the rafter.
	//
	// The seat Z should be the level of the double top plate surface.
	// In our coordinate system, the birdsmouth socket is at (RunH, 0, -Rise)
	// where Rise = RunH * TanA. The top plate surface is at that Z level.
	//
	// So seatZ = -RunH * TanA (the plate top surface Z)
	// The seat goes from X=SeatInnerX to X=RunH at Z=seatZ.
	// The heel is a vertical line at X=SeatInnerX from Z=seatZ up to
	// where it hits the rafter bottom edge.
	float SeatSurfaceZ = -RunH * TanA; // Top of double top plate

	// The bottom edge of the rafter (as a line in XZ) can be parameterized:
	// X_bot(t) = CosA*t - D*SinA
	// Z_bot(t) = -SinA*t - D*CosA
	// At X=SeatInnerX: t = (SeatInnerX + D*SinA)/CosA
	// Z = -SinA*(SeatInnerX + D*SinA)/CosA - D*CosA
	//   = -SeatInnerX*TanA - D*Sin²A/CosA - D*CosA
	//   = -SeatInnerX*TanA - D/CosA
	float HeelTopZ = -SeatInnerX * TanA - D / CosA;

	// Birdsmouth profile points (XZ):
	FVector Bm_heelTop(SeatInnerX, 0.0f, HeelTopZ);     // Bottom edge at heel X
	FVector Bm_heelBot(SeatInnerX, 0.0f, SeatSurfaceZ); // Inner corner of seat
	FVector Bm_seatOut(HeelX, 0.0f, SeatSurfaceZ);      // Outer corner of seat

	// After the birdsmouth, the bottom edge continues from Bm_seatOut
	// along the slope toward the tail. The rafter bottom edge resumes at
	// the wall line (X=RunH). From Bm_seatOut, going along SlopeDir:
	// But actually, after the seat cut, the bottom edge of the rafter
	// below the seat is removed. The rafter profile continues from
	// Bm_seatOut along the original bottom edge line toward the tail.
	//
	// At X=RunH, the bottom edge resumes: we need the bottom edge point
	// past the birdsmouth. The bottom continues from Bm_seatOut along
	// the slope to the tail.

	// --- TAIL PLUMB CUT (CUT 3) ---
	// Same as ridge plumb cut but at X=TotalH.
	float TopZ_atTail = -TotalH * TanA;
	FVector Tail_top(TotalH, 0.0f, TopZ_atTail);

	// Bottom edge plumb intersection at X=TotalH:
	float PlumbBotZ_atTail = -TotalH * TanA - D / CosA;
	FVector Tail_bot(TotalH, 0.0f, PlumbBotZ_atTail);

	// ===================================================================
	// RAFTER SIDE PROFILE (XZ cross-section, 9 points clockwise from ridge top)
	//
	//  P0 (ridge top) ----top edge---- P1 (tail top)
	//   |                                |
	//   | plumb                    plumb |
	//   | cut                       cut  |
	//   |                                |
	//  P8 (ridge bot) --bot edge-- P7    P2 (tail bot)
	//                               \    |
	//                           (slope)  |
	//                                 \  |
	//                              P6--P3 (seat outer = wall line)
	//                              |
	//                              P5 (heel bottom = seat inner)
	//                              |
	//                              P4 (heel top = bot edge at seat inner X)
	//                         --bot edge--
	//
	// Going clockwise from P0:
	//   P0 = ridge top (0, 0)
	//   P1 = tail top
	//   P2 = tail bot (plumb cut)
	//   P3 = bottom edge at X=RunH (just past birdsmouth, below seat)
	//        Actually P3 = seat outer corner = (RunH, SeatSurfaceZ)
	//   P4 = seat inner / heel bottom = (SeatInnerX, SeatSurfaceZ)
	//   P5 = heel top = bottom edge at SeatInnerX
	//   P6 = ridge bottom (plumb cut)
	//
	// Wait, we need to handle the segment from tail back to birdsmouth
	// along the bottom edge. Let me re-think the profile order.
	//
	// Profile going CLOCKWISE around the rafter side view:
	//   P0 = Ridge plumb top = (0, 0)
	//   P1 = Tail plumb top = (TotalH, -TotalH*TanA)
	//   P2 = Tail plumb bot = (TotalH, -TotalH*TanA - D/CosA)
	//   P3 = Bottom edge at X=RunH (past birdsmouth) =
	//        This is the point on the original bottom edge at X=RunH
	//        = (RunH, -RunH*TanA - D/CosA)
	//        BUT the birdsmouth removes material here! After the seat cut,
	//        the bottom goes UP to the seat surface.
	//        So P3 = seat outer = (RunH, SeatSurfaceZ) = (RunH, -RunH*TanA)
	//   P4 = Seat inner / heel bot = (SeatInnerX, -RunH*TanA)
	//   P5 = Heel top = (SeatInnerX, -SeatInnerX*TanA - D/CosA)
	//   P6 = Ridge plumb bot = (0, -D/CosA)
	//
	// That's 7 points for the side profile. The bottom edge from P2 to P3
	// follows the original slope line, then P3-P4 is the horizontal seat,
	// P4-P5 is the vertical heel, and P5-P6 is the bottom edge from heel
	// back to the ridge.
	// ===================================================================

	// Define the 7 profile points
	FVector Prof[7];
	Prof[0] = P0_top;     // Ridge plumb top
	Prof[1] = Tail_top;   // Tail plumb top
	Prof[2] = Tail_bot;   // Tail plumb bot
	Prof[3] = Bm_seatOut; // Birdsmouth seat outer (at wall line)
	Prof[4] = Bm_heelBot; // Birdsmouth heel bottom (seat inner corner)
	Prof[5] = Bm_heelTop; // Birdsmouth heel top (bottom edge at seat inner X)
	Prof[6] = P0_bot;     // Ridge plumb bot

	// ===================================================================
	// BUILD THE 3D MESH
	// The rafter is extruded along Y by +-HalfW.
	// Each segment of the profile generates faces.
	// ===================================================================

	// Helper: add a quad
	auto AddFace = [&](FVector VA, FVector VB, FVector VC, FVector VD, FVector Normal)
	{
		int32 Base = Vertices.Num();
		Vertices.Add(VA); Normals.Add(Normal); UVs.Add(FVector2D(0, 0));
		Vertices.Add(VB); Normals.Add(Normal); UVs.Add(FVector2D(1, 0));
		Vertices.Add(VC); Normals.Add(Normal); UVs.Add(FVector2D(1, 1));
		Vertices.Add(VD); Normals.Add(Normal); UVs.Add(FVector2D(0, 1));
		Triangles.Add(Base); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
		Triangles.Add(Base); Triangles.Add(Base + 2); Triangles.Add(Base + 3);
	};

	// Helper: add a triangle
	auto AddTri = [&](FVector VA, FVector VB, FVector VC, FVector Normal)
	{
		int32 Base = Vertices.Num();
		Vertices.Add(VA); Normals.Add(Normal); UVs.Add(FVector2D(0, 0));
		Vertices.Add(VB); Normals.Add(Normal); UVs.Add(FVector2D(1, 0));
		Vertices.Add(VC); Normals.Add(Normal); UVs.Add(FVector2D(0.5f, 1));
		Triangles.Add(Base); Triangles.Add(Base + 1); Triangles.Add(Base + 2);
	};

	FVector NormTop   = FVector(SinA, 0.0f, CosA);    // Roof surface (perp to slope, outward)
	FVector NormBot   = FVector(-SinA, 0.0f, -CosA);  // Ceiling side
	FVector NormLeft  = FVector(0.0f, -1.0f, 0.0f);
	FVector NormRight = FVector(0.0f, 1.0f, 0.0f);
	FVector NormPlumbRidge = FVector(-1.0f, 0.0f, 0.0f); // Ridge plumb cut faces -X
	FVector NormPlumbTail  = FVector(1.0f, 0.0f, 0.0f);  // Tail plumb cut faces +X
	FVector NormSeat  = FVector(0.0f, 0.0f, -1.0f);   // Seat cut faces down (bears on plate)
	FVector NormHeel  = FVector(-1.0f, 0.0f, 0.0f);   // Heel cut faces toward ridge

	// --- TOP FACE (roof surface): P0 to P1, along slope ---
	AddFace(Prof[0] - Y, Prof[0] + Y, Prof[1] + Y, Prof[1] - Y, NormTop);

	// --- TAIL PLUMB CUT FACE: P1 to P2 (vertical) ---
	AddFace(Prof[1] + Y, Prof[1] - Y, Prof[2] - Y, Prof[2] + Y, NormPlumbTail);

	// --- BOTTOM FACE from tail to birdsmouth seat outer: P2 to P3 (along slope) ---
	AddFace(Prof[2] - Y, Prof[2] + Y, Prof[3] + Y, Prof[3] - Y, NormBot);

	// --- BIRDSMOUTH SEAT CUT FACE: P3 to P4 (horizontal, faces downward onto plate) ---
	AddFace(Prof[3] + Y, Prof[3] - Y, Prof[4] - Y, Prof[4] + Y, NormSeat);

	// --- BIRDSMOUTH HEEL CUT FACE: P4 to P5 (vertical, faces toward ridge) ---
	AddFace(Prof[4] - Y, Prof[4] + Y, Prof[5] + Y, Prof[5] - Y, NormHeel);

	// --- BOTTOM FACE from heel to ridge: P5 to P6 (along slope) ---
	AddFace(Prof[5] + Y, Prof[5] - Y, Prof[6] - Y, Prof[6] + Y, NormBot);

	// --- RIDGE PLUMB CUT FACE: P6 to P0 (vertical) ---
	AddFace(Prof[6] - Y, Prof[6] + Y, Prof[0] + Y, Prof[0] - Y, NormPlumbRidge);

	// --- LEFT SIDE FACE (-Y): 7-point polygon with shared vertices ---
	// Using indexed triangulation eliminates gaps between triangles
	{
		int32 Base = Vertices.Num();
		for (int32 i = 0; i < 7; i++)
		{
			Vertices.Add(Prof[i] - Y);
			Normals.Add(NormLeft);
			UVs.Add(FVector2D(Prof[i].X / 200.0f, Prof[i].Z / 200.0f));
		}
		// Fan triangulation from vertex 0
		for (int32 i = 1; i < 6; i++)
		{
			Triangles.Add(Base + 0);
			Triangles.Add(Base + i);
			Triangles.Add(Base + i + 1);
		}
	}

	// --- RIGHT SIDE FACE (+Y): 7-point polygon with shared vertices (reversed winding) ---
	{
		int32 Base = Vertices.Num();
		for (int32 i = 0; i < 7; i++)
		{
			Vertices.Add(Prof[i] + Y);
			Normals.Add(NormRight);
			UVs.Add(FVector2D(Prof[i].X / 200.0f, Prof[i].Z / 200.0f));
		}
		// Fan triangulation from vertex 0 (reversed winding)
		for (int32 i = 1; i < 6; i++)
		{
			Triangles.Add(Base + 0);
			Triangles.Add(Base + i + 1);
			Triangles.Add(Base + i);
		}
	}

	// No component rotation needed — pitch is in the vertices
	if (ProceduralMesh)
	{
		ProceduralMesh->SetRelativeRotation(FRotator::ZeroRotator);
	}

	// Log all 7 profile points for verification
	for (int32 i = 0; i < 7; i++)
	{
		UE_LOG(LogTemp, Warning, TEXT("  Prof[%d] = (%.4f, %.4f, %.4f)"), i, Prof[i].X, Prof[i].Y, Prof[i].Z);
	}

	UE_LOG(LogTemp, Log,
		TEXT("Rafter mesh: pitch=%.1f/12 (%.1fdeg) run=%.1fcm overhang=%.1fcm seatDepth=%.1fcm"
		     "\n  P0(ridge top)=(%.1f,%.1f) P1(tail top)=(%.1f,%.1f)"
		     "\n  P2(tail bot)=(%.1f,%.1f)  P3(seat out)=(%.1f,%.1f)"
		     "\n  P4(heel bot)=(%.1f,%.1f)  P5(heel top)=(%.1f,%.1f)"
		     "\n  P6(ridge bot)=(%.1f,%.1f)"
		     "\n  Verts=%d Tris=%d"),
		PitchRatio, GetPitchAngleDegrees(), RunDistanceCm, OverhangCm, SeatDepthCm,
		Prof[0].X, Prof[0].Z, Prof[1].X, Prof[1].Z,
		Prof[2].X, Prof[2].Z, Prof[3].X, Prof[3].Z,
		Prof[4].X, Prof[4].Z, Prof[5].X, Prof[5].Z,
		Prof[6].X, Prof[6].Z,
		Vertices.Num(), Triangles.Num() / 3);
}
