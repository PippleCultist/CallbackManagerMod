#include <YYToolkit/YYTK_Shared.hpp>
#include "CallbackManagerInterface.h"
#include "ModuleMain.h"
#include <fstream>
#include <stacktrace>
#include <chrono>

using namespace Aurie;
using namespace YYTK;

YYTKInterface* g_ModuleInterface = nullptr;
YYRunnerInterface g_RunnerInterface;
CInstance* globalInstance = nullptr;

CallbackManagerInterface callbackManager;
std::ofstream outputLog;
int FrameNumber = 0;
bool hasObtainedTimeVar = false;
RValue timeVar = 0;

void FrameCallback(FWFrame& FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);
	if (totTime >= 12000000)
	{
		callbackManager.LogToFile(MODNAME, "Frame Number: %d totTime: %lld", FrameNumber, totTime);
		if (!hasObtainedTimeVar)
		{
			if (g_ModuleInterface->CallBuiltin("variable_global_exists", { RValue("time") }).ToBoolean())
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
			callbackManager.LogToFile(MODNAME, "%s", buffer);
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
			callbackManager.LogToFile(MODNAME, "%s %lld", codeIndexToName[it.second], it.first);
		}
	}

	totTime = 0;
	FrameNumber++;
	profilerMap.clear();
}

using YYErrorFunc = void (*)(const char* error, ...);
YYErrorFunc origYYErrorFunction = nullptr;
void YYErrorFunction(const char* error, ...)
{
	va_list args;
	va_start(args, error);
	va_list copyArgs;
	va_copy(copyArgs, args);
	int size = vsnprintf(NULL, 0, error, copyArgs);
	va_end(copyArgs);
	char* outputBuffer = new char[size + 1];
	vsprintf_s(outputBuffer, size + 1, error, args);
	va_end(args);
	std::string outputString = outputBuffer;
	outputString.append("\n");
	outputString.append(to_string(std::stacktrace::current()));
	outputString.append("\n");
	callbackManager.LogToFile(MODNAME, outputString.c_str());
	origYYErrorFunction(outputString.c_str());
}

void runnerInitCallback(FunctionWrapper<void(int)>& dummyWrapper)
{
	g_RunnerInterface = g_ModuleInterface->GetRunnerInterface();

	PVOID trampolineFunc = nullptr;
	MmCreateHook(g_ArSelfModule, "YYError", g_RunnerInterface.YYError, YYErrorFunction, &trampolineFunc);
	origYYErrorFunction = (YYErrorFunc)trampolineFunc;

	AurieStatus status = g_ModuleInterface->CreateCallback(g_ArSelfModule, EVENT_OBJECT_CALL, CodeCallback, 0);

	if (!AurieSuccess(status))
	{
		DbgPrintEx(LOG_SEVERITY_CRITICAL, "Failed to create code event callback with status %d", status);
		return;
	}
}

EXPORTED AurieStatus ModulePreinitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	UNREFERENCED_PARAMETER(ModulePath);

	CreateDirectory(L"Logs", NULL);
	auto time = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	outputLog.open(std::format("Logs/{:%Y_%m_%d_%H_%M_%S}.log", time));

	ObCreateInterface(Module, &callbackManager, "callbackManager");

	// Gets a handle to the interface exposed by YYTK
	// You can keep this pointer for future use, as it will not change unless YYTK is unloaded.
	g_ModuleInterface = GetInterface();

	// If we can't get the interface, we fail loading.
	if (g_ModuleInterface == nullptr)
	{
		DbgPrintEx(LOG_SEVERITY_CRITICAL, "Failed to get YYTK interface");
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
	}

	g_ModuleInterface->CreateCallback(
		Module,
		EVENT_RUNNER_INIT,
		runnerInitCallback,
		100
	);
	return AURIE_SUCCESS;
}

EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	UNREFERENCED_PARAMETER(ModulePath);

	if (ENABLEPROFILER)
	{
		AurieStatus last_status = AURIE_SUCCESS;
		last_status = g_ModuleInterface->CreateCallback(
			Module,
			EVENT_FRAME,
			FrameCallback,
			0
		);

		if (!AurieSuccess(last_status))
		{
			DbgPrintEx(LOG_SEVERITY_ERROR, "Failed to register frame callback");
		}
		g_ModuleInterface->GetGlobalInstance(&globalInstance);
	}

	callbackManager.LogToFile(MODNAME, "Finished initialization");

	return AURIE_SUCCESS;
}