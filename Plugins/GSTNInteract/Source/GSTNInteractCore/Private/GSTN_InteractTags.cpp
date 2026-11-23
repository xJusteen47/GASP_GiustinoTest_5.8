// Interaction focus tags (interaction.md §8). No GAS, no Network Prediction, no Mover.

#include "GSTN_InteractTags.h"

namespace GSTNInteractTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Interact, "Interact", "Root of the interaction tags owned by GSTNInteract");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Root_Target, "Interact.Target", "Root of the meanings a focus can project. Leaves are declared by the plugin or content that adds the meaning; at most one is projected at a time");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_FocusChanged, "Message.Interact.FocusChanged", "Local presentation event raised when the presented focus changes. Never read by the simulation, never replicated");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({ Root_Interact, Root_Target, Message_FocusChanged });
	}
}
