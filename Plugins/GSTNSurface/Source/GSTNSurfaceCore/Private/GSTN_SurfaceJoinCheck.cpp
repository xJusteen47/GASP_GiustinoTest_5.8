// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTN_SurfaceJoinCheck.h"

#include "Engine/NetConnection.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_SurfaceSettings.h"
#include "GSTN_SurfaceUtils.h"
#include "GSTNSurfaceCoreModule.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "GSTNSurface"

namespace GSTNSurfaceJoinPrivate
{
	/** "name?a=b?GSTNSurfaceTypes=c" -> "name?a=b": drops every occurrence of the option, so AppendJoinOption never announces twice */
	FString WithoutOption(const FString& Options)
	{
		TArray<FString> Parts;
		Options.ParseIntoArray(Parts, TEXT("?"), /*CullEmpty*/ false);
		const FString Prefix = FString(FGSTN_SurfaceJoinCheck::GetOptionName()) + TEXT("=");
		FString Result;
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			if (Index > 0 && Parts[Index].StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				continue;
			}
			Result += (Index > 0 ? TEXT("?") : TEXT("")) + Parts[Index];
		}
		return Result;
	}
}

const TCHAR* FGSTN_SurfaceJoinCheck::GetOptionName()
{
	return TEXT("GSTNSurfaceTypes");
}

FString FGSTN_SurfaceJoinCheck::MakeOption(uint32 Checksum)
{
	return FString::Printf(TEXT("%s=%08X"), GetOptionName(), Checksum);
}

FString FGSTN_SurfaceJoinCheck::GetJoinOption()
{
	return MakeOption(UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum());
}

void FGSTN_SurfaceJoinCheck::AppendJoinOption(FString& Options)
{
	Options = GSTNSurfaceJoinPrivate::WithoutOption(Options);
	Options += TEXT("?") + GetJoinOption();
}

EGSTN_SurfaceJoinVerdict FGSTN_SurfaceJoinCheck::Evaluate(const FString& RequestOptions, uint32 ServerChecksum, uint32* OutClientChecksum)
{
	// A connection stores the whole URL ("/Game/Map?Name=Bob?..."), and ParseOption reads only a string that starts at the first '?'
	int32 FirstOption = INDEX_NONE;
	if (!RequestOptions.FindChar(TEXT('?'), FirstOption))
	{
		return EGSTN_SurfaceJoinVerdict::MissingOption;
	}
	const FString Value = UGameplayStatics::ParseOption(RequestOptions.Mid(FirstOption), GetOptionName());
	if (Value.IsEmpty())
	{
		return EGSTN_SurfaceJoinVerdict::MissingOption;
	}

	// Exactly eight hex digits: anything else is not a checksum this plugin wrote
	bool bWellFormed = Value.Len() == 8;
	for (int32 Index = 0; bWellFormed && Index < Value.Len(); ++Index)
	{
		bWellFormed = FChar::IsHexDigit(Value[Index]);
	}
	if (!bWellFormed)
	{
		return EGSTN_SurfaceJoinVerdict::Mismatch;
	}

	const uint32 ClientChecksum = static_cast<uint32>(FCString::Strtoui64(*Value, nullptr, 16));
	if (OutClientChecksum)
	{
		*OutClientChecksum = ClientChecksum;
	}
	return ClientChecksum == ServerChecksum ? EGSTN_SurfaceJoinVerdict::Match : EGSTN_SurfaceJoinVerdict::Mismatch;
}

void FGSTN_SurfaceJoinCheck::CheckLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || !NewPlayer || GameMode->GetNetMode() == NM_Client)
	{
		return;
	}
	// The host of a listen server and a standalone player have no connection: they run the server's own table
	UNetConnection* Connection = NewPlayer->GetNetConnection();
	if (!Connection)
	{
		return;
	}

	const uint32 ServerChecksum = UGSTN_SurfaceUtils::GetSurfaceTypeTableChecksum();
	uint32 ClientChecksum = 0;
	const EGSTN_SurfaceJoinVerdict Verdict = Evaluate(Connection->RequestURL, ServerChecksum, &ClientChecksum);

	FText KickReason;
	switch (Verdict)
	{
	case EGSTN_SurfaceJoinVerdict::Match:
		return;
	case EGSTN_SurfaceJoinVerdict::MissingOption:
		if (!UGSTN_SurfaceSettings::Get().bRequireJoinChecksum)
		{
			UE_LOG(LogGSTNSurface, Warning, TEXT("%s joined without the %s option, so its surface type table cannot be compared with the server's (checksum %08X). Return FGSTN_SurfaceJoinCheck::GetJoinOption from ULocalPlayer::GetGameLoginOptions"),
				*GetNameSafe(NewPlayer), FGSTN_SurfaceJoinCheck::GetOptionName(), ServerChecksum);
			return;
		}
		KickReason = LOCTEXT("JoinCheckMissing", "Your game did not announce its surface type table.");
		break;
	case EGSTN_SurfaceJoinVerdict::Mismatch:
		KickReason = LOCTEXT("JoinCheckMismatch", "Your game and the server disagree on the surface types (different Surface.Type tags).");
		break;
	}

	const FString ClientText = Verdict == EGSTN_SurfaceJoinVerdict::MissingOption
		? FString(TEXT("not announced")) : FString::Printf(TEXT("%08X"), ClientChecksum);
	UE_LOG(LogGSTNSurface, Error, TEXT("Kicking %s: its surface type table does not match the server's (server %08X, client %s)"),
		*GetNameSafe(NewPlayer), ServerChecksum, *ClientText);

	if (GameMode->GameSession)
	{
		GameMode->GameSession->KickPlayer(NewPlayer, KickReason);
	}
	else
	{
		NewPlayer->ClientWasKicked(KickReason);
		Connection->Close();
	}
}

#undef LOCTEXT_NAMESPACE
