#include "WallSheathing.h"
#include "WindowFrame.h"
#include "DoorFrame.h"
#include "ConstructionPhaseManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"

AWallSheathing::AWallSheathing()
{
	PieceType = EPieceType::WallSheathing;

	SheetWidth = 121.92f;      // 4ft
	SheetHeight = 243.84f;     // 8ft
	SheetThickness = 1.27f;    // 0.50"

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	if (MeshComponent)
	{
		MeshComponent->SetupAttachment(SceneRoot);
	}

	bAutoNailOnPlace = false;
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

void AWallSheathing::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComponent && MeshComponent->GetStaticMesh())
	{
		FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
		float MeshWidth = Bounds.BoxExtent.X * 2.0f;
		float MeshHeight = Bounds.BoxExtent.Z * 2.0f;

		if (MeshWidth > 1.0f && MeshHeight > 1.0f)
		{
			const float WallCavityHeight = 247.66f;
			const float BottomExt = 3.82f;   // extends below bottom plate to cover rim board
			const float TopExt = 3.81f;       // extends above double top plate
			const float SideExt = 1.905f;     // extends one side to cover corner post face (half of 2x4 width)

			float TotalHeight = WallCavityHeight + BottomExt + TopExt;
			float TotalWidth = SheetWidth + SideExt;

			FVector CurScale = MeshComponent->GetRelativeScale3D();
			float ScaleX = TotalWidth / MeshWidth;
			float ScaleZ = TotalHeight / MeshHeight;
			MeshComponent->SetRelativeScale3D(FVector(ScaleX, CurScale.Y, ScaleZ));

			// Default: extension centered (split both sides). SetCornerExtensionSide()
			// will shift it to the correct side when the snap detects a corner.
			// ZShift centers the mesh so BottomExt hangs below and TopExt above the wall cavity
			float ZShift = (TopExt - BottomExt) / 2.0f;
			MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, ZShift));

			// Keep SheetHeight at wall cavity for socket positioning
			SheetHeight = WallCavityHeight;

			UE_LOG(LogTemp, Log, TEXT("WallSheathing: Scaled - TotalW=%.1f TotalH=%.1f (SideExt=%.2f BottomExt=%.2f TopExt=%.2f)"),
				TotalWidth, TotalHeight, SideExt, BottomExt, TopExt);
		}
	}

	// Set bottom socket Z to wall cavity bottom (not extended mesh bottom)
	{
		float HalfCavity = SheetHeight / 2.0f;
		for (FConstructionSocket& Socket : Sockets)
		{
			Socket.LocalPosition.Z = -HalfCavity;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("WallSheathing: BeginPlay - %.1f x %.1f x %.1fcm, Sockets=%d"),
		SheetWidth, SheetHeight, SheetThickness, Sockets.Num());
}

void AWallSheathing::InitializeSockets()
{
	Sockets.Empty();
	CreateFaceSockets();
	UE_LOG(LogTemp, Log, TEXT("WallSheathing: InitializeSockets - Generated %d sockets"), Sockets.Num());
}

void AWallSheathing::CreateFaceSockets()
{
	Sockets.Empty();

	// Single bottom-center socket — snaps to bottom plate top face.
	// All positioning (Z, yaw, interior/exterior offset) is handled in DetectSnapCandidates.
	FConstructionSocket Socket;
	Socket.SocketName = FName(TEXT("WallSheathing_Bottom"));
	Socket.SocketType = EConstructionSocketType::WallSheathing_Face;
	Socket.LocalPosition = FVector(0.0f, 0.0f, -SheetHeight / 2.0f);
	Socket.LocalRotation = FRotator(0.0f, 0.0f, 0.0f);
	Socket.Orientation = ESocketOrientation::Vertical;
	Socket.bIsOccupied = false;
	Sockets.Add(Socket);
}

void AWallSheathing::AdjustSocketsToMeshBounds()
{
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return;

	FBoxSphereBounds Bounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	float MeshBottomZ = (Bounds.Origin.Z - Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshTopZ = (Bounds.Origin.Z + Bounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;

	SheetHeight = MeshTopZ - MeshBottomZ;

	// Update socket Z positions to match scaled mesh
	for (FConstructionSocket& Socket : Sockets)
	{
		FString Name = Socket.SocketName.ToString();
		if (Name.Contains(TEXT("_Bot_")))
			Socket.LocalPosition.Z = MeshBottomZ;
		else if (Name.Contains(TEXT("_Mid_")))
			Socket.LocalPosition.Z = (MeshBottomZ + MeshTopZ) / 2.0f;
		else if (Name.Contains(TEXT("_Top_")))
			Socket.LocalPosition.Z = MeshTopZ;
	}

	UE_LOG(LogTemp, Log, TEXT("WallSheathing: AdjustSockets - MeshZ=[%.2f, %.2f] height=%.2fcm"),
		MeshBottomZ, MeshTopZ, SheetHeight);
}

void AWallSheathing::SetCornerExtensionSide(int32 Side)
{
	if (!MeshComponent) return;

	const float SideExt = 1.905f; // half of 2x4 width
	FVector Loc = MeshComponent->GetRelativeLocation();

	if (Side > 0)
	{
		// Extension on +X (right side in local space)
		Loc.X = SideExt / 2.0f;
	}
	else if (Side < 0)
	{
		// Extension on -X (left side in local space)
		Loc.X = -SideExt / 2.0f;
	}
	else
	{
		// No corner — center the extension
		Loc.X = 0.0f;
	}

	MeshComponent->SetRelativeLocation(Loc);
}

void AWallSheathing::ScalePiece(float ScaleDelta)
{
	SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
	CurrentScale = FVector(1.0f, 1.0f, 1.0f);
}

bool AWallSheathing::TryPlace()
{
	// Always go through normal placement first
	if (!Super::TryPlace()) return false;

	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return true;

	// Find any window or door frames that overlap this sheet
	FVector SheetLoc = GetActorLocation();
	FRotator SheetRot = GetActorRotation();
	FVector WallDir = FRotator(0, SheetRot.Yaw, 0).RotateVector(FVector::ForwardVector);
	FVector WallRight = FRotator(0, SheetRot.Yaw, 0).RotateVector(FVector::RightVector);

	// Sheet bounds in local 2D (along wall = X, vertical = Z)
	float SheetHalfW = SheetWidth / 2.0f;
	float SheetHalfH = SheetHeight / 2.0f;
	float SheetLeft = -SheetHalfW;
	float SheetRight = SheetHalfW;
	float SheetBottom = -SheetHalfH;
	float SheetTop = SheetHalfH;

	struct FCutout
	{
		float Left, Right, Bottom, Top; // In sheet-local coords (along wall, vertical)
	};

	TArray<FCutout> Cutouts;

	// Search for window frames
	TArray<AActor*> AllWindows;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindowFrame::StaticClass(), AllWindows);
	for (AActor* A : AllWindows)
	{
		AWindowFrame* WF = Cast<AWindowFrame>(A);
		if (!WF) continue;

		FVector FrameLoc = WF->GetActorLocation();

		// Check if frame is close to sheet (perpendicular distance)
		FVector ToFrame = FrameLoc - SheetLoc;
		float PerpDist = FMath::Abs(FVector::DotProduct(ToFrame, WallRight));
		if (PerpDist > 20.0f) continue; // Not on this wall

		// Project frame center onto sheet's local coordinate system
		float FrameAlongWall = FVector::DotProduct(ToFrame, WallDir);

		// Window origin is at mesh center — rough opening offset by sill height
		float ROHalfW = WF->RoughOpeningWidth / 2.0f;
		float ROBottom = ToFrame.Z + (-WF->FrameHeight / 2.0f + WF->RoughSillHeight);
		float ROTop = ROBottom + WF->RoughOpeningHeight;

		FCutout Cut;
		Cut.Left = FrameAlongWall - ROHalfW;
		Cut.Right = FrameAlongWall + ROHalfW;
		Cut.Bottom = ROBottom;
		Cut.Top = ROTop;

		// Check if cutout overlaps the sheet
		if (Cut.Right > SheetLeft + 1.0f && Cut.Left < SheetRight - 1.0f &&
			Cut.Top > SheetBottom + 1.0f && Cut.Bottom < SheetTop - 1.0f)
		{
			// Clamp cutout to sheet bounds
			Cut.Left = FMath::Max(Cut.Left, SheetLeft);
			Cut.Right = FMath::Min(Cut.Right, SheetRight);
			Cut.Bottom = FMath::Max(Cut.Bottom, SheetBottom);
			Cut.Top = FMath::Min(Cut.Top, SheetTop);
			Cutouts.Add(Cut);

			UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Window cutout at local [%.1f,%.1f]-[%.1f,%.1f]"),
				Cut.Left, Cut.Bottom, Cut.Right, Cut.Top);
		}
	}

	// Search for door frames
	TArray<AActor*> AllDoors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADoorFrame::StaticClass(), AllDoors);
	for (AActor* A : AllDoors)
	{
		ADoorFrame* DF = Cast<ADoorFrame>(A);
		if (!DF) continue;

		FVector FrameLoc = DF->GetActorLocation();

		FVector ToFrame = FrameLoc - SheetLoc;
		float PerpDist = FMath::Abs(FVector::DotProduct(ToFrame, WallRight));
		if (PerpDist > 20.0f) continue;

		float FrameAlongWall = FVector::DotProduct(ToFrame, WallDir);

		// Door origin is at the bottom center — opening goes straight up
		float ROHalfW = DF->RoughOpeningWidth / 2.0f;
		float ROBottom = ToFrame.Z;
		float ROTop = ToFrame.Z + DF->RoughOpeningHeight;

		FCutout Cut;
		Cut.Left = FrameAlongWall - ROHalfW;
		Cut.Right = FrameAlongWall + ROHalfW;
		Cut.Bottom = ROBottom;
		Cut.Top = ROTop;

		if (Cut.Right > SheetLeft + 1.0f && Cut.Left < SheetRight - 1.0f &&
			Cut.Top > SheetBottom + 1.0f && Cut.Bottom < SheetTop - 1.0f)
		{
			Cut.Left = FMath::Max(Cut.Left, SheetLeft);
			Cut.Right = FMath::Min(Cut.Right, SheetRight);
			Cut.Bottom = FMath::Max(Cut.Bottom, SheetBottom);
			Cut.Top = FMath::Min(Cut.Top, SheetTop);
			Cutouts.Add(Cut);

			UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Door cutout at local [%.1f,%.1f]-[%.1f,%.1f]"),
				Cut.Left, Cut.Bottom, Cut.Right, Cut.Top);
		}
	}

	// If no cutouts, keep the original sheet as-is
	if (Cutouts.Num() == 0) return true;

	// For simplicity, handle one cutout per sheet (first found)
	// Multiple cutouts on one 4ft sheet would be extremely rare
	FCutout Cut = Cutouts[0];

	// Generate up to 4 rectangular pieces around the cutout:
	// 1. Left strip: SheetLeft to Cut.Left, full height
	// 2. Right strip: Cut.Right to SheetRight, full height
	// 3. Bottom strip: Cut.Left to Cut.Right, SheetBottom to Cut.Bottom
	// 4. Top strip: Cut.Left to Cut.Right, Cut.Top to SheetTop

	struct FPieceRect
	{
		float Left, Right, Bottom, Top;
		bool IsValid() const { return (Right - Left) > 1.0f && (Top - Bottom) > 1.0f; }
	};

	TArray<FPieceRect> Pieces;

	// Left strip
	FPieceRect LeftStrip = { SheetLeft, Cut.Left, SheetBottom, SheetTop };
	if (LeftStrip.IsValid()) Pieces.Add(LeftStrip);

	// Right strip
	FPieceRect RightStrip = { Cut.Right, SheetRight, SheetBottom, SheetTop };
	if (RightStrip.IsValid()) Pieces.Add(RightStrip);

	// Bottom strip (between left and right cutout edges only)
	FPieceRect BottomStrip = { Cut.Left, Cut.Right, SheetBottom, Cut.Bottom };
	if (BottomStrip.IsValid()) Pieces.Add(BottomStrip);

	// Top strip (between left and right cutout edges only)
	FPieceRect TopStrip = { Cut.Left, Cut.Right, Cut.Top, SheetTop };
	if (TopStrip.IsValid()) Pieces.Add(TopStrip);

	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Splitting into %d pieces around cutout"), Pieces.Num());

	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Sheet bounds local: L=%.1f R=%.1f B=%.1f T=%.1f"),
		SheetLeft, SheetRight, SheetBottom, SheetTop);
	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Cutout local: L=%.1f R=%.1f B=%.1f T=%.1f"),
		Cut.Left, Cut.Right, Cut.Bottom, Cut.Top);
	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Sheet world center: (%.1f, %.1f, %.1f) SheetHeight=%.1f"),
		SheetLoc.X, SheetLoc.Y, SheetLoc.Z, SheetHeight);

	for (int32 i = 0; i < Pieces.Num(); i++)
	{
		const FPieceRect& R = Pieces[i];
		float CX = (R.Left + R.Right) / 2.0f;
		float CZ = (R.Bottom + R.Top) / 2.0f;
		UE_LOG(LogTemp, Warning, TEXT("  Piece[%d]: local [%.1f,%.1f]-[%.1f,%.1f] size %.1fx%.1f center(%.1f,%.1f) worldZ=%.1f"),
			i, R.Left, R.Bottom, R.Right, R.Top,
			R.Right - R.Left, R.Top - R.Bottom, CX, CZ, SheetLoc.Z + CZ);
	}

	// Get mesh and material from original sheet
	UStaticMesh* OrigMesh = MeshComponent->GetStaticMesh();
	UMaterialInterface* OrigMat = MeshComponent->GetMaterial(0);
	FBoxSphereBounds MeshBounds = OrigMesh->GetBounds();

	// Current mesh scale (includes wall cavity height extension)
	FVector OrigScale = MeshComponent->GetRelativeScale3D();

	for (const FPieceRect& Rect : Pieces)
	{
		float PieceW = Rect.Right - Rect.Left;
		float PieceH = Rect.Top - Rect.Bottom;
		float PieceCenterAlongWall = (Rect.Left + Rect.Right) / 2.0f;
		float PieceCenterVertical = (Rect.Bottom + Rect.Top) / 2.0f;

		// World position for this piece
		FVector PieceWorldLoc = SheetLoc
			+ WallDir * PieceCenterAlongWall
			+ FVector(0, 0, PieceCenterVertical);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AStaticMeshActor* Piece = GetWorld()->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), PieceWorldLoc, SheetRot, SpawnParams);

		if (Piece)
		{
			UStaticMeshComponent* SMC = Piece->GetStaticMeshComponent();
			if (SMC)
			{
				SMC->SetMobility(EComponentMobility::Movable);
				SMC->SetStaticMesh(OrigMesh);

				// Scale to match piece dimensions
				float ScaleX = (PieceW / SheetWidth) * OrigScale.X;
				float ScaleY = OrigScale.Y;
				float ScaleZ = (PieceH / SheetHeight) * OrigScale.Z;
				SMC->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));

				// Correct for mesh pivot offset — if the mesh pivot isn't at the
				// bounding box center, the scaled mesh won't be centered on the actor.
				// Shift the mesh so its visual center aligns with the actor position.
				float PivotOffsetX = MeshBounds.Origin.X * ScaleX;
				float PivotOffsetZ = MeshBounds.Origin.Z * ScaleZ;
				SMC->SetRelativeLocation(FVector(-PivotOffsetX, 0.0f, -PivotOffsetZ));

				if (OrigMat)
				{
					SMC->SetMaterial(0, OrigMat);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("WallSheathing: Spawned cutout piece at along=%.1f vert=%.1f size %.1fx%.1f pivotOff=(%.1f,%.1f)"),
				PieceCenterAlongWall, PieceCenterVertical, PieceW, PieceH,
				MeshBounds.Origin.X, MeshBounds.Origin.Z);
		}
	}

	// Hide the original sheet — it stays alive and registered in PlacedPieces
	// (no dangling pointer, building component can still reference it).
	// The sub-pieces provide the visual cutout appearance.
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}

	return true;
}
