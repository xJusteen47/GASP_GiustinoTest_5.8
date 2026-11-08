// Links, Network Prediction half.

#include "GSTN_LinkPresentationComponent.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "HAL/IConsoleManager.h"
#include "NetworkPredictionWorldManager.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Data/GSTN_LinkTypeDefinition.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GSTN_LinkAnimSelection.h"
#include "GSTN_LinkDebugDraw.h"
#include "GSTN_LinkRegistry.h"
#include "MoverComponent.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkSnapshotExt.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "State/GSTN_LinkMath.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace GSTNLinkPresentationPrivate
{
	TAutoConsoleVariable<int32> CVarPredictPartnerPresentation(TEXT("gstn.Link.PredictPartnerPresentation"), 1,
		TEXT("1 (default): the initiator of a partner move shows the partner's authored reaction from its own predicted fragment (FP G1, presentation only). 0 turns it off."));

	/** The frame data of a partner ability: its class default's timeline, which is what the timeline registry of the partner's table set holds */
	const FGSTN_CombatTimelineData* ResolvePartnerTimeline(const UClass* PartnerAbility)
	{
		const UGSTN_GA_BaseCombat* Default = PartnerAbility ? Cast<UGSTN_GA_BaseCombat>(PartnerAbility->GetDefaultObject()) : nullptr;
		return Default ? &Default->GetTimelineData() : nullptr;
	}

	/** Montage positions closer than this to the predicted one are left alone, as the fork's finalize leaves them (no notify churn) */
	constexpr float LeadEpsilonSeconds = 0.01f;

	USkeletalMeshComponent* FindMesh(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}

	/** The loadout of a fighter: on the actor, else on its PlayerState (where combat-m3 puts it), as the combat component finds it */
	const UGSTN_LoadoutComponent* FindLoadout(const AActor* Actor)
	{
		const UGSTN_LoadoutComponent* Found = Actor ? Actor->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		if (!Found)
		{
			const APawn* Pawn = Cast<APawn>(Actor);
			const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
			Found = PlayerState ? PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
		}
		return Found;
	}
}

UGSTN_LinkPresentationComponent::UGSTN_LinkPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the simulation and Mover's presentation moved the actors of this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	// FP G1: the early tick. After the Network Prediction finalize (it runs from OnWorldPreActorTick, before every tick group) and before the
	// partner's mesh animates (TG_PrePhysics, ordered by a prerequisite on the mesh while a window leads its montage)
	PredictTick.bCanEverTick = true;
	PredictTick.bStartWithTickEnabled = true;
	PredictTick.bAllowTickOnDedicatedServer = false;
	PredictTick.TickGroup = TG_PrePhysics;
	PredictTick.EndTickGroup = TG_PrePhysics;
}

void FGSTN_LinkPredictTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (Target && IsValid(Target) && TickType != LEVELTICK_ViewportsOnly)
	{
		FScopeCycleCounterUObject ComponentScope(Target);
		Target->TickPredictedPartner(DeltaTime);
	}
}

FString FGSTN_LinkPredictTickFunction::DiagnosticMessage()
{
	return Target ? Target->GetFullName() + TEXT("[PredictedPartner]") : FString(TEXT("GSTN_LinkPresentation[PredictedPartner]"));
}

FName FGSTN_LinkPredictTickFunction::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("GSTN_LinkPredictedPartner"));
}

void UGSTN_LinkPresentationComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);
	if (bRegister)
	{
		// A dedicated server never presents anything
		AActor* Owner = GetOwner();
		if (Owner && Owner->GetLevel() && GetNetMode() != NM_DedicatedServer && !PredictTick.IsTickFunctionRegistered())
		{
			PredictTick.Target = this;
			PredictTick.RegisterTickFunction(Owner->GetLevel());
		}
	}
	else if (PredictTick.IsTickFunctionRegistered())
	{
		PredictTick.UnRegisterTickFunction();
	}
}

const FGSTN_LinkFragment* UGSTN_LinkPresentationComponent::FindShownFragment(const AActor* Actor)
{
	const UGSTN_CombatSimComponent* Combat = Actor ? Actor->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
	const FGSTN_CombatSyncState* Sync = Combat ? Combat->GetSyncState() : nullptr;
	return Sync ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments) : nullptr;
}

UGSTN_LinkPresentationComponent* UGSTN_LinkPresentationComponent::FindOrAdd(AActor* Owner)
{
	if (!Owner)
	{
		return nullptr;
	}
	if (UGSTN_LinkPresentationComponent* Existing = Owner->FindComponentByClass<UGSTN_LinkPresentationComponent>())
	{
		return Existing;
	}
	UGSTN_LinkPresentationComponent* Component = NewObject<UGSTN_LinkPresentationComponent>(Owner, TEXT("GSTN_LinkPresentation"));
	Component->RegisterComponent();
	return Component;
}

bool UGSTN_LinkPresentationComponent::GetLinkedTransform(FTransform& OutTransform) const
{
	if (!bParkedShown)
	{
		return false;
	}
	OutTransform = LinkedTransform;
	return true;
}

bool UGSTN_LinkPresentationComponent::UpdateLinkedTransform(FGameplayTag& OutRoleTag)
{
	AActor* Owner = GetOwner();
	const FGSTN_LinkFragment* Fragment = FindShownFragment(Owner);
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	if (!Fragment || !Registry)
	{
		return false;
	}
	const FGSTN_LinkState& Body = Fragment->Links.Body;
	const UGSTN_LinkTypeDefinition* Definition = Registry->GetDefinition(Body.LinkTypeIndex);
	// M7 G1: an initiator that predicts its link (bInitiatorPredicts) and is the parked role is SHOWN parked from its request on (Pending), so the
	// optimistic accept PendingFrames later does not pop; a refusal detaches it again. The simulation parks it from the accept only
	const bool bShownPending = Definition && Definition->bInitiatorPredicts && Body.Phase == EGSTN_LinkPhase::Pending
		&& Body.HasFlag(GSTNLinkFlags::Predicted) && Definition->InitiatorRole.bParked;
	const uint8 ShownRole = bShownPending ? 1 : Body.Role;
	const EGSTN_LinkPhase ShownPhase = bShownPending ? EGSTN_LinkPhase::Active : Body.Phase;
	if (Definition && (ShownPhase == EGSTN_LinkPhase::Active || ShownPhase == EGSTN_LinkPhase::Exiting))
	{
		// G10: the shown role of any role (the carrier's too), for its anim layer; and the type's slide time, kept after the link ends for the detach
		OutRoleTag = Definition->GetRoleDef(ShownRole).RoleTag;
		ShownTypeBlendSeconds = Definition->PresentationBlendSeconds;
	}
	if (!(FGSTN_LinkSnapshotContributor::ComputeBits(Definition, ShownRole, ShownPhase) & GSTNLinkSnapshotBits::Parked) || Body.PartnerId.IsNull())
	{
		return false;
	}
	const UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	AActor* PartnerActor = Combat ? Combat->FindOwnerActor(Body.PartnerId.Id) : nullptr;
	if (!PartnerActor)
	{
		return false;
	}
	const FGSTN_LinkRoleDef& Role = Definition->GetRoleDef(ShownRole);
	PartnerSocket = Role.SocketOnPartner;
	// M7 G2: the parked role's own turn (LocalYawQ), as the snapshot substitution composes it, so the mesh faces where its hurtboxes do
	RoleOffset = UGSTN_LinkMath::ComposeLocalYaw(Body.ResolveParkedOffset(Role.SocketOffset), ShownRole == Body.Role ? Body.LocalYawQ : static_cast<int16>(0));

	const USkeletalMeshComponent* PartnerMesh = PartnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (bUseLiveSocket && PartnerMesh && !PartnerSocket.IsNone() && PartnerMesh->DoesSocketExist(PartnerSocket))
	{
		// The animated hand, this machine's pose: presentation only, never what the hurtboxes use
		LinkedTransform = RoleOffset * PartnerMesh->GetSocketTransform(PartnerSocket);
	}
	else
	{
		const FGSTN_LinkBodyData* PartnerBody = FGSTN_LinkAttach::Find(PartnerActor);
		const FTransform Socket = PartnerBody ? PartnerBody->GetSocket(PartnerSocket) : FTransform::Identity;
		FTransform PartnerTransform = PartnerActor->GetActorTransform();
		PartnerTransform.SetScale3D(FVector::OneVector);
		LinkedTransform = RoleOffset * Socket * PartnerTransform;
	}

	if (Partner.Get() != PartnerActor)
	{
		DetachMesh();
		Partner = PartnerActor;
	}
	return true;
}

void UGSTN_LinkPresentationComponent::AttachMesh(AActor* NewPartner)
{
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	USkeletalMeshComponent* PartnerMesh = NewPartner ? NewPartner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || !PartnerMesh)
	{
		return;
	}
	if (!bMeshAttached)
	{
		MeshOriginalParent = Mesh->GetAttachParent();
		MeshOriginalSocket = Mesh->GetAttachSocketName();
		MeshOriginalRelative = Mesh->GetRelativeTransform();
		// Mover's own offset of its visual component, not the relative transform of this frame (its smoothing may have moved the mesh)
		if (const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>(); Mover && Mover->GetPrimaryVisualComponent() == Mesh)
		{
			MeshOriginalRelative = Mover->GetBaseVisualComponentTransform();
		}
		bMeshAttached = true;
	}
	// The parked transform places the owner's ROOT on the socket; the mesh keeps its own offset from the root. Asserted on every tick, not
	// once: Mover writes its primary visual component every frame before the actors tick (FinalizeFrame resets the relative transform to its
	// base, dropping the role offset: the fixed second step of S2 finding A; FinalizeSmoothingFrame puts it back on its own capsule in world
	// space: the parked entity's mesh that never rode on its own client, S2 finding B). This tick runs after both (TG_PostPhysics)
	const FName Socket = PartnerMesh->DoesSocketExist(PartnerSocket) ? PartnerSocket : NAME_None;
	if (Mesh->GetAttachParent() != PartnerMesh || Mesh->GetAttachSocketName() != Socket)
	{
		Mesh->AttachToComponent(PartnerMesh, FAttachmentTransformRules::KeepRelativeTransform, Socket);
	}
	const FTransform Relative = MeshOriginalRelative * RoleOffset;
	if (!Mesh->GetRelativeTransform().Equals(Relative))
	{
		Mesh->SetRelativeTransform(Relative);
	}
}

void UGSTN_LinkPresentationComponent::DetachMesh()
{
	if (!bMeshAttached)
	{
		return;
	}
	AActor* Owner = GetOwner();
	USkeletalMeshComponent* Mesh = Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (Mesh)
	{
		USceneComponent* Parent = MeshOriginalParent.IsValid() ? MeshOriginalParent.Get() : (Owner ? Owner->GetRootComponent() : nullptr);
		Mesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, MeshOriginalSocket);
		Mesh->SetRelativeTransform(MeshOriginalRelative);
	}
	bMeshAttached = false;
}

void UGSTN_LinkPresentationComponent::SetCollisionIgnoredPartner(AActor* NewPartner)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* OwnRoot = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	AActor* Previous = IgnoredPartner.Get();
	if (!OwnRoot || Previous == NewPartner)
	{
		return;
	}
	if (Previous)
	{
		OwnRoot->IgnoreActorWhenMoving(Previous, false);
	}
	if (NewPartner && NewPartner != Owner)
	{
		OwnRoot->IgnoreActorWhenMoving(NewPartner, true);
		IgnoredPartner = NewPartner;
	}
	else
	{
		IgnoredPartner = nullptr;
	}
}

void UGSTN_LinkPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FGameplayTag RoleTag;
	const bool bWasAttached = bMeshAttached;
	bParkedShown = UpdateLinkedTransform(RoleTag);
	if (bParkedShown)
	{
		if (bAttachMesh)
		{
			AttachMesh(Partner.Get());
		}
	}
	else
	{
		DetachMesh();
		Partner = nullptr;
	}
	// G10: an attach or a detach this frame slides the mesh from where it was shown at the end of the last frame
	if (bWasAttached != bMeshAttached && bHasLastShown)
	{
		StartMeshBlend(LastShownWorld);
	}
	ApplyMeshBlend(DeltaTime);
	if (const USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner()))
	{
		LastShownWorld = Mesh->GetComponentTransform();
		bHasLastShown = true;
	}
	UpdateShownRole(RoleTag);
	FGSTN_LinkDebugDraw::DrawEntity(GetOwner(), *this);
}

float UGSTN_LinkPresentationComponent::GetPresentationBlendAlpha() const
{
	return bBlending && BlendSeconds > 0.f ? FMath::Clamp(BlendElapsed / BlendSeconds, 0.f, 1.f) : 1.f;
}

void UGSTN_LinkPresentationComponent::StartMeshBlend(const FTransform& FromWorld)
{
	BlendSeconds = ShownTypeBlendSeconds;
	BlendElapsed = 0.f;
	BlendFromWorld = FromWorld;
	bBlending = BlendSeconds > 0.f;
}

void UGSTN_LinkPresentationComponent::ApplyMeshBlend(float DeltaTime)
{
	if (!bBlending)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner());
	const USceneComponent* Parent = Mesh ? Mesh->GetAttachParent() : nullptr;
	if (!Parent)
	{
		bBlending = false;
		return;
	}
	// The settled transform is recomputed from the attachment, never read back from the mesh: Mover may or may not have rewritten the mesh
	// this frame, and the last frame's blended write must not feed the next one
	const FTransform TargetRelative = bMeshAttached ? MeshOriginalRelative * RoleOffset : MeshOriginalRelative;
	BlendElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(BlendElapsed / BlendSeconds, 0.f, 1.f);
	if (Alpha >= 1.f)
	{
		Mesh->SetRelativeTransform(TargetRelative);
		bBlending = false;
		return;
	}
	const FTransform Settled = TargetRelative * Parent->GetSocketTransform(Mesh->GetAttachSocketName());
	FTransform Shown;
	Shown.Blend(BlendFromWorld, Settled, FMath::SmoothStep(0.f, 1.f, Alpha));
	Mesh->SetWorldTransform(Shown);
}

void UGSTN_LinkPresentationComponent::UpdateShownRole(const FGameplayTag& NewRoleTag)
{
	if (NewRoleTag == ShownRoleTag)
	{
		return;
	}
	const FGameplayTag OldRoleTag = ShownRoleTag;
	ShownRoleTag = NewRoleTag;
	UnlinkRoleLayer();
	if (bLinkRoleAnimLayers && NewRoleTag.IsValid())
	{
		LinkRoleLayer(NewRoleTag);
	}
	OnShownRoleChanged.Broadcast(OldRoleTag, NewRoleTag);
}

void UGSTN_LinkPresentationComponent::LinkRoleLayer(const FGameplayTag& RoleTag)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const UGSTN_LoadoutComponent* Loadout = GSTNLinkPresentationPrivate::FindLoadout(GetOwner());
	if (!Loadout)
	{
		return;
	}
	const FGSTN_ResolvedLoadout& Resolved = Loadout->GetResolvedLoadout();
	const UGSTN_WeaponDefinition* Weapon = nullptr;
	for (const UGSTN_WeaponDefinition* Candidate : Resolved.Weapons)
	{
		if (Candidate)
		{
			// The first resolved weapon: the loadout's main one
			Weapon = Candidate;
			break;
		}
	}
	WantedRoleLayer = UGSTN_LinkAnimSelection::SelectRoleAnimLayer(Resolved.Kit, Weapon, RoleTag);
	if (WantedRoleLayer.IsNull())
	{
		return;
	}
	if (WantedRoleLayer.Get())
	{
		HandleRoleLayerLoaded(RoleTag);
		return;
	}
	// Loaded off the critical path (GatherRoleAnimLayers is what a client preloads, so this is rare); linked only if the role is still shown
	RoleLayerHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(WantedRoleLayer.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &UGSTN_LinkPresentationComponent::HandleRoleLayerLoaded, RoleTag));
}

void UGSTN_LinkPresentationComponent::HandleRoleLayerLoaded(FGameplayTag ForRoleTag)
{
	RoleLayerHandle.Reset();
	UClass* LayerClass = WantedRoleLayer.Get();
	USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner());
	if (ForRoleTag != ShownRoleTag || !LayerClass || !Mesh)
	{
		return;
	}
	Mesh->LinkAnimClassLayers(TSubclassOf<UAnimInstance>(LayerClass));
	LinkedRoleLayer = LayerClass;
}

void UGSTN_LinkPresentationComponent::UnlinkRoleLayer()
{
	if (RoleLayerHandle.IsValid())
	{
		RoleLayerHandle->CancelHandle();
		RoleLayerHandle.Reset();
	}
	if (LinkedRoleLayer)
	{
		if (USkeletalMeshComponent* Mesh = GSTNLinkPresentationPrivate::FindMesh(GetOwner()))
		{
			Mesh->UnlinkAnimClassLayers(TSubclassOf<UAnimInstance>(LinkedRoleLayer.Get()));
		}
		LinkedRoleLayer = nullptr;
	}
	WantedRoleLayer.Reset();
}

// ---- FP G1: predicted partner presentation ----------------------------------------------------------------------------------------------------

bool UGSTN_LinkPresentationComponent::GetPredictedPartnerMove(int32& OutMoveId, int32& OutTimelineFrame) const
{
	if (!bPredictedActive)
	{
		return false;
	}
	OutMoveId = PredictedMoveId;
	OutTimelineFrame = PredictedTimelineFrame;
	return true;
}

void UGSTN_LinkPresentationComponent::TickPredictedPartner(float DeltaTime)
{
	using namespace GSTNLinkPresentationPrivate;

	// Zero work for everyone who is not the initiator of a predicted partner move: the cvar, the controller and the fragment's record are the whole cost.
	// Tick order this relies on: the fork's montage finalize (UNetMontageSimulator::FinalizeMontageFrame, from the world manager's
	// BeginNewSimulationFrame on OnWorldPreActorTick) has already run, and so has this machine's combat simulation, so the fragment and the frame
	// read here are this frame's. The partner's mesh animates later in the same TG_PrePhysics group, ordered after this tick by a prerequisite
	// (set while a window leads its montage), so the instance position written here is the one the mesh evaluates; the actor components
	// of TickComponent (TG_PostPhysics) would write it a frame too late, after the finalize had pulled the instance back to the synced time
	if (CVarPredictPartnerPresentation.GetValueOnGameThread() == 0)
	{
		if (PredictPhase != EPredictPhase::Idle)
		{
			FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Dropped, 0);
		}
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled() || Pawn->HasAuthority())
	{
		// Not the initiator's own predicting machine (the partner's machine, a listen host, a spectator): the partner's real part is already shown there
		if (PredictPhase != EPredictPhase::Idle)
		{
			FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Dropped, 0);
		}
		return;
	}
	UGSTN_CombatSimComponent* Combat = CachedCombat.Get();
	if (!Combat)
	{
		Combat = GetOwner()->FindComponentByClass<UGSTN_CombatSimComponent>();
		CachedCombat = Combat;
		if (!Combat)
		{
			return;
		}
	}
	const FGSTN_CombatSyncState* Sync = Combat->GetSyncState();
	if (!Sync)
	{
		return;
	}
	const FGSTN_LinkFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Sync->Fragments);
	const int32 LocalFrame = Sync->Core.Input.History.GetLatestFrame();
	const FGSTN_LinkActions::FActive* Record = Fragment && Fragment->Actions.Active.Id != 0 && Fragment->Actions.Active.Side == GSTNLinkActionSide::Initiator
		? &Fragment->Actions.Active : nullptr;

	if (PredictPhase == EPredictPhase::Suppressing && Record && (Record->Id != Window.MoveId || Record->StartFrame != Window.InitiatorFrame))
	{
		// The initiator started another move: the suppression of the last one gives way to it
		ResetPredicted();
	}
	switch (PredictPhase)
	{
	case EPredictPhase::Idle:
		if (Record && !(Record->Id == RejectedMoveId && Record->StartFrame == RejectedFrame) && BeginPredictedWindow(*Fragment, *Combat))
		{
			TickPlaying(Fragment, LocalFrame);
		}
		break;
	case EPredictPhase::Playing:
		TickPlaying(Fragment, LocalFrame);
		break;
	case EPredictPhase::Suppressing:
		TickSuppressing(LocalFrame);
		break;
	}
}

bool UGSTN_LinkPresentationComponent::BeginPredictedWindow(const FGSTN_LinkFragment& Fragment, UGSTN_CombatSimComponent& Combat)
{
	using namespace GSTNLinkPresentationPrivate;

	RejectedMoveId = Fragment.Actions.Active.Id;
	RejectedFrame = Fragment.Actions.Active.StartFrame;
	FGSTN_LinkPartnerWindow Built;
	if (!FGSTN_LinkPartnerWindow::BuildFromInitiator(Fragment, UGSTN_LinkRegistry::Get(), &ResolvePartnerTimeline, Built))
	{
		return false;
	}
	const UGSTN_CombatWorldSubsystem* World = UGSTN_CombatWorldSubsystem::Get(GetWorld());
	AActor* PartnerActor = World ? World->FindOwnerActor(Fragment.Links.Body.PartnerId.Id) : nullptr;
	UGSTN_LinkPresentationComponent* PartnerComp = PartnerActor && PartnerActor != GetOwner() ? FindOrAdd(PartnerActor) : nullptr;
	if (!PartnerComp)
	{
		// The partner is not on this machine (not relevant here): nothing to show it on
		return false;
	}
	RejectedMoveId = 0;
	RejectedFrame = 0;

	Window = Built;
	PredictPhase = EPredictPhase::Playing;
	PredictedPartner = PartnerActor;
	PredictedPartnerComp = PartnerComp;
	PredictedPartnerMesh = FindMesh(PartnerActor);
	bPredictStarted = false;
	bAbortSeen = false;
	bPartnerSeenPlaying = false;
	bMontageStarted = false;
	bMontageStopped = false;
	PredictMontage = nullptr;
	PredictMontagePath.Reset();
	PredictSection = NAME_None;
	PredictPlayRate = 1.f;
	const UNetworkPredictionWorldManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
	PredictStepSeconds = Manager ? Manager->GetFixedTickState().FixedStepMS * 0.001f : 0.016f;

	// The partner's montage: soft, requested now and used from the first tick it is in memory (never a synchronous load on the game thread)
	if (const FGSTN_CombatTimelineData* Data = ResolvePartnerTimeline(Window.PartnerAbility); Data && !Data->Montage.IsNull())
	{
		PredictMontagePath = Data->Montage;
		PredictSection = Data->MontageSection;
		PredictPlayRate = Data->MontagePlayRate;
		if (UAnimMontage* Loaded = PredictMontagePath.Get())
		{
			PredictMontage = Loaded;
		}
		else
		{
			PredictMontageHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(PredictMontagePath.ToSoftObjectPath());
		}
	}
	if (USkeletalMeshComponent* Mesh = PredictedPartnerMesh.Get())
	{
		// The mesh evaluates after this tick for as long as the window lasts (removed in ResetPredicted)
		Mesh->PrimaryComponentTick.AddPrerequisite(this, PredictTick);
	}
	return true;
}

void UGSTN_LinkPresentationComponent::TickPlaying(const FGSTN_LinkFragment* Fragment, int32 LocalFrame)
{
	if (!PredictedPartnerComp.IsValid() || !PredictedPartner.IsValid())
	{
		// The partner left this machine
		FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Dropped, LocalFrame);
		return;
	}
	const FGSTN_LinkActions::FActive* Active = Fragment ? &Fragment->Actions.Active : nullptr;
	const bool bRecordLive = Active && Active->Id == Window.MoveId && Active->StartFrame == Window.InitiatorFrame && Active->Side == GSTNLinkActionSide::Initiator
		&& Fragment->Links.Body.Serial == Window.Serial;
	if (Fragment && Fragment->Links.Body.HasFlag(GSTNLinkFlags::PendingBreak) && Fragment->Links.Body.GetBreakReason() == EGSTN_LinkBreakReason::Forced)
	{
		// An abort (a side knocked out, the move too long) leaves the pending Forced break, whether or not the record is still there
		bAbortSeen = true;
	}
	if (!bRecordLive)
	{
		if (FGSTN_LinkPartnerPlayback::IsDroppedOnRecordLoss(Window, LocalFrame, bAbortSeen))
		{
			FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Dropped, LocalFrame);
			return;
		}
		if (!Window.bLengthKnown)
		{
			// No authored length: the window lasted as long as the initiator's record
			FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Ended, LocalFrame);
			return;
		}
	}
	if (Window.bLengthKnown && LocalFrame >= Window.PartnerEndFrame)
	{
		FinishPredicted(EGSTN_LinkPredictedPartnerEvent::Ended, LocalFrame);
		return;
	}
	const int32 TimelineFrame = Window.TimelineFrameAt(LocalFrame);
	if (TimelineFrame < 0)
	{
		// Before the partner's start (its offset): known, not shown yet. Also where a rollback to before the start lands
		PublishPredicted(false, Window.MoveId, 0);
		return;
	}
	if (!bPredictStarted)
	{
		bPredictStarted = true;
		PublishPredicted(true, Window.MoveId, TimelineFrame);
		NotifyPredicted(EGSTN_LinkPredictedPartnerEvent::Started, Window.MoveId);
	}
	PublishPredicted(true, Window.MoveId, TimelineFrame);
	DrivePartnerMontage(TimelineFrame);
}

void UGSTN_LinkPresentationComponent::TickSuppressing(int32 LocalFrame)
{
	AActor* PartnerActor = PredictedPartner.Get();
	if (!PartnerActor)
	{
		ResetPredicted();
		return;
	}
	if (LocalFrame < PredictEndedFrame)
	{
		// A rollback went back past the end: count from there
		PredictEndedFrame = LocalFrame;
	}
	const FGSTN_LinkFragment* PartnerFragment = FindShownFragment(PartnerActor);
	const bool bPlayingNow = PartnerFragment && PartnerFragment->Actions.Active.Id == Window.MoveId && PartnerFragment->Actions.Active.Side == GSTNLinkActionSide::Partner;
	bPartnerSeenPlaying |= bPlayingNow;
	if (FGSTN_LinkPartnerPlayback::IsSuppressionOver(LocalFrame - PredictEndedFrame, bPartnerSeenPlaying, bPlayingNow))
	{
		ResetPredicted();
		return;
	}
	// The late synced copy would start the montage again from its own, earlier time: keep it off until the partner's shown state is past the move
	DrivePartnerMontage(INT32_MAX);
}

void UGSTN_LinkPresentationComponent::DrivePartnerMontage(int32 TimelineFrame)
{
	using namespace GSTNLinkPresentationPrivate;

	if (bMontageStopped && !PredictMontage)
	{
		return;
	}
	if (!PredictMontage)
	{
		PredictMontage = PredictMontagePath.IsNull() ? nullptr : PredictMontagePath.Get();
		if (!PredictMontage)
		{
			// Still loading (or the timeline has none)
			return;
		}
		PredictMontageHandle.Reset();
		PredictSectionStart = 0.f;
		PredictMontageEnd = PredictMontage->GetPlayLength();
		const int32 SectionIndex = PredictSection.IsNone() ? INDEX_NONE : PredictMontage->GetSectionIndex(PredictSection);
		if (SectionIndex != INDEX_NONE)
		{
			PredictMontage->GetSectionStartAndEndTime(SectionIndex, PredictSectionStart, PredictMontageEnd);
		}
	}
	UAnimInstance* AnimInstance = GetPartnerAnimInstance();
	if (!AnimInstance)
	{
		return;
	}
	FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(PredictMontage);
	const bool bInstanceLive = Instance && !Instance->IsStopped();
	if (bMontageStopped)
	{
		if (bInstanceLive)
		{
			// The synced copy (late by the proxy's lag) started it again: off at once, before the mesh evaluates, so no frame of it is shown
			AnimInstance->Montage_Stop(0.f, PredictMontage);
		}
		return;
	}
	const float Predicted = FGSTN_LinkPartnerPlayback::PredictedPosition(TimelineFrame, PredictStepSeconds, PredictPlayRate, PredictSectionStart);
	if (FGSTN_LinkPartnerPlayback::HasReachedEnd(Predicted, PredictMontageEnd))
	{
		// The prediction is past the montage: blend it out and keep it off (the synced copy is behind and would play the tail again)
		if (bInstanceLive)
		{
			AnimInstance->Montage_Stop(PredictMontage->GetDefaultBlendOutTime(), PredictMontage);
		}
		bMontageStopped = true;
		return;
	}
	if (!bMontageStarted)
	{
		if (!bInstanceLive && AnimInstance->Montage_Play(PredictMontage, PredictPlayRate, EMontagePlayReturnType::MontageLength, Predicted) <= 0.f)
		{
			// The mesh cannot play it (another skeleton): no presentation montage, the published reaction stays
			bMontageStopped = true;
			return;
		}
		bMontageStarted = true;
		return;
	}
	if (bInstanceLive && FGSTN_LinkPartnerPlayback::ShouldWriteLead(Instance->GetPosition(), Predicted, LeadEpsilonSeconds))
	{
		// The finalize of this frame set the instance to the synced time (behind the prediction): the later of the two is shown
		Instance->SetPosition(FGSTN_LinkPartnerPlayback::LeadPosition(Instance->GetPosition(), Predicted));
	}
}

void UGSTN_LinkPresentationComponent::StopPartnerMontage(float BlendOut)
{
	if (!PredictMontage)
	{
		return;
	}
	if (UAnimInstance* AnimInstance = GetPartnerAnimInstance())
	{
		FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(PredictMontage);
		if (Instance && !Instance->IsStopped())
		{
			AnimInstance->Montage_Stop(BlendOut, PredictMontage);
		}
	}
}

UAnimInstance* UGSTN_LinkPresentationComponent::GetPartnerAnimInstance() const
{
	const USkeletalMeshComponent* Mesh = PredictedPartnerMesh.Get();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

void UGSTN_LinkPresentationComponent::PublishPredicted(bool bActive, int32 MoveId, int32 TimelineFrame)
{
	if (UGSTN_LinkPresentationComponent* PartnerComp = PredictedPartnerComp.Get())
	{
		PartnerComp->bPredictedActive = bActive;
		PartnerComp->PredictedMoveId = bActive ? MoveId : 0;
		PartnerComp->PredictedTimelineFrame = bActive ? TimelineFrame : 0;
	}
}

void UGSTN_LinkPresentationComponent::NotifyPredicted(EGSTN_LinkPredictedPartnerEvent Event, int32 MoveId)
{
	// On the partner's component (what its anim blueprint reads) and on the initiator's own (what the initiator's machine and the harness count)
	UGSTN_LinkPresentationComponent* PartnerComp = PredictedPartnerComp.Get();
	for (UGSTN_LinkPresentationComponent* Component : { PartnerComp, this })
	{
		if (Component && (Component != this || PartnerComp != this))
		{
			++Component->PredictedEventCounts[static_cast<int32>(Event)];
			Component->OnPredictedPartnerMove.Broadcast(Event, MoveId);
		}
	}
}

void UGSTN_LinkPresentationComponent::FinishPredicted(EGSTN_LinkPredictedPartnerEvent Event, int32 LocalFrame)
{
	const int32 MoveId = Window.MoveId;
	if (Event == EGSTN_LinkPredictedPartnerEvent::Dropped)
	{
		// A refusal, an abort or a rollback: the snap. The montage goes at once, the reaction is retracted, and nothing is suppressed (whatever the
		// authority shows is shown)
		StopPartnerMontage(0.f);
		PublishPredicted(false, 0, 0);
		NotifyPredicted(Event, MoveId);
		ResetPredicted();
		return;
	}
	if (!bPredictStarted)
	{
		bPredictStarted = true;
		NotifyPredicted(EGSTN_LinkPredictedPartnerEvent::Started, MoveId);
	}
	PublishPredicted(false, 0, 0);
	NotifyPredicted(Event, MoveId);
	if (bMontageStarted && PredictMontage)
	{
		// Blend the montage out (unless it already ended) and keep the synced copy off until the partner's shown state is past the move
		if (!bMontageStopped)
		{
			StopPartnerMontage(PredictMontage->GetDefaultBlendOutTime());
			bMontageStopped = true;
		}
		PredictPhase = EPredictPhase::Suppressing;
		PredictEndedFrame = LocalFrame;
		return;
	}
	ResetPredicted();
}

void UGSTN_LinkPresentationComponent::RemovePartnerTickPrerequisite()
{
	if (USkeletalMeshComponent* Mesh = PredictedPartnerMesh.Get())
	{
		Mesh->PrimaryComponentTick.RemovePrerequisite(this, PredictTick);
	}
}

void UGSTN_LinkPresentationComponent::ResetPredicted()
{
	if (PredictPhase == EPredictPhase::Idle && !PredictedPartnerComp.IsValid() && !PredictMontageHandle.IsValid())
	{
		return;
	}
	if (PredictPhase != EPredictPhase::Suppressing)
	{
		PublishPredicted(false, 0, 0);
	}
	RemovePartnerTickPrerequisite();
	if (PredictMontageHandle.IsValid())
	{
		PredictMontageHandle->CancelHandle();
		PredictMontageHandle.Reset();
	}
	PredictPhase = EPredictPhase::Idle;
	Window = FGSTN_LinkPartnerWindow();
	PredictedPartner = nullptr;
	PredictedPartnerComp = nullptr;
	PredictedPartnerMesh = nullptr;
	PredictMontage = nullptr;
	PredictMontagePath.Reset();
	bPredictStarted = false;
	bMontageStarted = false;
	bMontageStopped = false;
}

void UGSTN_LinkPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetPredicted();
	bPredictedActive = false;
	DetachMesh();
	UnlinkRoleLayer();
	SetCollisionIgnoredPartner(nullptr);
	Super::EndPlay(EndPlayReason);
}
