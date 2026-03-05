#pragma once

#include "CoreMinimal.h"
#include "BuildablePiece.h"
#include "ConstructionTypes.h"
#include "RoofSheathing.generated.h"

UCLASS()
class BORNTOSHINE_API ARoofSheathing : public ABuildablePiece
{
	GENERATED_BODY()

public:
	ARoofSheathing();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetLength;     // 8ft = 243.84cm (along ridge direction)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetWidth;      // 4ft = 121.92cm (up the slope)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Construction|Dimensions")
	float SheetThickness;  // 0.50" = 1.27cm

	// Set by snap system, used by TryPlace() for trimming
	float RoofRidgeStart = 0.0f;
	float RoofRidgeEnd = 0.0f;
	float RoofSlopeMax = 0.0f;
	FVector RoofRidgeDir = FVector::ZeroVector;
	FVector RoofSlopeDir = FVector::ZeroVector;
	FVector RoofRafterOrigin = FVector::ZeroVector;

	virtual void ScalePiece(float ScaleDelta) override;
	virtual bool TryPlace() override;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeSockets() override;

private:
	UPROPERTY(VisibleAnywhere)
	class USceneComponent* SceneRoot;

	void CreateFaceSockets();
};
