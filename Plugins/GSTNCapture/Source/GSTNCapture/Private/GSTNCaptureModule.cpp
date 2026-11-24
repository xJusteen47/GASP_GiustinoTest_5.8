#include "Modules/ModuleManager.h"

// No startup work: UGSTN_FastCapture is created on demand by the capture drivers
IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNCapture)
