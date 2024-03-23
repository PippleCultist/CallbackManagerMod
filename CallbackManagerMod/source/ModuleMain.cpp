#include <YYToolkit/shared.hpp>
#include "CallbackManagerInterface.h"
#include "ModuleMain.h"
#include <fstream>
#include "YYTKTypes/YYObjectBase.h"
#include "YYTKTypes/CHashMap.h"
using namespace Aurie;
using namespace YYTK;

YYTKInterface* g_ModuleInterface = nullptr;
YYRunnerInterface g_RunnerInterface;
CInstance* globalInstance = nullptr;

CallbackManagerInterface callbackManager;
std::ofstream outFile;
int FrameNumber = 0;
bool hasObtainedTimeVar = false;
RValue timeVar;

void FrameCallback(FWFrame& FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);
	if (totTime >= 12000000)
	{
		outFile << "Frame Number: " << FrameNumber << " totTime: " << totTime << "\n";
		if (!hasObtainedTimeVar)
		{
			if (g_ModuleInterface->CallBuiltin("variable_global_exists", { RValue("time") }).AsBool())
			{
				timeVar = g_ModuleInterface->CallBuiltin("variable_global_get", { RValue("time") });
				hasObtainedTimeVar = true;
			}
		}
		if (hasObtainedTimeVar)
		{
			char buffer[100];
			sprintf_s(
				buffer,
				sizeof(buffer),
				"%d:%02d:%02d\n",
				static_cast<int>(timeVar[0].m_Real),
				static_cast<int>(timeVar[1].m_Real),
				static_cast<int>(timeVar[2].m_Real)
			);
			outFile << buffer;
		}
		std::vector<std::pair<long long, int>> sortVec;
		for (auto& it : profilerMap)
		{
			sortVec.push_back(std::make_pair(it.second, it.first));
		}
		std::sort(sortVec.begin(), sortVec.end(), std::greater<>());
		long long cumulativeTime = 0;
		for (auto& it : sortVec)
		{
			if (it.first < 500000 || cumulativeTime >= totTime - 2000000)
			{
				break;
			}
			cumulativeTime += it.first;
			outFile << codeIndexToName[it.second] << " " << it.first << "\n";
		}
	}

	totTime = 0;
	FrameNumber++;
	profilerMap.clear();
}

EXPORTED AurieStatus ModulePreinitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
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

	if (ENABLEPROFILER)
	{
		last_status = g_ModuleInterface->CreateCallback(
			Module,
			EVENT_FRAME,
			FrameCallback,
			0
		);

		if (!AurieSuccess(last_status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed to register frame callback");
		}
		outFile.open("profilerResults.txt");
		g_ModuleInterface->GetGlobalInstance(&globalInstance);
	}

	g_RunnerInterface = g_ModuleInterface->GetRunnerInterface();

	return AURIE_SUCCESS;
}