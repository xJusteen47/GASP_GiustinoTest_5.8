// An external client of the editor hooks (plan m5 E19 (2), S 12b): what an editor module of another plugin (the destructible parts editor is the
// planned one) does with the PUBLIC surface of GSTNCombatEditor only. Includes below are the public headers plus engine ones: if this
// file needs a Private include to compile, a hook is not published and the test is wrong, not the include.

#include "Misc/AutomationTest.h"
#include "PropertyEditorModule.h"
#include "IPropertyTypeCustomization.h"
#include "Modules/ModuleManager.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Validation/GSTN_ValidationReport.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNEditorHooksTest
{
	/** The customization of a fake extension struct: the shape of what a parts editor plugs into the hit data row */
	class FFakeExtensionCustomization final : public IPropertyTypeCustomization
	{
	public:
		static TSharedRef<IPropertyTypeCustomization> MakeInstance() { return MakeShared<FFakeExtensionCustomization>(); }
		virtual void CustomizeHeader(TSharedRef<IPropertyHandle>, FDetailWidgetRow&, IPropertyTypeCustomizationUtils&) override {}
		virtual void CustomizeChildren(TSharedRef<IPropertyHandle>, IDetailChildrenBuilder&, IPropertyTypeCustomizationUtils&) override {}
	};

	/** A fake track editor of an external plugin: its validator reports into the same report the panel shows, under its own code */
	class FFakeExternalTrackEditor final : public IGSTN_TrackEditor
	{
	public:
		virtual const UScriptStruct* GetTrackStruct() const override { return FGSTN_TestExternalTrack::StaticStruct(); }
		virtual FText GetDisplayName() const override { return FText::FromString(TEXT("External")); }
		virtual void Validate(const FGSTN_TimelineTrack& Track, const FGSTN_CombatTimelineData&, FGSTN_ValidationReport& Report) const override
		{
			if (static_cast<const FGSTN_TestExternalTrack&>(Track).Level < 0)
			{
				Report.Add(EGSTN_Severity::Warning, TEXT("External.Level"), FText::FromString(TEXT("negative level")));
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HooksExternalClientTest, "GSTNCombat.Editor.Hooks.ExternalClientCompiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGSTN_HooksExternalClientTest::RunTest(const FString& Parameters)
{
	using namespace GSTNEditorHooksTest;

	// (a) A hit extension customization registers and unregisters through the engine's property module, no M5 type involved
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const FName ExtensionName = FGSTN_TestExternalHitExtension::StaticStruct()->GetFName();
	PropertyModule.RegisterCustomPropertyTypeLayout(ExtensionName, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FFakeExtensionCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();
	TestTrue(TEXT("the fake extension derives from the hit extension base"), FGSTN_TestExternalHitExtension::StaticStruct()->IsChildOf(FGSTN_HitExtension::StaticStruct()));
	PropertyModule.UnregisterCustomPropertyTypeLayout(ExtensionName);

	// (b) A track editor with a validator registers through the public registry, is found for its struct and reports with its own code
	FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	Registry.Register(MakeShared<FFakeExternalTrackEditor>());
	const TSharedPtr<const IGSTN_TrackEditor> Found = Registry.Find(FGSTN_TestExternalTrack::StaticStruct());
	TestTrue(TEXT("the external editor is registered"), Found.IsValid());
	if (Found.IsValid())
	{
		FGSTN_TestExternalTrack Track;
		Track.Level = -1;
		FGSTN_CombatTimelineData Data;
		FGSTN_ValidationReport Report;
		Found->Validate(Track, Data, Report);
		TestTrue(TEXT("its finding is in the report under its own code"), Report.HasCode(TEXT("External.Level")));
		TestFalse(TEXT("a warning does not invalidate"), Report.HasErrors());

		TArray<TSharedRef<const IGSTN_TrackEditor>> All;
		Registry.GetAll(All);
		TestTrue(TEXT("and Add Track lists it"), All.ContainsByPredicate([](const TSharedRef<const IGSTN_TrackEditor>& E) { return E->GetTrackStruct() == FGSTN_TestExternalTrack::StaticStruct(); }));
	}
	TestTrue(TEXT("unregistering removes it"), Registry.Unregister(FGSTN_TestExternalTrack::StaticStruct()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
