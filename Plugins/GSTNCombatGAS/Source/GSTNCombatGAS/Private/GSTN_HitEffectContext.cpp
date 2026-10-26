// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "GSTN_HitEffectContext.h"
#include "Rules/GSTN_CombatRules.h"

FGameplayEffectContextHandle FGSTN_HitEffectContext::Make(AActor* Instigator, AActor* Causer, const UGSTN_CombatRules* InRules)
{
	FGSTN_HitEffectContext* Context = new FGSTN_HitEffectContext();
	Context->Rules = InRules;
	FGameplayEffectContextHandle Handle(Context);
	Handle.AddInstigator(Instigator, Causer);
	return Handle;
}

const FGSTN_HitEffectContext* FGSTN_HitEffectContext::FromHandle(const FGameplayEffectContextHandle& Handle)
{
	return FromHandleMutable(Handle);
}

FGSTN_HitEffectContext* FGSTN_HitEffectContext::FromHandleMutable(const FGameplayEffectContextHandle& Handle)
{
	const FGameplayEffectContext* Context = Handle.Get();
	if (Context != nullptr && Context->GetScriptStruct() == StaticStruct())
	{
		return const_cast<FGSTN_HitEffectContext*>(static_cast<const FGSTN_HitEffectContext*>(Context));
	}
	return nullptr;
}

FGSTN_HitEffectContext* FGSTN_HitEffectContext::Duplicate() const
{
	// Copying the whole struct carries the hit fields; the base then re-adds the hit result so the copy owns its own
	FGSTN_HitEffectContext* NewContext = new FGSTN_HitEffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

bool FGSTN_HitEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	// Indices are small and often INDEX_NONE: shifted by one they pack into one byte in the common case
	int32 Fields[4] = { TimelineIndex + 1, HitboxIndex + 1, HurtboxIndex + 1, StartFrame + 1 };
	for (int32& Field : Fields)
	{
		uint32 Packed = static_cast<uint32>(FMath::Max(Field, 0));
		Ar.SerializeIntPacked(Packed);
		Field = static_cast<int32>(Packed);
	}
	TimelineIndex = Fields[0] - 1;
	HitboxIndex = Fields[1] - 1;
	HurtboxIndex = Fields[2] - 1;
	StartFrame = Fields[3] - 1;

	bool bOk = true;
	HitLocation.NetSerialize(Ar, Map, bOk);
	Relationship.NetSerialize(Ar, Map, bOk);
	KOReason.NetSerialize(Ar, Map, bOk);

	uint8 ReactionByte = static_cast<uint8>(Reaction);
	Ar << ReactionByte;
	Reaction = static_cast<EGSTN_HitReaction>(ReactionByte);

	uint8 StaggeredBit = bStaggered ? 1 : 0;
	Ar.SerializeBits(&StaggeredBit, 1);
	bStaggered = StaggeredBit != 0;

	bOutSuccess = bOk;
	return true;
}
