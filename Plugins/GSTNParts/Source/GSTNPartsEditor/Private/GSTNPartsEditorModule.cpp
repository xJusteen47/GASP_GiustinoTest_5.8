#include "GSTNPartsEditorModule.h"

#include "GSTN_PartBalanceReport.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPartsEditor);

class FGSTNPartsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		BalanceReportCommand = IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("GSTN.Parts.BalanceReport"),
			TEXT("GSTN.Parts.BalanceReport <PartSetPath> <TablePath|None> <Damage> <Penetration> [StaggerForce]: logs hits to break, kill and stagger per part"),
			FConsoleCommandWithArgsDelegate::CreateStatic(&UGSTN_PartBalanceReport::RunConsoleCommand),
			ECVF_Default);
	}

	virtual void ShutdownModule() override
	{
		if (BalanceReportCommand)
		{
			IConsoleManager::Get().UnregisterConsoleObject(BalanceReportCommand);
			BalanceReportCommand = nullptr;
		}
	}

private:
	IConsoleObject* BalanceReportCommand = nullptr;
};

IMPLEMENT_MODULE(FGSTNPartsEditorModule, GSTNPartsEditor)
