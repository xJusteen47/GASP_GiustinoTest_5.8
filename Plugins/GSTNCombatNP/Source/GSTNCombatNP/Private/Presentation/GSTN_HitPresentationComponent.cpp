// Combat simulation (GAS-NPP fork): the attacker's own hit presentation (combat-m4-hits 9b, M4 H10).

#include "Presentation/GSTN_HitPresentationComponent.h"

#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "GSTN_HitEffectContext.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"

UGSTN_HitPresentationComponent::UGSTN_HitPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Ticks only while an effect is alive
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

uint32 UGSTN_HitPresentationComponent::MakeHitId(const AActor* Attacker, const AActor* Victim, int32 Timeline, int32 HitboxIndex)
{
	return GSTNHitPresentation::MakeHitId(Attacker ? Attacker->GetUniqueID() : 0, Victim ? Victim->GetUniqueID() : 0, Timeline, HitboxIndex);
}

bool UGSTN_HitPresentationComponent::ShouldPlayAuthoritativeCue(const AActor* Attacker, const AActor* Victim, const FGameplayCueParameters& Params)
{
	UGSTN_HitPresentationComponent* Component = Attacker ? Attacker->FindComponentByClass<UGSTN_HitPresentationComponent>() : nullptr;
	const FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandle(Params.EffectContext);
	if (!Component || !Context)
	{
		return true;
	}
	if (Component->Dedup.ConsumeLocal(MakeHitId(Attacker, Victim, Context->TimelineIndex, Context->HitboxIndex)))
	{
		++Component->NumCuesSuppressed;
		return false;
	}
	return true;
}

FString UGSTN_HitPresentationComponent::GetPresentationInfo() const
{
	return FString::Printf(TEXT("%d,%d,%d,%d,%d,%.1f"), NumStarted, NumCancelled, NumCuesPlayed, NumCuesSuppressed, Effects.Num(), LastHitstopMs);
}

USkeletalMeshComponent* UGSTN_HitPresentationComponent::FindMesh(const AActor* Actor) const
{
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		return Character->GetMesh();
	}
	return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UGSTN_HitPresentationComponent::PauseMesh(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return;
	}
	FPauseState& State = PausedMeshes.FindOrAdd(Mesh);
	if (State.Holders++ == 0)
	{
		State.OldRate = Mesh->GlobalAnimRateScale;
		Mesh->GlobalAnimRateScale = 0.f;
	}
}

void UGSTN_HitPresentationComponent::ReleaseMesh(USkeletalMeshComponent* Mesh)
{
	FPauseState* State = Mesh ? PausedMeshes.Find(Mesh) : nullptr;
	if (!State)
	{
		return;
	}
	if (--State->Holders <= 0)
	{
		Mesh->GlobalAnimRateScale = State->OldRate;
		PausedMeshes.Remove(Mesh);
	}
}

void UGSTN_HitPresentationComponent::SetFlash(AActor* Victim, float Value)
{
	if (FlashParameterName.IsNone() || !Victim)
	{
		return;
	}
	TArray<USkeletalMeshComponent*, TInlineAllocator<4>> Meshes;
	Victim->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		Mesh->SetScalarParameterValueOnMaterials(FlashParameterName, Value);
	}
}

void UGSTN_HitPresentationComponent::OnAttackerFrame(const FGSTN_CombatSyncCore& Core, int32 Frame, int32 AttackerId, TConstArrayView<FGSTN_PredictedHitView> Views, float StepMs)
{
	if (!bEnabled)
	{
		return;
	}
	const bool bRunLive = Core.ActionState == EGSTN_ActionState::InTimeline && Core.TimelineStartFrame != INDEX_NONE;
	FGSTN_HitPresentationTracker::FDelta Delta;
	Tracker.Update(Core.Hit, bRunLive, Core.TimelineStartFrame, Delta);

	for (const FGSTN_HitPresentationKey& Key : Delta.Cancelled)
	{
		CancelEffect(Key);
	}
	for (const FGSTN_HitPresentationKey& Key : Delta.Started)
	{
		// The latest prediction for this victim and group inside the running move (a re-simulation replaces earlier ones)
		const FGSTN_PredictedHitView* Found = nullptr;
		for (const FGSTN_PredictedHitView& View : Views)
		{
			if (View.VictimId == Key.VictimId && View.HitGroup == Key.HitGroup && !View.bRicochet && View.Frame >= Key.TimelineStartFrame && (!Found || View.Frame > Found->Frame))
			{
				Found = &View;
			}
		}
		if (Found)
		{
			StartEffect(Key, *Found, Frame, AttackerId, StepMs);
		}
	}
	Dedup.Prune(Frame);
	UpdateTickEnabled();
}

void UGSTN_HitPresentationComponent::StartEffect(const FGSTN_HitPresentationKey& Key, const FGSTN_PredictedHitView& View, int32 Frame, int32 AttackerId, float StepMs)
{
	AActor* Attacker = GetOwner();
	UGSTN_CombatWorldSubsystem* World = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	AActor* Victim = World ? World->FindOwnerActor(Key.VictimId) : nullptr;
	if (!Attacker || !Victim)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	++NumStarted;

	FEffect& Effect = Effects.AddDefaulted_GetRef();
	Effect.Key = Key;
	Effect.HitId = MakeHitId(Attacker, Victim, View.Timeline, View.HitboxIndex);
	Effect.Victim = Victim;
	Effect.StartTime = Now;
	Effect.VictimMesh = FindMesh(Victim);

	if (bExecuteCueLocal && View.CueTag.IsValid())
	{
		if (UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			FGameplayCueParameters Params;
			Params.Location = View.HitLocation;
			Params.Instigator = Attacker;
			Params.EffectCauser = Attacker;
			Params.EffectContext = FGSTN_HitEffectContext::Make(Attacker, Attacker, nullptr);
			if (FGSTN_HitEffectContext* Context = FGSTN_HitEffectContext::FromHandleMutable(Params.EffectContext))
			{
				Context->HitLocation = FVector_NetQuantize10(View.HitLocation);
				Context->TimelineIndex = View.Timeline;
				Context->HitboxIndex = View.HitboxIndex;
			}
			CueManager->HandleGameplayCue(Victim, View.CueTag, EGameplayCueEvent::Executed, Params);
			Dedup.NoteLocal(Effect.HitId, Frame);
			++NumCuesPlayed;
		}
	}

	if (!FlashParameterName.IsNone() && FlashSeconds > 0.f)
	{
		Effect.bFlashing = true;
		SetFlash(Victim, 1.f);
	}

	if (bPauseMeshes)
	{
		const double Step = StepMs * 0.001;
		if (View.AttackerHitstopFrames > 0)
		{
			if (USkeletalMeshComponent* Mesh = FindMesh(Attacker))
			{
				PauseMesh(Mesh);
				Effect.bAttackerPaused = true;
				Effect.AttackerPauseEnd = Now + View.AttackerHitstopFrames * Step;
				LastHitstopMs = View.AttackerHitstopFrames * StepMs;
			}
		}
		if (View.bHasKnockback && View.Knockback.HitstopFrames > 0 && Effect.VictimMesh.IsValid())
		{
			PauseMesh(Effect.VictimMesh.Get());
			Effect.bVictimPaused = true;
			Effect.VictimPauseEnd = Now + View.Knockback.HitstopFrames * Step;
		}
	}

	// The victim reacts after its hitstop; a victim that ticks here (the host's world) moves by itself and needs no extrapolation
	if (bPredictVictimReaction && View.bHasKnockback && Victim->GetLocalRole() == ROLE_SimulatedProxy && Effect.VictimMesh.IsValid() && !View.Knockback.LaunchVelocity.IsNearlyZero())
	{
		Effect.bReacting = true;
		Effect.ReactionVelocity = View.Knockback.LaunchVelocity;
		Effect.ReactionStart = Now + View.Knockback.HitstopFrames * StepMs * 0.001;
	}
}

void UGSTN_HitPresentationComponent::EndEffect(FEffect& Effect)
{
	AActor* Victim = Effect.Victim.Get();
	if (Effect.bFlashing)
	{
		SetFlash(Victim, 0.f);
		Effect.bFlashing = false;
	}
	if (Effect.bAttackerPaused)
	{
		ReleaseMesh(FindMesh(GetOwner()));
		Effect.bAttackerPaused = false;
	}
	if (Effect.bVictimPaused)
	{
		ReleaseMesh(Effect.VictimMesh.Get());
		Effect.bVictimPaused = false;
	}
	if (USkeletalMeshComponent* Mesh = Effect.VictimMesh.Get(); Mesh && !Effect.AppliedOffset.IsZero())
	{
		Mesh->AddWorldOffset(-Effect.AppliedOffset);
		Effect.AppliedOffset = FVector::ZeroVector;
	}
}

void UGSTN_HitPresentationComponent::CancelEffect(const FGSTN_HitPresentationKey& Key)
{
	for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
	{
		if (Effects[Index].Key == Key)
		{
			// The prediction did not stand: the real cue, if one comes, is the only one
			Dedup.Forget(Effects[Index].HitId);
			EndEffect(Effects[Index]);
			Effects.RemoveAtSwap(Index);
			++NumCancelled;
		}
	}
	UpdateTickEnabled();
}

void UGSTN_HitPresentationComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(!Effects.IsEmpty());
}

void UGSTN_HitPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = Effects.Num() - 1; Index >= 0; --Index)
	{
		FEffect& Effect = Effects[Index];
		bool bAlive = false;

		if (Effect.bFlashing)
		{
			const double Age = Now - Effect.StartTime;
			if (Age >= FlashSeconds)
			{
				SetFlash(Effect.Victim.Get(), 0.f);
				Effect.bFlashing = false;
			}
			else
			{
				SetFlash(Effect.Victim.Get(), static_cast<float>(1.0 - Age / FlashSeconds));
				bAlive = true;
			}
		}
		if (Effect.bAttackerPaused)
		{
			if (Now >= Effect.AttackerPauseEnd)
			{
				ReleaseMesh(FindMesh(GetOwner()));
				Effect.bAttackerPaused = false;
			}
			bAlive |= Effect.bAttackerPaused;
		}
		if (Effect.bVictimPaused)
		{
			if (Now >= Effect.VictimPauseEnd)
			{
				ReleaseMesh(Effect.VictimMesh.Get());
				Effect.bVictimPaused = false;
			}
			bAlive |= Effect.bVictimPaused;
		}

		if (Effect.bReacting)
		{
			USkeletalMeshComponent* Mesh = Effect.VictimMesh.Get();
			const double T = Now - Effect.ReactionStart;
			if (!Mesh || T >= ReactionSeconds)
			{
				if (Mesh && !Effect.AppliedOffset.IsZero())
				{
					Mesh->AddWorldOffset(-Effect.AppliedOffset);
				}
				Effect.AppliedOffset = FVector::ZeroVector;
				Effect.bReacting = false;
			}
			else
			{
				bAlive = true;
				if (T > 0.0)
				{
					// The velocity decays linearly over the window, and the whole offset fades out over its second half: the authoritative motion
					// takes the mesh over. It moves by deltas so it composes with whatever else positions the mesh
					const double Duration = FMath::Max<double>(ReactionSeconds, UE_SMALL_NUMBER);
					const double Travel = T - T * T / (2.0 * Duration);
					const double Fade = T < Duration * 0.5 ? 1.0 : 1.0 - (T - Duration * 0.5) / (Duration * 0.5);
					FVector Offset = Effect.ReactionVelocity * Travel * Fade;
					if (Offset.SizeSquared() > FMath::Square(static_cast<double>(MaxReactionOffset)))
					{
						Offset = Offset.GetSafeNormal() * MaxReactionOffset;
					}
					Mesh->AddWorldOffset(Offset - Effect.AppliedOffset);
					Effect.AppliedOffset = Offset;
				}
			}
		}

		if (!bAlive)
		{
			Effects.RemoveAtSwap(Index);
		}
	}
	UpdateTickEnabled();
}

void UGSTN_HitPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FEffect& Effect : Effects)
	{
		EndEffect(Effect);
	}
	Effects.Reset();
	Super::EndPlay(EndPlayReason);
}
