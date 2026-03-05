// Born To Shine - Floor Joist (2x6 lumber spanning between rim boards)

#pragma once

#include "CoreMinimal.h"
#include "RimBoard.h"
#include "FloorJoist.generated.h"

/**
 * Floor Joist - 2x6 lumber that spans between two parallel rim boards.
 *
 * Same physical dimensions as a rim board (1.5" x 5.5" x variable length).
 * Sits on TOP of rim boards, perpendicular to the through boards (1 and 3).
 * Spaced at 16" on center (standard framing).
 *
 * Socket Layout:
 * - Joist End Sockets (2): At each end, snap to RimBoard_Top_Face sockets
 * - Joist Top Face Sockets: Along the top for future plywood sheathing
 */
UCLASS()
class BORNTOSHINE_API AFloorJoist : public ARimBoard
{
	GENERATED_BODY()

public:
	AFloorJoist();

protected:
	virtual void InitializeSockets() override;

private:
	void CreateJoistEndSockets();
	void CreateJoistTopSockets();
};
