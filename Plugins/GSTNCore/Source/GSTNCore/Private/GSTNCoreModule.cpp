// Shared helpers of the GSTN plugins: pure, no world.

#include "Modules/ModuleManager.h"

/** Nothing to register: the module holds plain types and math only (no tags, no settings, no log category yet) */
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNCore)
