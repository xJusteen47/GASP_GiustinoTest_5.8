// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_LinkTypeDefinition.h"

#include "Placement/GSTN_Placer_Ring.h"
#include "Rules/GSTN_LinkRule.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	/** Whether the tag is registered and sits strictly under the root. Compared by name so the check needs no root tag to exist */
	bool IsUnderRoot(const FGameplayTag& Tag, const TCHAR* Root)
	{
		return Tag.IsValid() && Tag.ToString().StartsWith(FString(Root) + TEXT("."));
	}

	void ValidateRole(const FString& Name, const TCHAR* Label, const FGSTN_LinkRoleDef& Role, TArray<FString>& OutErrors)
	{
		if (!Role.RoleTag.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the %s role has no role tag, or its tag is not registered (the plugin that declares it is missing)."), *Name, Label));
		}
		else if (!IsUnderRoot(Role.RoleTag, TEXT("Combat.Link.Role")))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the %s role tag '%s' is not under Combat.Link.Role."), *Name, Label, *Role.RoleTag.ToString()));
		}
		if (Role.SocketOffset.ContainsNaN() || !Role.SocketOffset.IsRotationNormalized())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: the %s role's socket offset is not a valid transform."), *Name, Label));
		}
	}
}

const UGSTN_LinkExitPlacer* UGSTN_LinkTypeDefinition::GetExitPlacer() const
{
	return ExitPlacer != nullptr ? ExitPlacer.Get() : GetDefault<UGSTN_Placer_Ring>();
}

int32 UGSTN_LinkTypeDefinition::GetParkedRoleIndex() const
{
	if (InitiatorRole.bParked)
	{
		return 1;
	}
	return TargetRole.bParked ? 2 : 0;
}

int32 UGSTN_LinkTypeDefinition::FindRoleIndex(const FGameplayTag& RoleTag) const
{
	if (!RoleTag.IsValid())
	{
		return 0;
	}
	if (InitiatorRole.RoleTag == RoleTag)
	{
		return 1;
	}
	return TargetRole.RoleTag == RoleTag ? 2 : 0;
}

bool UGSTN_LinkTypeDefinition::CanCoexistWith(const UGSTN_LinkTypeDefinition& Other) const
{
	// One slot per channel: two links of one channel never coexist
	if (Channel == Other.Channel)
	{
		return false;
	}
	const UGSTN_LinkTypeDefinition& HandType = Channel == EGSTN_LinkChannel::Hand ? *this : Other;
	const UGSTN_LinkTypeDefinition& BodyType = Channel == EGSTN_LinkChannel::Hand ? Other : *this;
	return !HandType.ForbiddenWithBody.Contains(BodyType.LinkType);
}

void UGSTN_LinkTypeDefinition::Validate(TArray<FString>& OutErrors) const
{
	const FString Name = GetName();

	if (!LinkType.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the link type tag is not set, or is not registered (the plugin that declares it is missing)."), *Name));
	}
	else if (!IsUnderRoot(LinkType, TEXT("Combat.Link.Type")))
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the link type tag '%s' is not under Combat.Link.Type."), *Name, *LinkType.ToString()));
	}

	ValidateRole(Name, TEXT("initiator"), InitiatorRole, OutErrors);
	ValidateRole(Name, TEXT("target"), TargetRole, OutErrors);
	if (InitiatorRole.RoleTag.IsValid() && InitiatorRole.RoleTag == TargetRole.RoleTag)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: both roles carry the tag '%s'; the roles must be told apart."), *Name, *InitiatorRole.RoleTag.ToString()));
	}

	if (InitiatorRole.bParked && TargetRole.bParked)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: both roles are parked; at most one role of a link is parked on the other."), *Name));
	}
	if (Channel == EGSTN_LinkChannel::Hand && GetParkedRoleIndex() != 0)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: a Hand-channel link never parks an entity; only the Body channel does."), *Name));
	}

	if (MaxFrames < 0)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MaxFrames %d is negative."), *Name, MaxFrames));
	}
	if (Break.MaxDistanceCm < 0.f)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: MaxDistanceCm %.1f is negative."), *Name, Break.MaxDistanceCm));
	}
	for (const FGSTN_RoleHitBreak& HitBreak : Break.HitBreaks)
	{
		if (FindRoleIndex(HitBreak.RoleTag) == 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: a hit break names the role '%s', which is neither role of the definition."), *Name, *HitBreak.RoleTag.ToString()));
		}
		if (HitBreak.MinKnockbackToBreak < 0.f)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: a hit break for '%s' has a negative knockback threshold."), *Name, *HitBreak.RoleTag.ToString()));
		}
	}

	for (int32 RuleIndex = 0; RuleIndex < Rules.Num(); ++RuleIndex)
	{
		if (Rules[RuleIndex] == nullptr)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: request rule %d is empty."), *Name, RuleIndex));
		}
	}

	if (!ForbiddenWithBody.IsEmpty() && Channel != EGSTN_LinkChannel::Hand)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: ForbiddenWithBody is only meaningful on a Hand-channel type; a Body type excludes every other Body type by itself."), *Name));
	}
	for (const FGameplayTag& Forbidden : ForbiddenWithBody)
	{
		if (!IsUnderRoot(Forbidden, TEXT("Combat.Link.Type")))
		{
			OutErrors.Add(FString::Printf(TEXT("%s: ForbiddenWithBody lists '%s', which is not a registered tag under Combat.Link.Type."), *Name, *Forbidden.ToString()));
		}
	}
}

FPrimaryAssetId UGSTN_LinkTypeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNLinkAssetTypes::LinkType(), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_LinkTypeDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FString> Errors;
	Validate(Errors);
	for (const FString& Error : Errors)
	{
		Context.AddError(FText::FromString(Error));
		Result = EDataValidationResult::Invalid;
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
