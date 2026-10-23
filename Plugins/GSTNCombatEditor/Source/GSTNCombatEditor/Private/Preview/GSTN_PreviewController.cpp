// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/GSTN_PreviewController.h"

#include "Animation/AnimMontage.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

TSharedRef<FGSTN_PreviewController> FGSTN_PreviewController::Create(const TSharedRef<FGSTN_TimelineEditorModel>& Model, const FSoftObjectPath& StorageKey)
{
	FSoftObjectPath Key = StorageKey;
	if (Key.IsNull())
	{
		if (const UObject* Target = Model->GetContext().GetTarget())
		{
			Key = FSoftObjectPath(Target);
		}
	}
	TSharedRef<FGSTN_PreviewController> Controller = MakeShareable(new FGSTN_PreviewController(Model, Key));
	Controller->Initialize();
	return Controller;
}

TSharedRef<FGSTN_PreviewController> FGSTN_PreviewController::CreateUnstored(const TSharedRef<FGSTN_TimelineEditorModel>& Model)
{
	TSharedRef<FGSTN_PreviewController> Controller = MakeShareable(new FGSTN_PreviewController(Model, FSoftObjectPath()));
	Controller->Initialize();
	return Controller;
}

FGSTN_PreviewController::FGSTN_PreviewController(const TSharedRef<FGSTN_TimelineEditorModel>& InModel, const FSoftObjectPath& InStorageKey)
	: Model(InModel)
	, Scene(MakeShared<FGSTN_TimelinePreviewScene>())
	, StorageKey(InStorageKey)
{
}

FGSTN_PreviewController::~FGSTN_PreviewController()
{
	Model->OnPlayheadChanged.Remove(PlayheadHandle);
	Model->OnDataChanged.Remove(DataHandle);
}

void FGSTN_PreviewController::Initialize()
{
	// The asset's own subject if one was stored, else the settings' default kit and skin, else nothing
	if (StorageKey.IsNull() || !UGSTN_CombatEditorPreviewState::Find(StorageKey, Subject))
	{
		const UGSTN_CombatEditorSettings& Settings = UGSTN_CombatEditorSettings::Get();
		if (!Settings.DefaultPreviewKit.IsNull() || !Settings.DefaultPreviewSkin.IsNull())
		{
			Subject.Kind = EGSTN_PreviewSubjectKind::KitSkin;
			Subject.Kit = Settings.DefaultPreviewKit;
			Subject.Skin = Settings.DefaultPreviewSkin;
		}
	}
	PlayheadHandle = Model->OnPlayheadChanged.AddSP(this, &FGSTN_PreviewController::HandlePlayheadChanged);
	DataHandle = Model->OnDataChanged.AddSP(this, &FGSTN_PreviewController::HandleDataChanged);
	ApplySubject();
}

void FGSTN_PreviewController::SubjectEdited()
{
	ApplySubject();
	if (!StorageKey.IsNull())
	{
		UGSTN_CombatEditorPreviewState::Store(StorageKey, Subject);
	}
}

void FGSTN_PreviewController::SetSubject(const FGSTN_PreviewSubject& NewSubject)
{
	Subject = NewSubject;
	SubjectEdited();
}

void FGSTN_PreviewController::ApplySubject()
{
	const FGSTN_ResolvedSubject Resolved = GSTNPreviewSubject::Resolve(Subject);
	SceneProblems = Scene->SetSubject(Resolved);
	Label = Resolved.Label;
	Problems = Resolved.Problems;
	Problems.Append(SceneProblems);
	UpdateMontage();
	Refresh();
}

void FGSTN_PreviewController::UpdateMontage()
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	UAnimMontage* Played = nullptr;
	FName Section = NAME_None;
	float PlayRate = 1.f;
	if (Data != nullptr)
	{
		const FSoftObjectPath Path = Data->Montage.ToSoftObjectPath();
		if (Path != MontagePath)
		{
			MontagePath = Path;
			Montage.Reset(Data->Montage.IsNull() ? nullptr : Data->Montage.LoadSynchronous());
		}
		Played = Montage.Get();
		Section = Data->MontageSection;
		PlayRate = Data->MontagePlayRate;
	}
	Scene->GetPose().SetMontage(Played, Section, PlayRate, Model->GetTickRate());
}

void FGSTN_PreviewController::Refresh()
{
	Scene->GetPose().Evaluate(Model->GetPlayhead());
	OnPreviewChanged.Broadcast();
}

void FGSTN_PreviewController::HandlePlayheadChanged(int32 Frame)
{
	Scene->GetPose().Evaluate(Frame);
	OnPreviewChanged.Broadcast();
}

void FGSTN_PreviewController::HandleDataChanged()
{
	UpdateMontage();
	Refresh();
}
