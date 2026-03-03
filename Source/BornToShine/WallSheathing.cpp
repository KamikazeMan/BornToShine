#include "WallSheathing.h"
#include "WindowFrame.h"
#include "DoorFrame.h"
#include "ConstructionPhaseManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"

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
	if (!Super::TryPlace()) return false;
	if (!MeshComponent || !MeshComponent->GetStaticMesh()) return true;

	// Get the actual rendered mesh bounds in actor-local space
	FBoxSphereBounds MeshBounds = MeshComponent->GetStaticMesh()->GetBounds();
	FVector MeshScale = MeshComponent->GetRelativeScale3D();
	FVector MeshRelLoc = MeshComponent->GetRelativeLocation();

	// DEBUG TEST: spawn a single piece covering the entire sheet
	// This should look identical to the original mesh. If it doesn't,
	// the coordinate system is wrong.
	if (false) // Change to true to test, false to disable
	{
		MeshComponent->SetVisibility(false);

		UStaticMeshComponent* TestSMC = NewObject<UStaticMeshComponent>(this, TEXT("TestFullSheet"));
		TestSMC->SetMobility(EComponentMobility::Movable);
		TestSMC->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		TestSMC->SetStaticMesh(MeshComponent->GetStaticMesh());
		TestSMC->SetRelativeScale3D(MeshScale);
		TestSMC->SetRelativeLocation(MeshRelLoc);
		if (MeshComponent->GetMaterial(0))
			TestSMC->SetMaterial(0, MeshComponent->GetMaterial(0));
		TestSMC->RegisterComponent();

		UE_LOG(LogTemp, Warning, TEXT("WallSheathing: DEBUG full sheet test - scale=(%.3f,%.3f,%.3f) loc=(%.2f,%.2f,%.2f)"),
			MeshScale.X, MeshScale.Y, MeshScale.Z, MeshRelLoc.X, MeshRelLoc.Y, MeshRelLoc.Z);

		return true; // Skip cutout logic
	}

	// Rendered mesh extents in actor-local space
	float MeshMinX = (MeshBounds.Origin.X - MeshBounds.BoxExtent.X) * MeshScale.X + MeshRelLoc.X;
	float MeshMaxX = (MeshBounds.Origin.X + MeshBounds.BoxExtent.X) * MeshScale.X + MeshRelLoc.X;
	float MeshMinZ = (MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshMaxZ = (MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z) * MeshScale.Z + MeshRelLoc.Z;
	float MeshFullW = MeshMaxX - MeshMinX;
	float MeshFullH = MeshMaxZ - MeshMinZ;

	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Mesh local bounds X=[%.1f,%.1f] Z=[%.1f,%.1f] size=%.1fx%.1f"),
		MeshMinX, MeshMaxX, MeshMinZ, MeshMaxZ, MeshFullW, MeshFullH);

	// Actor transform
	FVector ActorLoc = GetActorLocation();
	FRotator ActorRot = GetActorRotation();
	FVector WallDir = FRotator(0, ActorRot.Yaw, 0).RotateVector(FVector::ForwardVector);
	FVector WallRight = FRotator(0, ActorRot.Yaw, 0).RotateVector(FVector::RightVector);

	// Convert mesh bounds to world space
	// Actor-local X maps to WallDir, Actor-local Z maps to world Z
	// MeshMinX/MeshMaxX = along-wall extents in actor local
	// MeshMinZ/MeshMaxZ = vertical extents in actor local

	struct FCutout { float Left, Right, Bottom, Top; }; // actor-local coords
	TArray<FCutout> Cutouts;

	// --- Window frames ---
	TArray<AActor*> AllWindows;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWindowFrame::StaticClass(), AllWindows);
	for (AActor* A : AllWindows)
	{
		AWindowFrame* WF = Cast<AWindowFrame>(A);
		if (!WF) continue;

		UE_LOG(LogTemp, Warning, TEXT("WallSheathing: WindowFrame dims — FrameOverallW=%.1f RoughOpeningW=%.1f RoughOpeningH=%.1f FrameH=%.1f SillH=%.1f"),
			WF->FrameOverallWidth, WF->RoughOpeningWidth, WF->RoughOpeningHeight, WF->FrameHeight, WF->RoughSillHeight);

		FVector ToFrame = WF->GetActorLocation() - ActorLoc;
		float PerpDist = FMath::Abs(FVector::DotProduct(ToFrame, WallRight));
		if (PerpDist > 20.0f) continue;

		float FrameAlongWall = FVector::DotProduct(ToFrame, WallDir);
		float FrameZ = ToFrame.Z; // vertical offset from actor center

		// Calculate cutout from bottom plate reference, bypassing FrameHeight
		// (which gets overridden by mesh bounds in BeginPlay).
		//
		// Both the sheathing and window frame snap to the same bottom plate.
		// The sheathing actor center is at bottomPlateTop + wallCavityHeight/2.
		// MeshMinZ in actor-local space = the sheet bottom edge.
		//
		// The window frame's RoughSillHeight is measured from the bottom plate top.
		// The window's RoughOpeningHeight is the opening height from sill to header.
		//
		// Convert sill position to sheet-local Z:
		// SillZ_local = MeshMinZ + RoughSillHeight (sill above sheet bottom)
		// But MeshMinZ includes the bottom extension (3.82cm below plate top),
		// so add that back: sill starts at MeshMinZ + BottomExt + RoughSillHeight

		const float BottomExt = 3.82f;
		float SillLocalZ = MeshMinZ + BottomExt + WF->RoughSillHeight;
		float HeaderLocalZ = SillLocalZ + WF->RoughOpeningHeight;
		float HalfOpeningW = WF->RoughOpeningWidth / 2.0f;

		FCutout Cut;
		Cut.Left = FrameAlongWall - HalfOpeningW;
		Cut.Right = FrameAlongWall + HalfOpeningW;
		Cut.Bottom = SillLocalZ + 52.0f;   // Empirical vertical correction
		Cut.Top = HeaderLocalZ + 52.0f;     // Empirical vertical correction

		UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Window cutout — SillLocalZ=%.1f HeaderLocalZ=%.1f (MeshMinZ=%.1f BottomExt=%.1f SillH=%.1f ROH=%.1f)"),
			SillLocalZ, HeaderLocalZ, MeshMinZ, BottomExt, WF->RoughSillHeight, WF->RoughOpeningHeight);

		// Check overlap with mesh bounds
		if (Cut.Right > MeshMinX && Cut.Left < MeshMaxX &&
			Cut.Top > MeshMinZ && Cut.Bottom < MeshMaxZ)
		{
			Cut.Left = FMath::Max(Cut.Left, MeshMinX);
			Cut.Right = FMath::Min(Cut.Right, MeshMaxX);
			Cut.Bottom = FMath::Max(Cut.Bottom, MeshMinZ);
			Cut.Top = FMath::Min(Cut.Top, MeshMaxZ);
			Cutouts.Add(Cut);

			UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Window cutout local [%.1f,%.1f]-[%.1f,%.1f]"),
				Cut.Left, Cut.Bottom, Cut.Right, Cut.Top);
		}
	}

	// --- Door frames ---
	TArray<AActor*> AllDoors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADoorFrame::StaticClass(), AllDoors);
	for (AActor* A : AllDoors)
	{
		ADoorFrame* DF = Cast<ADoorFrame>(A);
		if (!DF) continue;

		UE_LOG(LogTemp, Warning, TEXT("WallSheathing: DoorFrame dims — FrameOverallW=%.1f RoughOpeningW=%.1f RoughOpeningH=%.1f FrameH=%.1f"),
			DF->FrameOverallWidth, DF->RoughOpeningWidth, DF->RoughOpeningHeight, DF->FrameHeight);

		FVector ToFrame = DF->GetActorLocation() - ActorLoc;
		float PerpDist = FMath::Abs(FVector::DotProduct(ToFrame, WallRight));
		if (PerpDist > 20.0f) continue;

		float FrameAlongWall = FVector::DotProduct(ToFrame, WallDir);
		float FrameZ = ToFrame.Z; // door origin is at bottom
		float HalfOverallW = DF->RoughOpeningWidth / 2.0f;

		FCutout Cut;
		Cut.Left = FrameAlongWall - HalfOverallW;
		Cut.Right = FrameAlongWall + HalfOverallW;
		Cut.Bottom = MeshMinZ; // door goes to floor — extend to mesh bottom
		// Door rough opening measured from bottom plate top
		const float DoorBottomExt = 3.82f;
		Cut.Top = MeshMinZ + DoorBottomExt + DF->RoughOpeningHeight;

		if (Cut.Right > MeshMinX && Cut.Left < MeshMaxX &&
			Cut.Top > MeshMinZ && Cut.Bottom < MeshMaxZ)
		{
			Cut.Left = FMath::Max(Cut.Left, MeshMinX);
			Cut.Right = FMath::Min(Cut.Right, MeshMaxX);
			Cut.Bottom = FMath::Max(Cut.Bottom, MeshMinZ);
			Cut.Top = FMath::Min(Cut.Top, MeshMaxZ);
			Cutouts.Add(Cut);

			UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Door cutout local [%.1f,%.1f]-[%.1f,%.1f]"),
				Cut.Left, Cut.Bottom, Cut.Right, Cut.Top);
		}
	}

	if (Cutouts.Num() == 0) return true;

	FCutout Cut = Cutouts[0];

	// Generate pieces around the cutout (in actor-local coordinates matching mesh bounds)
	struct FPieceRect
	{
		float Left, Right, Bottom, Top;
		bool IsValid() const { return (Right - Left) > 1.0f && (Top - Bottom) > 1.0f; }
	};

	TArray<FPieceRect> Pieces;

	FPieceRect LeftStrip = { MeshMinX, Cut.Left, MeshMinZ, MeshMaxZ };
	if (LeftStrip.IsValid()) Pieces.Add(LeftStrip);

	FPieceRect RightStrip = { Cut.Right, MeshMaxX, MeshMinZ, MeshMaxZ };
	if (RightStrip.IsValid()) Pieces.Add(RightStrip);

	FPieceRect BottomStrip = { Cut.Left, Cut.Right, MeshMinZ, Cut.Bottom };
	if (BottomStrip.IsValid()) Pieces.Add(BottomStrip);

	FPieceRect TopStrip = { Cut.Left, Cut.Right, Cut.Top, MeshMaxZ };
	if (TopStrip.IsValid()) Pieces.Add(TopStrip);

	const float OverlapMargin = 5.0f;
	for (FPieceRect& Rect : Pieces)
	{
		float OrigLeft = Rect.Left;
		float OrigRight = Rect.Right;
		float OrigBottom = Rect.Bottom;
		float OrigTop = Rect.Top;

		// Expand all edges
		Rect.Left -= OverlapMargin;
		Rect.Right += OverlapMargin;
		Rect.Bottom -= OverlapMargin;
		Rect.Top += OverlapMargin;

		// Clamp to mesh bounds
		Rect.Left = FMath::Max(Rect.Left, MeshMinX);
		Rect.Right = FMath::Min(Rect.Right, MeshMaxX);
		Rect.Bottom = FMath::Max(Rect.Bottom, MeshMinZ);
		Rect.Top = FMath::Min(Rect.Top, MeshMaxZ);

		// Any edge that originally touched a cutout boundary must not expand
		// into the opening. Check against ALL four cutout edges.
		bool bLeftTouchesCutout = (FMath::Abs(OrigLeft - Cut.Left) < 2.0f) || (FMath::Abs(OrigLeft - Cut.Right) < 2.0f);
		bool bRightTouchesCutout = (FMath::Abs(OrigRight - Cut.Left) < 2.0f) || (FMath::Abs(OrigRight - Cut.Right) < 2.0f);
		bool bBottomTouchesCutout = (FMath::Abs(OrigBottom - Cut.Bottom) < 2.0f) || (FMath::Abs(OrigBottom - Cut.Top) < 2.0f);
		bool bTopTouchesCutout = (FMath::Abs(OrigTop - Cut.Bottom) < 2.0f) || (FMath::Abs(OrigTop - Cut.Top) < 2.0f);

		if (bLeftTouchesCutout) Rect.Left = OrigLeft;
		if (bRightTouchesCutout) Rect.Right = OrigRight;
		if (bBottomTouchesCutout) Rect.Bottom = OrigBottom;
		if (bTopTouchesCutout) Rect.Top = OrigTop;
	}

	UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Splitting into %d pieces"), Pieces.Num());

	UE_LOG(LogTemp, Warning, TEXT("WallSheathing DEBUG: ActorLoc=(%.1f,%.1f,%.1f) ActorRot=(%.1f,%.1f,%.1f)"),
		ActorLoc.X, ActorLoc.Y, ActorLoc.Z, ActorRot.Pitch, ActorRot.Roll, ActorRot.Yaw);
	UE_LOG(LogTemp, Warning, TEXT("WallSheathing DEBUG: MeshRelLoc=(%.2f,%.2f,%.2f) MeshScale=(%.3f,%.3f,%.3f)"),
		MeshRelLoc.X, MeshRelLoc.Y, MeshRelLoc.Z, MeshScale.X, MeshScale.Y, MeshScale.Z);
	UE_LOG(LogTemp, Warning, TEXT("WallSheathing DEBUG: SceneRoot loc=(%.2f,%.2f,%.2f)"),
		SceneRoot->GetRelativeLocation().X, SceneRoot->GetRelativeLocation().Y, SceneRoot->GetRelativeLocation().Z);
	UE_LOG(LogTemp, Warning, TEXT("WallSheathing DEBUG: MeshBounds Origin=(%.2f,%.2f,%.2f) Extent=(%.2f,%.2f,%.2f)"),
		MeshBounds.Origin.X, MeshBounds.Origin.Y, MeshBounds.Origin.Z,
		MeshBounds.BoxExtent.X, MeshBounds.BoxExtent.Y, MeshBounds.BoxExtent.Z);

	// Get original mesh info
	UStaticMesh* OrigMesh = MeshComponent->GetStaticMesh();
	UMaterialInterface* OrigMat = MeshComponent->GetMaterial(0);
	FVector OrigScale = MeshComponent->GetRelativeScale3D();

	// Unscaled mesh dimensions
	float UnscaledW = MeshBounds.BoxExtent.X * 2.0f;
	float UnscaledH = MeshBounds.BoxExtent.Z * 2.0f;

	// Hide original mesh
	MeshComponent->SetVisibility(false);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	for (int32 i = 0; i < Pieces.Num(); i++)
	{
		const FPieceRect& Rect = Pieces[i];
		float PieceW = Rect.Right - Rect.Left;
		float PieceH = Rect.Top - Rect.Bottom;

		float CenterX = (Rect.Left + Rect.Right) / 2.0f;
		float CenterZ = (Rect.Bottom + Rect.Top) / 2.0f;

		FName CompName = FName(*FString::Printf(TEXT("CutoutPiece_%d"), i));
		UStaticMeshComponent* PieceSMC = NewObject<UStaticMeshComponent>(this, CompName);
		if (!PieceSMC) continue;

		PieceSMC->SetMobility(EComponentMobility::Movable);
		PieceSMC->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepRelativeTransform);
		PieceSMC->SetStaticMesh(OrigMesh);

		float ScaleX = PieceW / UnscaledW;
		float ScaleY = OrigScale.Y;
		float ScaleZ = PieceH / UnscaledH;
		PieceSMC->SetRelativeScale3D(FVector(ScaleX, ScaleY, ScaleZ));

		// Position relative to SceneRoot, same space as MeshComponent
		PieceSMC->SetRelativeLocation(FVector(CenterX, MeshRelLoc.Y, CenterZ));

		if (OrigMat)
		{
			PieceSMC->SetMaterial(0, OrigMat);
		}

		PieceSMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PieceSMC->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

		PieceSMC->RegisterComponent();

		UE_LOG(LogTemp, Warning, TEXT("WallSheathing: Piece[%d] center=(%.1f,%.1f) size=%.1fx%.1f scale=(%.3f,%.3f,%.3f)"),
			i, CenterX, CenterZ, PieceW, PieceH, ScaleX, ScaleY, ScaleZ);
	}

	return true;
}
