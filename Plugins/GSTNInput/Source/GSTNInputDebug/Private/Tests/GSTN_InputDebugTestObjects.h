// Test-only objects of GSTNInputDebug.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Debug/GSTN_InputHistorySource.h"
#include "GSTN_InputDebugTestObjects.generated.h"

/** A source over a history the test owns, with a request it can set */
class FGSTN_TestHistorySource : public IGSTN_InputHistorySource
{
public:
	FGSTN_TestHistorySource(const UObject* InObject, const FGSTN_InputHistory* InHistory)
		: Object(InObject), History(InHistory)
	{
	}

	virtual bool IsSourceValid() const override { return Object.IsValid() && bValid; }
	virtual const FGSTN_InputHistory* GetInputHistoryView() const override { return History; }
	virtual bool GetLatestCommandRequest(int32& OutFrame, FGameplayTag& OutCommand) const override
	{
		OutFrame = RequestFrame;
		OutCommand = RequestCommand;
		return RequestFrame != INDEX_NONE;
	}
	virtual void GetButtonTags(TArray<FGameplayTag>& OutTags) const override { OutTags = Buttons; }
	virtual FText GetSourceLabel() const override { return FText::FromString(Label); }
	virtual const UObject* GetSourceObject() const override { return Object.Get(); }

	TWeakObjectPtr<const UObject> Object;
	const FGSTN_InputHistory* History = nullptr;
	bool bValid = true;
	int32 RequestFrame = INDEX_NONE;
	FGameplayTag RequestCommand;
	TArray<FGameplayTag> Buttons;
	FString Label = TEXT("test source");
};

/** A pawn component that implements the provider interface: found by the registry with no finder */
UCLASS(NotBlueprintable, Transient)
class UGSTN_TestProviderComponent : public UActorComponent, public IGSTN_InputHistoryProvider
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<IGSTN_InputHistorySource> CreateInputHistorySource() override
	{
		++NumCreated;
		return MakeShared<FGSTN_TestHistorySource>(this, &History);
	}

	FGSTN_InputHistory History;
	int32 NumCreated = 0;
};
