// Levels mode: tags and module.

#include "GSTN_LevelsTags.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNMode_LevelsRuntime);

namespace GSTNLevelsTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Levels_Collectible, "Levels.Collectible", "Default identity of a Levels collectible; designers add leaves on the actor");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Levels_LevelStarted, "Message.Levels.LevelStarted", "The level began. Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Levels_GoalGraceStarted, "Message.Levels.GoalGraceStarted", "A goal zone's grace timer started (Magnitude = seconds)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Levels_GoalReached, "Message.Levels.GoalReached", "A goal zone finished; the level completes");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Levels_LevelCompleted, "Message.Levels.LevelCompleted", "The level is complete (Magnitude = total score)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Levels_LevelSetCompleted, "Message.Levels.LevelSetCompleted", "The last level of the set was completed");
}
