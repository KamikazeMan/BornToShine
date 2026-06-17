// Born To Shine - Game instance

#include "BornToShineGameInstance.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* GiSaveSlotName = TEXT("BornToShineSlot");
	constexpr int32 GiSaveUserIndex = 0;
}

bool UBornToShineGameInstance::HasExistingSave() const
{
	return UGameplayStatics::DoesSaveGameExist(GiSaveSlotName, GiSaveUserIndex);
}
