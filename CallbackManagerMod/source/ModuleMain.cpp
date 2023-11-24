#include <YYToolkit/shared.hpp>
#include "CallbackManagerInterface.h"
#include "ModuleMain.h"
using namespace Aurie;
using namespace YYTK;

YYTKInterface* g_ModuleInterface = nullptr;
YYRunnerInterface g_RunnerInterface;
AurieModule* g_AurieModule = nullptr;

CallbackManagerInterface callbackManager;

EXPORTED AurieStatus ModulePreinitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	g_AurieModule = Module;
	ObCreateInterface(Module, &callbackManager, "callbackManager");
	return AURIE_SUCCESS;
}

EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	UNREFERENCED_PARAMETER(ModulePath);

	AurieStatus last_status = AURIE_SUCCESS;

	// Gets a handle to the interface exposed by YYTK
	// You can keep this pointer for future use, as it will not change unless YYTK is unloaded.
	last_status = ObGetInterface(
		"YYTK_Main",
		(AurieInterfaceBase*&)(g_ModuleInterface)
	);

	// If we can't get the interface, we fail loading.
	if (!AurieSuccess(last_status))
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

	g_RunnerInterface = g_ModuleInterface->GetRunnerInterface();

	return AURIE_SUCCESS;
}