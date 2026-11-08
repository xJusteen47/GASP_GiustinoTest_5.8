// Bridge between parts and minions (plan P15 interface, P16 hit flow).

#include "GSTNPartsMinionModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPartsMinion);

/**
 * No startup code on purpose. The glue is UGSTN_PartsMinionSubsystem, a world subsystem of this module: it exists only when this plugin
 * is enabled (which needs both GSTNParts and GSTNMinion), binds to the world's minion subsystem in Initialize and unbinds in Deinitialize.
 * Neither plugin knows this one, so without it the minion hit flow runs exactly as M8 ships it.
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNPartsMinion)
