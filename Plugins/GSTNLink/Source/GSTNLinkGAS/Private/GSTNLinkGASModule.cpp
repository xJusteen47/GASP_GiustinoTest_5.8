// Links, ability-system half (no Network Prediction, no Mover).

#include "Modules/ModuleManager.h"

/**
 * The module holds no startup work: the link registry is an engine subsystem that builds itself once the asset manager finished its
 * first scan, and plugins append their break reasons through UGSTN_LinkRegistry::RegisterPluginReason from their own startup.
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNLinkGAS)
