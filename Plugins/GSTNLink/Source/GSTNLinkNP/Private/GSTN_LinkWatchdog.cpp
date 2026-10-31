// Links, Network Prediction half.

#include "GSTN_LinkWatchdog.h"

#include "AbilitySystemComponent.h"
#include "Algo/BinarySearch.h"
#include "Components/CapsuleComponent.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Engine/World.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkSettings.h"
#include "GSTN_LinkTags.h"
#include "Placement/GSTN_LinkExitPlacer.h"
#include "Rules/GSTN_LinkRule_Grab.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "State/GSTN_LinkMath.h"
#include "Sync/GSTN_InboxEntry.h"
#include "World/GSTN_CombatWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLinkWatchdog, Log, All);

namespace
{
	const FGSTN_LinkWatchdogEntity* FindEntity(TConstArrayView<FGSTN_LinkWatchdogEntity> Entities, int32 EntityId)
	{
		const int32 Index = Algo::LowerBoundBy(Entities, EntityId, &FGSTN_LinkWatchdogEntity::EntityId);
		return Entities.IsValidIndex(Index) && Entities[Index].EntityId == EntityId ? &Entities[Index] : nullptr;
	}

	/** The partner holds the other end of this very link */
	bool IsMatched(const FGSTN_LinkWatchdogEntity& Entity, const FGSTN_LinkState& State, const FGSTN_LinkWatchdogEntity* Partner, EGSTN_LinkChannel Channel)
	{
		if (!Partner)
		{
			return false;
		}
		const FGSTN_LinkState& Other = Partner->Links.Get(Channel);
		return Other.IsActive() && Other.Serial == State.Serial && Other.LinkTypeIndex == State.LinkTypeIndex && Other.Role != State.Role
			&& static_cast<int32>(Other.PartnerId.Id) == Entity.EntityId && Other.PartnerId.Kind == GSTNEntityKinds::Fighter;
	}

	/** The grab rules a definition's request rules carry (the type's grab immunity), or null for a type that is not a grab */
	const FGSTN_GrabRules* FindGrabRules(const UGSTN_LinkTypeDefinition& Definition)
	{
		for (const UGSTN_LinkRule* Rule : Definition.Rules)
		{
			if (const UGSTN_LinkRule_Grab* Grab = Cast<UGSTN_LinkRule_Grab>(Rule))
			{
				return &Grab->Rules;
			}
		}
		return nullptr;
	}

	FGSTN_LinkBreakInput MakeInput(const FGSTN_LinkWatchdogEntity& Self, const FGSTN_LinkState& State, const FGSTN_LinkWatchdogEntity& Partner,
		const UGSTN_LinkTypeDefinition& Definition, int32 Frame, float ParkedDistanceCm)
	{
		FGSTN_LinkBreakInput Input;
		Input.Frame = Frame;
		Input.LinkFrame = State.LinkFrame;
		Input.EndFrame = State.EndFrame;
		Input.bPendingBreak = State.HasFlag(GSTNLinkFlags::PendingBreak);
		Input.PendingReason = State.GetBreakReason();
		Input.OwnRoleTag = Definition.GetRoleDef(State.Role).RoleTag;
		Input.PartnerDistanceCm = ParkedDistanceCm;
		Input.bOwnDead = Self.bDead;
		Input.bPartnerDead = Partner.bDead;
		Input.PartnerStateTags = Partner.StateTags;
		return Input;
	}

	uint64 PostKey(int32 EntityId, EGSTN_LinkChannel Channel, uint8 Serial)
	{
		return (static_cast<uint64>(static_cast<uint32>(EntityId)) << 16) | (static_cast<uint64>(Channel) << 8) | Serial;
	}
}

// ---- Pure logic

float FGSTN_LinkWatchdogLogic::ParkedDistance(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_LinkWatchdogEntity& Initiator, const FGSTN_LinkWatchdogEntity& Target)
{
	const int32 ParkedIndex = Definition.GetParkedRoleIndex();
	if (ParkedIndex == 0)
	{
		return 0.f;
	}
	const FGSTN_LinkWatchdogEntity& Parked = ParkedIndex == 1 ? Initiator : Target;
	const FGSTN_LinkWatchdogEntity& Anchor = ParkedIndex == 1 ? Target : Initiator;
	const FTransform Expected = UGSTN_LinkMath::ParkedTransform(Anchor.Transform, Definition.GetRoleDef(ParkedIndex));
	return static_cast<float>(FVector::Dist(Parked.Transform.GetLocation(), Expected.GetLocation()));
}

FGSTN_ExitResult FGSTN_LinkWatchdogLogic::PlaceExit(const UGSTN_LinkTypeDefinition& Definition, const FGSTN_LinkWatchdogEntity& Exiting,
	const FGSTN_LinkWatchdogEntity& Staying, const IGSTN_ExitCollisionQuery* Collision)
{
	FGSTN_ExitQuery Query;
	Query.PartnerTransform = Staying.Transform;
	Query.Up = Exiting.Up;
	Query.PreferredDirection = Staying.Transform.GetUnitAxis(EAxis::X);
	Query.CapsuleRadius = Exiting.CapsuleRadius;
	Query.CapsuleHalfHeight = Exiting.CapsuleHalfHeight;
	Query.Collision = Collision;
	FGSTN_ExitResult Exit = Definition.GetExitPlacer()->Place(Query);
	// The command carries floats: quantise here, so the authority's own copy and every client apply the same numbers
	const FVector3f Location(Exit.Transform.GetLocation());
	const float Yaw = static_cast<float>(Exit.Transform.Rotator().Yaw);
	Exit.Transform = FTransform(FRotator(0.0, Yaw, 0.0), FVector(Location));
	Exit.Velocity = FVector(FVector3f(Exit.Velocity));
	return Exit;
}

void FGSTN_LinkWatchdogLogic::Evaluate(TConstArrayView<FGSTN_LinkWatchdogEntity> Entities, int32 Frame, const FParams& Params,
	TArray<FGSTN_LinkWatchdogPost>& OutPosts, TArray<FGSTN_LinkWatchdogBreak>& OutBreaks)
{
	for (const FGSTN_LinkWatchdogEntity& Entity : Entities)
	{
		for (const EGSTN_LinkChannel Channel : { EGSTN_LinkChannel::Body, EGSTN_LinkChannel::Hand })
		{
			const FGSTN_LinkState& State = Entity.Links.Get(Channel);
			if (!State.IsActive() || State.Role == 0)
			{
				continue;
			}
			const UGSTN_LinkTypeDefinition* Definition = Params.Definitions ? Params.Definitions(State.LinkTypeIndex) : nullptr;
			if (!Definition)
			{
				continue;
			}
			const int32 PartnerId = State.PartnerId.IsNull() ? INDEX_NONE : static_cast<int32>(State.PartnerId.Id);
			const FGSTN_LinkWatchdogEntity* Partner = FindEntity(Entities, PartnerId);

			FGSTN_LinkWatchdogBreak Break;
			Break.EntityId = Entity.EntityId;
			Break.PartnerId = PartnerId;
			Break.Channel = Channel;
			Break.Serial = State.Serial;
			Break.LinkTypeIndex = State.LinkTypeIndex;

			const bool bMatched = IsMatched(Entity, State, Partner, Channel);
			if (!bMatched)
			{
				// One-sided: the target holds role 2 one frame before the initiator gets the response (formation), so only after the grace
				if (Frame - State.LinkFrame < Params.OrphanGraceFrames)
				{
					continue;
				}
				Break.bOrphan = true;
				Break.Reason = EGSTN_LinkBreakReason::Forced;
			}
			else
			{
				// A matched pair is decided once, by its lower entity id, from both points of view (the first side that breaks names the reason)
				if (Partner->EntityId < Entity.EntityId)
				{
					continue;
				}
				const FGSTN_LinkState& PartnerState = Partner->Links.Get(Channel);
				const FGSTN_LinkWatchdogEntity& Initiator = State.Role == 1 ? Entity : *Partner;
				const FGSTN_LinkWatchdogEntity& Target = State.Role == 1 ? *Partner : Entity;
				const float Distance = ParkedDistance(*Definition, Initiator, Target);
				const FGSTN_LinkBreakDecision Own = UGSTN_LinkMath::EvaluateBreak(Definition->Break, Definition->MaxFrames,
					MakeInput(Entity, State, *Partner, *Definition, Frame, Distance));
				const FGSTN_LinkBreakDecision Other = Own.bBreak ? Own : UGSTN_LinkMath::EvaluateBreak(Definition->Break, Definition->MaxFrames,
					MakeInput(*Partner, PartnerState, Entity, *Definition, Frame, Distance));
				if (!Own.bBreak && !Other.bBreak)
				{
					continue;
				}
				Break.Reason = Own.bBreak ? Own.Reason : Other.Reason;
			}

			// The command both sides apply at Frame + 1
			FGSTN_LinkCommandEntry Command;
			Command.Kind = EGSTN_LinkCommandKind::Break;
			Command.Channel = Channel;
			Command.Serial = State.Serial;
			Command.Reason = static_cast<uint8>(Break.Reason);

			const int32 ParkedIndex = Definition->GetParkedRoleIndex();
			const FGSTN_GrabRules* GrabRules = FindGrabRules(*Definition);
			const int32 ImmunityFrames = !GrabRules ? 0 : Break.Reason == EGSTN_LinkBreakReason::Throw ? GrabRules->ImmunityFramesAfterThrow : GrabRules->ImmunityFramesAfterRelease;

			const FGSTN_LinkWatchdogEntity* Sides[2] = { &Entity, bMatched ? Partner : nullptr };
			for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
			{
				const FGSTN_LinkWatchdogEntity* Side = Sides[SideIndex];
				if (!Side)
				{
					continue;
				}
				const FGSTN_LinkWatchdogEntity* Other = SideIndex == 0 ? (bMatched ? Partner : nullptr) : &Entity;
				const uint8 SideRole = Side->Links.Get(Channel).Role;
				FGSTN_LinkWatchdogPost& Post = OutPosts.AddDefaulted_GetRef();
				Post.TargetId = Side->EntityId;
				Post.SenderId = Other ? Other->EntityId : Side->EntityId;
				Post.StampFrame = Frame;
				Post.Command = Command;
				if (SideRole != 0 && SideRole == ParkedIndex)
				{
					// The parked side leaves through the type's placer, around the side that stays (an orphan's partner is gone: it stays where its capsule is)
					FGSTN_ExitResult Exit;
					if (Other)
					{
						Exit = PlaceExit(*Definition, *Side, *Other, Params.Collision);
					}
					else
					{
						Exit.Transform = Side->Transform;
						Exit.bValid = true;
					}
					Post.Command.ExitPosition = FVector3f(Exit.Transform.GetLocation());
					Post.Command.ExitYawDeg = static_cast<float>(Exit.Transform.Rotator().Yaw);
					Post.Command.ExitVelocity = FVector3f(Exit.Velocity);
					Post.Command.ExitFlags = GSTNLinkExitFlags::Teleport | (Exit.bValid ? 0 : GSTNLinkExitFlags::Fallback);
					Break.bExitFallback = !Exit.bValid;
				}
				// The grab's target gets its breather before it can be grabbed again
				if (GrabRules && SideRole == 2 && ImmunityFrames > 0)
				{
					Post.Command.ImmunityEndFrame = Frame + 1 + ImmunityFrames;
				}
			}
			OutBreaks.Add(Break);
		}
	}
}

// ---- World query

bool FGSTN_WorldExitCollisionQuery::IsCapsuleFree(const FVector& Location, const FQuat& Rotation, float Radius, float HalfHeight) const
{
	if (!World)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNLinkExit), false);
	for (const AActor* Actor : Ignored)
	{
		Params.AddIgnoredActor(Actor);
	}
	return !World->OverlapBlockingTestByChannel(Location, Rotation, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
}

// ---- Subsystem

UGSTN_LinkWatchdog* UGSTN_LinkWatchdog::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_LinkWatchdog>() : nullptr;
}

bool UGSTN_LinkWatchdog::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGSTN_LinkWatchdog::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UGSTN_LinkWatchdog::OnPostActorTick);
}

void UGSTN_LinkWatchdog::Deinitialize()
{
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
	Super::Deinitialize();
}

bool UGSTN_LinkWatchdog::IsAuthority() const
{
	// The documented exception (see the class comment): the watchdog is the authority's observer, not simulation code
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void UGSTN_LinkWatchdog::OnPostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World == GetWorld())
	{
		Evaluate();
	}
}

int32 UGSTN_LinkWatchdog::Evaluate()
{
	UWorld* World = GetWorld();
	UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!IsAuthority() || !Combat || !Registry)
	{
		return 0;
	}

	TArray<int32> Ids;
	Combat->GetRegisteredEntityIds(Ids);
	TArray<FGSTN_LinkWatchdogEntity> Entities;
	TArray<const AActor*, TInlineAllocator<16>> Actors;
	bool bAnyLink = false;
	int32 Frame = INDEX_NONE;
	for (const int32 Id : Ids)
	{
		AActor* Owner = Combat->FindOwnerActor(Id);
		const UGSTN_CombatSimComponent* Sim = Owner ? Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
		const FGSTN_CombatSyncState* Sync = Sim ? Sim->GetSyncState() : nullptr;
		if (!Sync)
		{
			continue;
		}
		FGSTN_LinkWatchdogEntity& Entity = Entities.AddDefaulted_GetRef();
		Actors.Add(Owner);
		Entity.EntityId = Id;
		Entity.Frame = Sync->Core.Input.History.GetLatestFrame();
		Frame = FMath::Max(Frame, Entity.Frame);
		if (const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments))
		{
			Entity.Links = Fragment->Links;
			bAnyLink |= Fragment->Links.Body.IsLinked() || Fragment->Links.Hand.IsLinked();
		}
		Entity.Transform = Owner->GetActorTransform();
		Entity.Up = Owner->GetActorUpVector();
		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Owner->GetRootComponent()))
		{
			Entity.CapsuleRadius = Capsule->GetScaledCapsuleRadius();
			Entity.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		Entity.bDead = Sync->Core.ActionState == EGSTN_ActionState::KO;
		if (const UAbilitySystemComponent* ASC = Owner->FindComponentByClass<UAbilitySystemComponent>())
		{
			ASC->GetOwnedGameplayTags(Entity.StateTags);
		}
	}
	if (!bAnyLink || Frame == INDEX_NONE)
	{
		PostedFrames.Reset();
		return 0;
	}
	Entities.Sort([](const FGSTN_LinkWatchdogEntity& A, const FGSTN_LinkWatchdogEntity& B) { return A.EntityId < B.EntityId; });

	const FGSTN_WorldExitCollisionQuery Collision(World, TArray<const AActor*, TInlineAllocator<2>>(Actors.GetData(), Actors.Num()));
	FGSTN_LinkWatchdogLogic::FParams Params;
	Params.OrphanGraceFrames = UGSTN_LinkSettings::Get().OrphanGraceFrames;
	Params.Collision = &Collision;
	Params.Definitions = [Registry](uint8 Index) { return Registry->GetDefinition(Index); };

	TArray<FGSTN_LinkWatchdogPost> Posts;
	TArray<FGSTN_LinkWatchdogBreak> Breaks;
	FGSTN_LinkWatchdogLogic::Evaluate(Entities, Frame, Params, Posts, Breaks);

	// A command waits one frame for its consumption: not posted again for the same serial until that frame has passed
	for (auto It = PostedFrames.CreateIterator(); It; ++It)
	{
		if (Frame > It.Value() + 2)
		{
			It.RemoveCurrent();
		}
	}
	TSet<uint64> Skipped;
	for (const FGSTN_LinkWatchdogBreak& Break : Breaks)
	{
		if (PostedFrames.Contains(PostKey(Break.EntityId, Break.Channel, Break.Serial)))
		{
			Skipped.Add(PostKey(Break.EntityId, Break.Channel, Break.Serial));
			Skipped.Add(PostKey(Break.PartnerId, Break.Channel, Break.Serial));
		}
	}

	int32 Posted = 0;
	for (const FGSTN_LinkWatchdogPost& Post : Posts)
	{
		const uint64 Key = PostKey(Post.TargetId, Post.Command.Channel, Post.Command.Serial);
		if (Skipped.Contains(Key))
		{
			continue;
		}
		FGSTN_InboxEntry Entry = FGSTN_LinkInboxHandlers::MakeCommand(Post.Command);
		Entry.Frame = Post.StampFrame;
		Entry.SenderId = Post.SenderId;
		Combat->PostToInbox(Post.TargetId, Entry);
		PostedFrames.Add(Key, Frame);
	}

	UGSTN_SimEventSubsystem* Events = UGSTN_SimEventSubsystem::Get(World);
	for (const FGSTN_LinkWatchdogBreak& Break : Breaks)
	{
		if (Skipped.Contains(PostKey(Break.EntityId, Break.Channel, Break.Serial)))
		{
			continue;
		}
		++Posted;
		++NumBreaks;
		NumOrphans += Break.bOrphan ? 1 : 0;
		LastBreak = Break;
		LastBreakFrame = Frame;
		UE_LOG(LogGSTNLinkWatchdog, Log, TEXT("Link break at frame %d: entity %d and %d, type %d, serial %d, reason %d%s%s"), Frame, Break.EntityId, Break.PartnerId,
			Break.LinkTypeIndex, Break.Serial, static_cast<int32>(Break.Reason), Break.bOrphan ? TEXT(", orphan") : TEXT(""),
			Break.bExitFallback ? TEXT(", exit fallback (no free place)") : TEXT(""));
		if (Events)
		{
			FGSTN_SimEvent Event;
			Event.EventType = GSTNLinkTags::Event_LinkBroken;
			Event.Frame = Frame;
			Event.Instigator = Combat->FindOwnerActor(Break.EntityId);
			Event.Target = Combat->FindOwnerActor(Break.PartnerId);
			Event.Magnitude = static_cast<float>(Break.Reason);
			const FGameplayTag ReasonTag = UGSTN_LinkMath::BreakReasonTag(Break.Reason);
			if (ReasonTag.IsValid())
			{
				Event.Tags.AddTag(ReasonTag);
			}
			Events->Broadcast(Event);
			if (Break.Reason == EGSTN_LinkBreakReason::Escape)
			{
				Event.EventType = GSTNLinkTags::Event_GrabEscaped;
				Events->Broadcast(Event);
			}
		}
	}
	return Posted;
}
