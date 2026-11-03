// Pure interaction focus core (no GAS, no Network Prediction, no Mover). The plugin ships no tag ini, so there is no
// tag search path to register (contrast GSTNSurface): the roots are native, the leaves belong to the consumers.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNInteractCore);
