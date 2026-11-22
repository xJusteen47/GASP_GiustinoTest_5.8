// Pure platform data and math (no Mover, no Network Prediction, no GAS).

#include "GSTNPlatformCoreModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPlatform);

/**
 * Nothing to register: the plugin's tags are native (GSTN_PlatformTags.cpp) and it ships no Config/Tags ini, so there is no tag
 * search path to add (a plugin that adds one must call UGameplayTagsManager::AddTagIniSearchPath itself, as GSTNSurfaceCore does)
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNPlatformCore)
