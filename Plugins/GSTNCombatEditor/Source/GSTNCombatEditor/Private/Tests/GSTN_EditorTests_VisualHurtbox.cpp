#include "GameplayTagsManager.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hurtbox/GSTN_HurtboxEditModel.h"
#include "Hurtbox/SGSTN_HurtboxDetails.h"
#include "Hurtbox/SGSTN_HurtboxList.h"
#include "Misc/AutomationTest.h"
#include "Styling/AppStyle.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

// Offscreen snapshot of the hurtbox set editor's list and details (GSTN.Visual.Hurtbox.Panel). Run like the other GSTN.Visual tests
// (Tests/GSTN_VisualSnapshot.h); it asserts nothing about the look. The 3D viewport is not drawn here (it needs a scene and a viewport client).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualHurtboxPanelTest, "GSTN.Visual.Hurtbox.Panel", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualHurtboxPanelTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transactional));
	const TCHAR* Names[] = { TEXT("head"), TEXT("spine_01_0"), TEXT("spine_01_1"), TEXT("upperarm_l"), TEXT("upperarm_r"), TEXT("hand_l"), TEXT("hand_r") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		FGSTN_Hurtbox Hurtbox;
		Hurtbox.Name = Names[Index];
		Hurtbox.Bone = FName(*FString(Names[Index]).Replace(TEXT("_0"), TEXT("")).Replace(TEXT("_1"), TEXT("")));
		Hurtbox.Shape.Type = Index % 3 == 0 ? EGSTN_ShapeType::Capsule : EGSTN_ShapeType::Sphere;
		Hurtbox.Shape.Extent = FVector(8, 8, 18);
		Hurtbox.Shape.Local = FTransform(FQuat::Identity, FVector(0, (Index - 3) * 12.0, 150));
		if (Index == 0)
		{
			Hurtbox.Tags.AddTag(GSTNCombatHitTags::Root_Hurtbox.GetTag());
			Hurtbox.Priority = 2;
		}
		Set->Hurtboxes.Add(Hurtbox);
	}
	const TSharedRef<FGSTN_HurtboxEditModel> Model = MakeShared<FGSTN_HurtboxEditModel>(Set.Get());
	const int32 Selected[] = { 3 };
	Model->SetSelection(Selected);

	const TSharedRef<SWidget> Panel = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox).HeightOverride(190.f) [ SNew(SGSTN_HurtboxList).Model(Model) ]
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SGSTN_HurtboxDetails).Model(Model)
			]
		];
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, Panel, FIntPoint(620, 560), TEXT("hurtbox-panel")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
