// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTN_PlatformCollision.h"

#include "GSTN_SurfaceTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_PlatformCollision)

namespace GSTNPlatformCollision
{
	namespace
	{
		FGSTN_PlatformCollisionRule MakeRule(std::initializer_list<FGameplayTag> AnyOf, TConstArrayView<TEnumAsByte<ECollisionChannel>> Channels)
		{
			FGameplayTagContainer Tags;
			for (const FGameplayTag& Tag : AnyOf)
			{
				Tags.AddTag(Tag);
			}
			FGSTN_PlatformCollisionRule Rule;
			Rule.When = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
			Rule.BlockChannels = TArray<TEnumAsByte<ECollisionChannel>>(Channels.GetData(), Channels.Num());
			return Rule;
		}
	}

	TArray<FGSTN_PlatformCollisionRule> MakeDefaultRules(TConstArrayView<TEnumAsByte<ECollisionChannel>> MoverChannels,
		TConstArrayView<TEnumAsByte<ECollisionChannel>> WallProbeChannels, TConstArrayView<TEnumAsByte<ECollisionChannel>> StandAndLaunchExtraChannels)
	{
		using namespace GSTNSurfaceTags;
		TArray<FGSTN_PlatformCollisionRule> Rules;
		Rules.Add(MakeRule({ Behaviour_Stand, Behaviour_WallSlide, Behaviour_WallClimb, Behaviour_Launch }, MoverChannels));
		Rules.Add(MakeRule({ Behaviour_WallSlide, Behaviour_WallClimb }, WallProbeChannels));
		if (StandAndLaunchExtraChannels.Num() > 0)
		{
			Rules.Add(MakeRule({ Behaviour_Stand, Behaviour_Launch }, StandAndLaunchExtraChannels));
		}
		return Rules;
	}

	bool DeriveResponses(const FGSTN_SurfaceBehaviour& Behaviour, TConstArrayView<FGSTN_PlatformCollisionRule> Rules,
		FCollisionResponseContainer& OutResponses)
	{
		OutResponses.SetAllChannels(ECR_Ignore);
		bool bAnyBlock = false;
		for (const FGSTN_PlatformCollisionRule& Rule : Rules)
		{
			if (Rule.When.IsEmpty() || !Rule.When.Matches(Behaviour.Behaviours))
			{
				continue;
			}
			for (const TEnumAsByte<ECollisionChannel> Channel : Rule.BlockChannels)
			{
				OutResponses.SetResponse(Channel, ECR_Block);
				bAnyBlock = true;
			}
		}
		return bAnyBlock;
	}
}
