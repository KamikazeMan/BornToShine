// Born To Shine - Foundation Block with Corner Sockets

#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "FoundationBlock.generated.h"

/**
 * Foundation block - the first piece placed in construction
 * Features:
 * - Snaps to 8ft (243.84cm) grid intervals
 * - Has 4 corner sockets at 14cm from ground
 * - Has center snap point at 14cm from ground
 * - Accepts rim board connections
 */
UCLASS()
class BORNTOSHINE_API AFoundationBlock : public ABuildablePiece
{
	GENERATED_BODY()

public:
	AFoundationBlock();

	// Override placement to include grid snapping
	virtual void UpdatePreviewPosition(const FVector& NewLocation, const FRotator& NewRotation) override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

	// Grid snapping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float GridSize;  // 8ft = 243.84cm

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	float SocketHeightOffset;  // 14cm from ground

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FVector BlockDimensions;  // Standard foundation block size

	// Snap location to grid
	FVector SnapToGrid(const FVector& Location) const;

	// Create corner sockets
	void CreateCornerSockets();

	// Create center socket
	void CreateCenterSocket();

	// Create side sockets (for mid-span support)
	void CreateSideSocket s();
};
