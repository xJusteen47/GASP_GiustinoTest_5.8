#include "GSTN_PawnData.h"
#include "GameFramework/Pawn.h"

FPrimaryAssetId UGSTN_PawnData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetAssetType(), GetFName());
}

TSubclassOf<APawn> UGSTN_PawnData::LoadPawnClass() const
{
	return PawnClass.LoadSynchronous();
}
