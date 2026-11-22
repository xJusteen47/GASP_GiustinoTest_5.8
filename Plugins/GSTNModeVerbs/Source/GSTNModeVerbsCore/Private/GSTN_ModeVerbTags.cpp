// Mode verbs, pure core.

#include "GSTN_ModeVerbTags.h"

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNModeVerbsCore);

namespace GSTNModeVerbTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Mode, "Ability.Mode", "Root of the verbs a mode adds as abilities; carried by every UGSTN_GA_ModeVerb");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Mode_Interact, "Ability.Mode.Interact", "Activate a terminal, arm a charge, open a gate. Target: the interact focus (Interact.Target.Objective)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Mode_CallReinforcement, "Ability.Mode.CallReinforcement", "Call a reinforcement; handled by the respawn framework. Target: aim point");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Mode_CallDrop, "Ability.Mode.CallDrop", "Call a supply drop. Target: aim point");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Mode_CallExtraction, "Ability.Mode.CallExtraction", "Call extraction. Target: the extraction console (interact target)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Mode_Interact, "Command.Mode.Interact", "Hold(Interact) requiring Interact.Target.Objective; a binding in the kit's slot set (data)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Objective, "Interact.Target.Objective", "A requestable mission prop is the interact focus. Projected by the interact probe from the prop's GSTNInteract record");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mode_Verb, "Message.Mode.Verb", "A verb was confirmed and validated by the host, once. Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mode_VerbConfirmed, "Message.Mode.VerbConfirmed", "Raw seam D-1 key for a target-less verb; the verb subsystem validates it and republishes Message.Mode.Verb");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mode_VerbRefused, "Message.Mode.VerbRefused", "The host refused a confirmed verb (Magnitude = EGSTN_VerbRefusal). Server event stream only");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({ Ability_Mode, Ability_Mode_Interact, Ability_Mode_CallReinforcement, Ability_Mode_CallDrop, Ability_Mode_CallExtraction,
			Command_Mode_Interact, Interact_Target_Objective, Message_Mode_Verb, Message_Mode_VerbConfirmed, Message_Mode_VerbRefused });
	}
}
