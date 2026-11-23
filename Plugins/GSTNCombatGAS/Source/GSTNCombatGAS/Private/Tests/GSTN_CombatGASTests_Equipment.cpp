// Automation tests of the equipment look (combat-loadout-mods 9, M11 L6): the kit and weapon visuals, the pure composition (slot
// replacement, recolour, a piece on another piece's socket) and what the skin component builds from it. Transient world, no PIE.

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Cosmetics/GSTN_SkinComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tests/GSTN_GASTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNEquipmentTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* SpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	FGSTN_AttachmentRequest Piece(const TCHAR* Mesh, FName Socket, FName Slot = NAME_None, FName Parent = NAME_None)
	{
		FGSTN_AttachmentRequest Request;
		Request.Mesh = Mesh ? TSoftObjectPtr<UObject>(FSoftObjectPath(Mesh)) : TSoftObjectPtr<UObject>();
		Request.Socket = Socket;
		Request.VisualSlot = Slot;
		Request.ParentSlot = Parent;
		return Request;
	}

	/** Hands out a fixed list of requests (a backpack and a mod, as an optional plugin would) */
	class FListProvider : public IGSTN_AttachmentProvider
	{
	public:
		TArray<FGSTN_AttachmentRequest> Requests;
		virtual void GatherAttachments(const FGSTN_AttachmentContext& Context, TArray<FGSTN_AttachmentRequest>& OutRequests) const override
		{
			OutRequests.Append(Requests);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EquipmentComposeTest, "GSTNCombat.GAS.Equipment.Compose", GSTNEquipmentTest::Flags)
bool FGSTN_EquipmentComposeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNEquipmentTest;
	const FName Sword(TEXT("Sword"));

	TArray<FGSTN_AttachmentRequest> Requests;
	Requests.Add(Piece(CubePath, GSTNEquipment::Socket_ImplantBack, GSTNEquipment::Slot_ImplantBack));
	Requests.Add(Piece(CubePath, GSTNEquipment::Socket_WeaponHandR, Sword));
	// A backpack on the implant's connector, then one whose parent slot nobody made
	Requests.Add(Piece(CubePath, GSTNEquipment::Socket_ConnectorBackpack, TEXT("Backpack"), GSTNEquipment::Slot_ImplantBack));
	Requests.Add(Piece(CubePath, TEXT("Implant_Hand_L"), NAME_None, TEXT("Missing")));
	// A mod: a longer blade on the same socket, then a recolour of it, then a recolour of nothing
	FGSTN_AttachmentRequest Blade = Piece(SpherePath, NAME_None, Sword);
	Blade.bReplacesSlot = true;
	Blade.VectorParameters.Add(TEXT("Color"), FLinearColor::Red);
	Requests.Add(Blade);
	FGSTN_AttachmentRequest Tint = Piece(nullptr, NAME_None, Sword);
	Tint.ScalarParameters.Add(TEXT("Metal"), 0.5f);
	Tint.VectorParameters.Add(TEXT("Color"), FLinearColor::Blue);
	Requests.Add(Tint);
	Requests.Add(Piece(nullptr, NAME_None, TEXT("Nothing")));

	TArray<FGSTN_AttachmentRequest> Pieces;
	FGSTN_EquipmentVisualUtils::Compose(Requests, Pieces);
	if (!TestEqual(TEXT("implant, sword, backpack, hand piece: the replacement and the recolours build nothing"), Pieces.Num(), 4))
	{
		return false;
	}
	TestEqual(TEXT("the backpack keeps its parent slot"), Pieces[2].ParentSlot, GSTNEquipment::Slot_ImplantBack);
	TestTrue(TEXT("a parent nobody made falls back to the pawn"), Pieces[3].ParentSlot.IsNone());
	TestEqual(TEXT("the blade swapped the sword's mesh"), Pieces[1].Mesh.ToSoftObjectPath(), FSoftObjectPath(SpherePath));
	TestEqual(TEXT("and kept its socket"), Pieces[1].Socket, GSTNEquipment::Socket_WeaponHandR);
	const FLinearColor* Colour = Pieces[1].VectorParameters.Find(TEXT("Color"));
	TestTrue(TEXT("the later recolour wins"), Colour && *Colour == FLinearColor::Blue);
	TestTrue(TEXT("recolours add up"), Pieces[1].ScalarParameters.Contains(TEXT("Metal")));
	TestFalse(TEXT("the implant is not recoloured"), Pieces[0].HasMaterialParameters());

	// A replacement with nothing to replace is added
	TArray<FGSTN_AttachmentRequest> Lone;
	Lone.Add(Blade);
	FGSTN_EquipmentVisualUtils::Compose(Lone, Pieces);
	TestTrue(TEXT("a replacement alone is added"), Pieces.Num() == 1 && !Pieces[0].bReplacesSlot);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EquipmentLoadoutVisualsTest, "GSTNCombat.GAS.Equipment.LoadoutVisuals", GSTNEquipmentTest::Flags)
bool FGSTN_EquipmentLoadoutVisualsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNEquipmentTest;

	UGSTN_PrimaryKitDefinition* Kit = NewObject<UGSTN_PrimaryKitDefinition>();
	Kit->Visual.Parts.Add(Piece(CubePath, GSTNEquipment::Socket_ImplantBack));
	Kit->Visual.Parts.Add(Piece(CubePath, GSTNEquipment::Socket_ImplantFootR));
	UGSTN_WeaponDefinition* Parent = NewObject<UGSTN_WeaponDefinition>();
	Parent->Visual.Parts.Add(Piece(CubePath, GSTNEquipment::Socket_WeaponHandR, TEXT("Sword")));
	UGSTN_WeaponDefinition* Variant = NewObject<UGSTN_WeaponDefinition>();
	Variant->ParentWeapon = Parent;

	TArray<FGSTN_AttachmentRequest> Requests;
	FGSTN_EquipmentVisualUtils::GatherLoadoutVisuals(Kit, Variant, Requests);
	if (!TestEqual(TEXT("two kit pieces, then the parent weapon's sword"), Requests.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("the back implant gets its slot by itself"), Requests[0].VisualSlot, GSTNEquipment::Slot_ImplantBack);
	TestTrue(TEXT("a foot implant stays anonymous"), Requests[1].VisualSlot.IsNone());
	TestEqual(TEXT("a weapon without a look inherits its parent's"), Requests[2].VisualSlot, FName(TEXT("Sword")));

	Requests.Reset();
	FGSTN_EquipmentVisualUtils::GatherLoadoutVisuals(nullptr, nullptr, Requests);
	TestEqual(TEXT("no kit, no weapon, no piece"), Requests.Num(), 0);

	// The engine cube has no connector socket: the back implant is reported; a part with nothing to do is too
	FGSTN_EquipmentVisual Visual = Kit->Visual;
	Visual.Parts.Add(Piece(nullptr, NAME_None, TEXT("Sword")));
	TArray<FText> Warnings;
	FGSTN_EquipmentVisualUtils::Validate(Visual, TEXT("Kit"), Warnings);
	TestEqual(TEXT("connector missing and an empty part"), Warnings.Num(), 2);
	TestTrue(TEXT("the connector warning names the socket"), Warnings.Num() > 0 && Warnings[0].ToString().Contains(GSTNEquipment::Socket_ConnectorBackpack.ToString()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EquipmentPiecesTest, "GSTNCombat.GAS.Equipment.SkinPieces", GSTNEquipmentTest::Flags)
bool FGSTN_EquipmentPiecesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNEquipmentTest;
	// In memory before the refresh, so the pieces are built at once (the async path keeps the old look and is checked in PIE)
	TSoftObjectPtr<UObject>(FSoftObjectPath(CubePath)).LoadSynchronous();
	TSoftObjectPtr<UObject>(FSoftObjectPath(SpherePath)).LoadSynchronous();

	GSTNCombatGASTest::FRig Rig;
	UWorld& World = *Rig.World;
	AActor* Pawn = World.SpawnActor<AActor>();
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>(Pawn);
	Pawn->SetRootComponent(Mesh);
	Mesh->RegisterComponent();
	UGSTN_SkinComponent* Skin = NewObject<UGSTN_SkinComponent>(Pawn);
	Skin->bAutoBind = false;
	Skin->RegisterComponent();

	const FName Sword(TEXT("Sword"));
	const TSharedRef<FListProvider> Provider = MakeShared<FListProvider>();
	Provider->Requests.Add(Piece(CubePath, GSTNEquipment::Socket_ImplantBack, GSTNEquipment::Slot_ImplantBack));
	Provider->Requests.Add(Piece(CubePath, GSTNEquipment::Socket_ConnectorBackpack, TEXT("Backpack"), GSTNEquipment::Slot_ImplantBack));
	FGSTN_AttachmentRequest Hidden = Piece(SpherePath, GSTNEquipment::Socket_WeaponHandR, Sword);
	Hidden.bShownByDefault = false;
	Hidden.VectorParameters.Add(TEXT("Color"), FLinearColor::Green);
	Provider->Requests.Add(Hidden);
	const FName Id(TEXT("GSTNTest.EquipmentPieces"));
	FGSTN_AttachmentProviderRegistry::Register(Id, Provider, 0);

	Skin->RefreshAttachments();
	TestFalse(TEXT("nothing to wait for"), Skin->IsAttachmentLoadPending());
	USceneComponent* Implant = Skin->FindVisualSlotComponent(GSTNEquipment::Slot_ImplantBack);
	USceneComponent* Pack = Skin->FindVisualSlotComponent(TEXT("Backpack"));
	USceneComponent* Blade = Skin->FindVisualSlotComponent(Sword);
	TestTrue(TEXT("three pieces"), Implant && Pack && Blade && Skin->GetAttachmentComponents().Num() == 3);
	if (Implant && Pack && Blade)
	{
		TestTrue(TEXT("the implant is on the pawn"), Implant->GetAttachParent() == Mesh);
		TestTrue(TEXT("the backpack is on the implant"), Pack->GetAttachParent() == Implant);
		TestEqual(TEXT("on its connector socket"), Pack->GetAttachSocketName(), GSTNEquipment::Socket_ConnectorBackpack);
		TestFalse(TEXT("the sword is hidden by default"), Blade->IsVisible());
		const UStaticMeshComponent* BladeMesh = Cast<UStaticMeshComponent>(Blade);
		TestTrue(TEXT("recoloured through a dynamic instance"), BladeMesh && Cast<UMaterialInstanceDynamic>(BladeMesh->GetMaterial(0)) != nullptr);

		Skin->SetVisualSlotShown(Sword, true);
		TestTrue(TEXT("a move shows the sword"), Blade->IsVisible());
		Skin->SetVisualSlotShown(GSTNEquipment::Slot_ImplantBack, false);
		TestTrue(TEXT("hiding the implant leaves the backpack"), !Implant->IsVisible() && Pack->IsVisible());
		Skin->ClearVisualSlotShown(GSTNEquipment::Slot_ImplantBack);
		TestTrue(TEXT("cleared: back to shown"), Implant->IsVisible());
	}

	// The override outlives a refresh (a kit swap mid-move keeps the sword out); clearing it goes back to the default
	Skin->RefreshAttachments();
	USceneComponent* NewBlade = Skin->FindVisualSlotComponent(Sword);
	TestTrue(TEXT("rebuilt, still shown"), NewBlade && NewBlade != Blade && NewBlade->IsVisible());
	Skin->ClearVisualSlotShown(Sword);
	TestTrue(TEXT("cleared: hidden again"), NewBlade && !NewBlade->IsVisible());

	FGSTN_AttachmentProviderRegistry::Unregister(Id);
	Skin->RefreshAttachments();
	TestEqual(TEXT("no provider, no piece"), Skin->GetAttachmentComponents().Num(), 0);
	Pawn->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
