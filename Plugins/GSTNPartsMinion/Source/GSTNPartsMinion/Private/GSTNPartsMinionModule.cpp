// Bridge between parts and minions (plan P15: interface only).

#include "GSTNPartsMinionModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPartsMinion);

/**
 * Empty on purpose. P16 registers here the glue that runs the part resolve step from the minion hit flow and applies its outcome to
 * the store (see README "What P16 adds"). Until then the module only proves that the bridge links against both pure halves.
 */
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNPartsMinion)
