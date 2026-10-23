// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelinePanelTest.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace GSTNTimelinePanelTest
{
	const TCHAR* const OpenCommandName = TEXT("gstn.Combat.OpenTimelinePanelTest");

	namespace
	{
		const FName PanelTestTabId(TEXT("GSTNTimelinePanelTest"));

		/** "stress" was given to the command that opens the tab: read by the spawner */
		bool bOpenStress = false;

		IConsoleCommand* OpenCommand = nullptr;
		bool bTabRegistered = false;

		template <typename TTrack>
		TInstancedStruct<FGSTN_TimelineTrack> MakeDemoTrack(int32 Start, int32 End)
		{
			TTrack Track;
			Track.StartFrame = Start;
			Track.EndFrame = End;
			return TInstancedStruct<FGSTN_TimelineTrack>::Make<TTrack>(Track);
		}

		FGSTN_TestKeyedTrack MakeDemoKeyedTrack(int32 Start, int32 End, int32 KeyStep)
		{
			FGSTN_TestKeyedTrack Track;
			Track.StartFrame = Start;
			Track.EndFrame = End;
			for (int32 Frame = Start; Frame < End; Frame += KeyStep)
			{
				FGSTN_TestKey Key;
				Key.Frame = Frame;
				Key.Location = FVector(Frame * 2.0, 0.0, 0.0);
				Track.Keys.Add(Key);
			}
			return Track;
		}

		/**
		 * The host of the tab's content: it owns the transient timeline, the model and the undo client for as long as the tab is open
		 * (the panel only shares the model), and makes sure the demo keyed track has its adapter.
		 */
		class SGSTN_TimelinePanelTestHost : public SCompoundWidget
		{
		public:
			SLATE_BEGIN_ARGS(SGSTN_TimelinePanelTestHost) {}
				SLATE_ARGUMENT(bool, bStress)
			SLATE_END_ARGS()

			void Construct(const FArguments& InArgs)
			{
				// Transactional, or the model's Modify() records nothing and undo has nothing to restore
				Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
				FillDemoTimeline(Timeline->Data, InArgs._bStress);
				FGSTN_KeyedTrackAccessRegistry::Register(FGSTN_TestKeyedTrack::StaticStruct(), MakeShared<FGSTN_TestKeyedAccess>());

				Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
				UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);

				ChildSlot
				[
					SNew(SGSTN_TimelinePanel)
					.Model(Model)
					.bShowPaintStats(true)
				];
			}

		private:
			TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
			TSharedPtr<FGSTN_TimelineEditorModel> Model;
			TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;
		};

		TSharedRef<SDockTab> SpawnPanelTestTab(const FSpawnTabArgs& Args)
		{
			const bool bStress = bOpenStress;
			bOpenStress = false;
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				.Label(LOCTEXT("PanelTestTabLabel", "GSTN Timeline Panel Test"))
				[
					SNew(SGSTN_TimelinePanelTestHost).bStress(bStress)
				];
		}

		void OpenPanelTestTab(const TArray<FString>& Args)
		{
			if (!FSlateApplication::IsInitialized() || !bTabRegistered)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s needs the editor UI (a rendering editor, not a commandlet)"), OpenCommandName);
				return;
			}
			bOpenStress = Args.Contains(TEXT("stress"));
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(PanelTestTabId));
		}
	}

	void Register()
	{
		OpenCommand = IConsoleManager::Get().RegisterConsoleCommand(
			OpenCommandName,
			TEXT("Opens the GSTN timeline panel test tab on a transient timeline. Add 'stress' for 12 keyed tracks of 60 keys. Editor only; removed when the toolkit of E5 lands."),
			FConsoleCommandWithArgsDelegate::CreateStatic(&OpenPanelTestTab),
			ECVF_Default);

		// Without Slate (a commandlet, -nullrhi automation) there is nowhere to dock a tab: the console command then only logs
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PanelTestTabId, FOnSpawnTab::CreateStatic(&SpawnPanelTestTab))
				.SetDisplayName(LOCTEXT("PanelTestTabName", "GSTN Timeline Panel Test"))
				.SetMenuType(ETabSpawnerMenuType::Hidden);
			bTabRegistered = true;
		}
	}

	void Unregister()
	{
		if (OpenCommand != nullptr)
		{
			IConsoleManager::Get().UnregisterConsoleObject(OpenCommand);
			OpenCommand = nullptr;
		}
		if (bTabRegistered && FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PanelTestTabId);
		}
		bTabRegistered = false;
		FGSTN_KeyedTrackAccessRegistry::Unregister(FGSTN_TestKeyedTrack::StaticStruct());
	}

	void FillDemoTimeline(FGSTN_CombatTimelineData& Data, bool bStress)
	{
		if (bStress)
		{
			Data.TotalFrames = 120;
			Data.FirstActionableFrame = 90;
			for (int32 Row = 0; Row < 12; ++Row)
			{
				Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TestKeyedTrack>(MakeDemoKeyedTrack(0, 120, 2)));
			}
			return;
		}

		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 24;
		Data.Tail.bHasTail = true;
		Data.Tail.StartFrame = INDEX_NONE;
		Data.Tail.BlendOutFrames = 6;

		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_Hitbox>(8, 11));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_Hitbox>(14, 17));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_Tags>(0, 24));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_CancelWindow>(16, 26));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_Movement>(4, 12));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_HurtboxModifier>(2, 9));
		Data.Tracks.Add(MakeDemoTrack<FGSTN_TimelineTrack_Event>(5, 6));
		FGSTN_TimelineTrack_WeaponSwitch Switch;
		Switch.StartFrame = 20;
		Switch.EndFrame = 21;
		Switch.SwapFrame = 20;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_WeaponSwitch>(Switch));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TestKeyedTrack>(MakeDemoKeyedTrack(6, 20, 4)));
	}
}

#undef LOCTEXT_NAMESPACE
