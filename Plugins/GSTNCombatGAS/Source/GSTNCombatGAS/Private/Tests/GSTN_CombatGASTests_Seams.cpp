// Automation tests of the M11 seams of the ability layer (combat-loadout-mods 11): the order of the loadout extension registry and
// the attachment providers the skin component composes. Transient world, no PIE.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "Cosmetics/GSTN_SkinComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Tests/GSTN_GASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags SeamsTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Asks for one engine cube per call on a fixed socket; counts its calls */
	class FTestCubeProvider : public IGSTN_AttachmentProvider
	{
	public:
		FName Socket;
		mutable int32 Calls = 0;
		mutable bool bSawLoadout = true;

		virtual void GatherAttachments(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests) const override
		{
			++Calls;
			bSawLoadout = Context.Resolved != nullptr;
			FGSTN_AttachmentRequest& Request = OutRequests.AddDefaulted_GetRef();
			Request.Socket = Socket;
			Request.Mesh = TSoftObjectPtr<UObject>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
			Request.Offset = FTransform(FVector(0.f, -10.f, 0.f));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LoadoutExtensionPriorityTest, "GSTNCombat.GAS.Loadout.ExtensionPriority", SeamsTestFlags)
bool FGSTN_LoadoutExtensionPriorityTest::RunTest(const FString& Parameters)
{
	// Priority first, then the id: "Z.Early" (priority -10) runs before "A.Late" (priority 5) and both around the default ones
	const FName Late(TEXT("GSTNTest.A.Late"));
	const FName Early(TEXT("GSTNTest.Z.Early"));
	const FName MidB(TEXT("GSTNTest.M.B"));
	const FName MidA(TEXT("GSTNTest.M.A"));
	TArray<FName> Order;
	auto Make = [&Order](FName Id, int32 Priority)
	{
		FGSTN_LoadoutExtension Extension;
		Extension.Priority = Priority;
		Extension.ContributeCompileInput = [&Order, Id](const FGSTN_LoadoutExtensionContext&, FGSTN_CompileInput&) { Order.Add(Id); };
		FGSTN_LoadoutExtensionRegistry::Register(Id, MoveTemp(Extension));
	};
	Make(Late, 5);
	Make(MidB, 0);
	Make(Early, -10);
	Make(MidA, 0);

	TArray<FName> Ids;
	FGSTN_LoadoutExtensionRegistry::GetOrderedIds(Ids);
	Ids.RemoveAll([&](const FName& Id) { return Id != Late && Id != Early && Id != MidA && Id != MidB; });
	TestTrue(TEXT("priority, then id"), Ids == TArray<FName>({ Early, MidA, MidB, Late }));

	// The callbacks run in that order too (only the test ones are recorded)
	FGSTN_LoadoutExtensionContext Context;
	{
		// Other plugins' extensions read the input; give them a real one
		FGSTN_CompileInput Input;
		FGSTN_LoadoutExtensionRegistry::RunContributeCompileInput(Context, Input);
	}
	TestTrue(TEXT("callbacks follow the same order"), Order == TArray<FName>({ Early, MidA, MidB, Late }));

	for (const FName& Id : { Late, Early, MidA, MidB })
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(Id);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SkinAttachmentProvidersTest, "GSTNCombat.GAS.Skin.AttachmentProviders", SeamsTestFlags)
bool FGSTN_SkinAttachmentProvidersTest::RunTest(const FString& Parameters)
{
	GSTNCombatGASTest::FRig Rig;
	UWorld& World = *Rig.World;
	AActor* Pawn = World.SpawnActor<AActor>();
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Pawn);
	Pawn->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	UGSTN_SkinComponent* Skin = NewObject<UGSTN_SkinComponent>(Pawn);
	Skin->bAutoBind = false;
	Skin->RegisterComponent();

	const TSharedRef<FTestCubeProvider> Back = MakeShared<FTestCubeProvider>();
	Back->Socket = TEXT("Implant_Back");
	const TSharedRef<FTestCubeProvider> Pack = MakeShared<FTestCubeProvider>();
	Pack->Socket = TEXT("Connector_Backpack");
	const FName BackId(TEXT("GSTNTest.Implant"));
	const FName PackId(TEXT("GSTNTest.Backpack"));
	// The backpack id sorts first by name, but its priority puts it after the implant it sits on
	FGSTN_AttachmentProviderRegistry::Register(BackId, Back, 0);
	FGSTN_AttachmentProviderRegistry::Register(PackId, Pack, 10);

	Skin->RefreshAttachments();
	const TArray<TObjectPtr<USceneComponent>>& Made = Skin->GetAttachmentComponents();
	TestEqual(TEXT("one component per request"), Made.Num(), 2);
	if (Made.Num() == 2)
	{
		const UStaticMeshComponent* First = Cast<UStaticMeshComponent>(Made[0]);
		const UStaticMeshComponent* Second = Cast<UStaticMeshComponent>(Made[1]);
		TestTrue(TEXT("static meshes"), First && Second && First->GetStaticMesh() != nullptr);
		TestTrue(TEXT("priority order: implant, then backpack"), First && Second && First->GetAttachSocketName() == Back->Socket
			&& Second->GetAttachSocketName() == Pack->Socket);
		TestTrue(TEXT("attached to the pawn's mesh"), First && First->GetAttachParent() == Mesh);
		TestTrue(TEXT("with the request offset"), First && First->GetRelativeLocation().Equals(FVector(0.f, -10.f, 0.f)));
		TestTrue(TEXT("never collides"), First && First->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestTrue(TEXT("registered"), First && First->IsRegistered());
	}
	TestFalse(TEXT("no loadout bound: the context says so"), Back->bSawLoadout);

	// A refresh replaces, never stacks; a removed provider's mesh goes away
	TWeakObjectPtr<USceneComponent> Old = Made.Num() > 0 ? Made[0].Get() : nullptr;
	FGSTN_AttachmentProviderRegistry::Unregister(PackId);
	Skin->RefreshAttachments();
	TestEqual(TEXT("one provider left, one component"), Skin->GetAttachmentComponents().Num(), 1);
	TestTrue(TEXT("the old component was destroyed"), !Old.IsValid() || Old->IsBeingDestroyed() || !Old->IsRegistered());
	TestEqual(TEXT("each refresh asks again"), Back->Calls, 2);

	FGSTN_AttachmentProviderRegistry::Unregister(BackId);
	Skin->RefreshAttachments();
	TestEqual(TEXT("no provider, no attachment"), Skin->GetAttachmentComponents().Num(), 0);
	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
