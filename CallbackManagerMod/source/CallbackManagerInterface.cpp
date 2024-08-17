#include "CallbackManagerInterface.h"
#include "ModuleMain.h"
#include <YYToolkit/shared.hpp>
#include <array>
#include <semaphore>
#include <chrono>
#include <fstream>

using namespace Aurie;
using namespace YYTK;

extern std::ofstream outputLog;

AurieStatus CallbackManagerInterface::Create()
{
	return AURIE_SUCCESS;
}
void CallbackManagerInterface::Destroy()
{

}

void CallbackManagerInterface::QueryVersion(
	OUT short& Major,
	OUT short& Minor,
	OUT short& Patch
)
{
	Major = 1;
	Minor = 0;
	Patch = 0;
}

bool callOriginalFunctionFlag = false;
bool cancelOriginalFunctionFlag = false;

std::binary_semaphore initOrigFuncSemaphore(1);

std::unordered_map<int, CallbackRoutineList<CodeEvent>*> codeIndexToCallbackMap;
std::unordered_map<std::string, CallbackRoutineList<CodeEvent>> codeEventCallbackMap;

std::unordered_map<int, long long> profilerMap;
std::unordered_map<int, const char*> codeIndexToName;
long long curTime = 0;
long long totTime = 0;

void CodeCallback(FWCodeEvent& CodeContext)
{
	if (ENABLEPROFILER)
	{
		curTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
	}
	std::tuple<CInstance*, CInstance*, CCode*, int, RValue*> args = CodeContext.Arguments();

	CInstance*	Self	= std::get<0>(args);
	CInstance*	Other	= std::get<1>(args);
	CCode*		Code	= std::get<2>(args);
	int			Flags	= std::get<3>(args);
	RValue*		Res		= std::get<4>(args);

	auto codeEventCallback = codeIndexToCallbackMap.find(Code->m_CodeIndex);
	CallbackRoutineList<CodeEvent>* callbackRoutineList = nullptr;
	if (codeEventCallback != codeIndexToCallbackMap.end())
	{
		callbackRoutineList = codeEventCallback->second;
	}
	else
	{
		if (ENABLEPROFILER)
		{
			codeIndexToName[Code->m_CodeIndex] = Code->GetName();
		}
		if (codeEventCallbackMap.find(Code->GetName()) == codeEventCallbackMap.end())
		{
			callbackRoutineList = &(codeEventCallbackMap[Code->GetName()] = CallbackRoutineList<CodeEvent>());
		}
		else
		{
			callbackRoutineList = &codeEventCallbackMap[Code->GetName()];
		}
		codeIndexToCallbackMap[Code->m_CodeIndex] = callbackRoutineList;
	}

	bool prevCallOriginalFunctionFlag = callOriginalFunctionFlag;
	bool prevCancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
	bool callFlag = false;
	bool cancelFlag = false;
	callOriginalFunctionFlag = false;
	cancelOriginalFunctionFlag = false;
	for (CallbackRoutine<CodeEvent>& routine : callbackRoutineList->routineList)
	{
		if (routine.beforeRoutine != nullptr)
		{
			routine.beforeRoutine(args);
			routine.callOriginalFunctionFlag = callOriginalFunctionFlag;
			routine.cancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
			callFlag = callFlag || callOriginalFunctionFlag;
			cancelFlag = cancelFlag || cancelOriginalFunctionFlag;
			callOriginalFunctionFlag = false;
			cancelOriginalFunctionFlag = false;
		}
	}
	if (callFlag && cancelFlag)
	{
		g_ModuleInterface->Print(CM_RED, "ERROR: A CALL AND CANCEL REQUEST WAS SENT TO THE CODE EVENT %s", Code->GetName());
		for (CallbackRoutine<CodeEvent>& routine : callbackRoutineList->routineList)
		{
			if (routine.callOriginalFunctionFlag)
			{
				g_ModuleInterface->Print(CM_RED, "CALL REQUEST: %s", routine.modName.c_str());
			}
			if (routine.cancelOriginalFunctionFlag)
			{
				g_ModuleInterface->Print(CM_RED, "CANCEL REQUEST: %s", routine.modName.c_str());
			}
		}
		CodeContext.Override(true);
	}
	else if (callFlag || !cancelFlag)
	{
		CodeContext.Call();
	}
	else
	{
		CodeContext.Override(true);
	}
	for (CallbackRoutine<CodeEvent>& routine : callbackRoutineList->routineList)
	{
		if (routine.afterRoutine != nullptr)
		{
			routine.afterRoutine(args);
		}
	}
	callOriginalFunctionFlag = prevCallOriginalFunctionFlag;
	cancelOriginalFunctionFlag = prevCancelOriginalFunctionFlag;
	if (ENABLEPROFILER)
	{
		long long timeElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count() - curTime;
		if (profilerMap.count(Code->m_CodeIndex) == 0)
		{
			profilerMap[Code->m_CodeIndex] = 0;
		}
		profilerMap[Code->m_CodeIndex] += timeElapsed;
		totTime += timeElapsed;
	}
}

bool hasCreatedCodeEventCallback = false;

AurieStatus CallbackManagerInterface::RegisterCodeEventCallback(
	IN const std::string& ModName,
	IN const std::string& CodeEventName,
	IN CodeEvent BeforeCodeEventRoutine,
	IN CodeEvent AfterCodeEventRoutine
)
{
	AurieStatus status = AURIE_SUCCESS;
	if (!hasCreatedCodeEventCallback)
	{
		status = g_ModuleInterface->CreateCallback(g_ArSelfModule, EVENT_OBJECT_CALL, CodeCallback, 0);

		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed to create code event callback with status %d", status);
			return status;
		}
		hasCreatedCodeEventCallback = true;
	}

	auto codeEventCallback = codeEventCallbackMap.find(CodeEventName);
	if (codeEventCallback == codeEventCallbackMap.end())
	{
		codeEventCallbackMap[CodeEventName] = CallbackRoutineList<CodeEvent>();
	}
	if (BeforeCodeEventRoutine != nullptr || AfterCodeEventRoutine != nullptr)
	{
		codeEventCallbackMap[CodeEventName].routineList.push_back(std::move(CallbackRoutine(BeforeCodeEventRoutine, AfterCodeEventRoutine, ModName)));
	}
	return AURIE_SUCCESS;
}

const int maxScriptFunctionCallbacks = 1000;
struct ScriptFunctionCallbackObject
{
	CallbackRoutineList<PFUNC_YYGMLScript> callbackRoutineList;

	ScriptFunctionCallbackObject()
	{
	}

	ScriptFunctionCallbackObject(CallbackRoutineList<PFUNC_YYGMLScript> callbackRoutineList) : callbackRoutineList(callbackRoutineList)
	{
	}

	RValue& HandleScriptFunctionCallback(
		IN CInstance* Self,
		IN CInstance* Other,
		OUT RValue& ReturnValue,
		IN int ArgumentCount,
		IN RValue** Arguments
	)
	{
		// Sometimes the hook might happen before the originalFunction is set. Just wait until it is set in the register callback
		if (callbackRoutineList.originalFunction == nullptr)
		{
			initOrigFuncSemaphore.acquire();
			initOrigFuncSemaphore.release();
			if (callbackRoutineList.originalFunction == nullptr)
			{
				g_ModuleInterface->Print(CM_RED, "Still couldn't get the original function\n");
				return ReturnValue;
			}
		}
		bool prevCallOriginalFunctionFlag = callOriginalFunctionFlag;
		bool prevCancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
//		printf("script function callback %d %d %s\n", callOriginalFunctionFlag, cancelOriginalFunctionFlag, callbackRoutineList.name.c_str());
		bool callFlag = false;
		bool cancelFlag = false;
		callOriginalFunctionFlag = false;
		cancelOriginalFunctionFlag = false;
		for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList.routineList)
		{
			if (routine.beforeRoutine != nullptr)
			{
				routine.beforeRoutine(Self, Other, ReturnValue, ArgumentCount, Arguments);
				routine.callOriginalFunctionFlag = callOriginalFunctionFlag;
				routine.cancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
				callFlag = callFlag || callOriginalFunctionFlag;
				cancelFlag = cancelFlag || cancelOriginalFunctionFlag;
				callOriginalFunctionFlag = false;
				cancelOriginalFunctionFlag = false;
			}
		}
		if (callFlag && cancelFlag)
		{
			g_ModuleInterface->Print(CM_RED, "ERROR: A CALL AND CANCEL REQUEST WAS SENT TO THE FUNCTION %s", callbackRoutineList.name.c_str());
			for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList.routineList)
			{
				if (routine.callOriginalFunctionFlag)
				{
					g_ModuleInterface->Print(CM_RED, "CALL REQUEST: %s", routine.modName.c_str());
				}
				if (routine.cancelOriginalFunctionFlag)
				{
					g_ModuleInterface->Print(CM_RED, "CANCEL REQUEST: %s", routine.modName.c_str());
				}
			}
		}
		else if (callFlag || !cancelFlag)
		{
			ReturnValue = callbackRoutineList.originalFunction(Self, Other, ReturnValue, ArgumentCount, Arguments);
		}
		for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList.routineList)
		{
			if (routine.afterRoutine != nullptr)
			{
				routine.afterRoutine(Self, Other, ReturnValue, ArgumentCount, Arguments);
			}
		}
		callOriginalFunctionFlag = prevCallOriginalFunctionFlag;
		cancelOriginalFunctionFlag = prevCancelOriginalFunctionFlag;
		return ReturnValue;
	}
} scriptFunctionCallbackArr[maxScriptFunctionCallbacks];

template<size_t index>
RValue& scriptFunctionCallbackHelper(
	IN CInstance* Self,
	IN CInstance* Other,
	OUT RValue& ReturnValue,
	IN int ArgumentCount,
	IN RValue** Arguments
)
{
	return scriptFunctionCallbackArr[index].HandleScriptFunctionCallback(Self, Other, ReturnValue, ArgumentCount, Arguments);
}

template<size_t... Is>
auto MakeScriptFunctionCallbackArrayHelper(std::integer_sequence<size_t, Is...>)
{
	return std::array{ scriptFunctionCallbackHelper<Is>... };
}

template<size_t size>
auto MakeScriptFunctionCallbackArray()
{
	return MakeScriptFunctionCallbackArrayHelper(std::make_index_sequence<size>{});
}

std::unordered_map<std::string, ScriptFunctionCallbackObject*> scriptFunctionNameCallbackMap;
auto scriptFunctionCallbackObjectArr = MakeScriptFunctionCallbackArray<maxScriptFunctionCallbacks>();
int numScriptCallback = 0;
AurieStatus CallbackManagerInterface::RegisterScriptFunctionCallback(
	IN const std::string& ModName,
	IN const std::string& ScriptFunctionName,
	IN PFUNC_YYGMLScript BeforeScriptFunctionRoutine,
	IN PFUNC_YYGMLScript AfterScriptFunctionRoutine,
	OUT PFUNC_YYGMLScript* OriginalScriptFunctionRoutine
)
{
	AurieStatus status = AURIE_SUCCESS;
	auto callbackList = scriptFunctionNameCallbackMap.find(ScriptFunctionName);
	if (callbackList == scriptFunctionNameCallbackMap.end())
	{
		int scriptIndex = g_RunnerInterface.Script_Find_Id(ScriptFunctionName.c_str()) - 100000;
		CScript* scriptPtr = nullptr;
		status = g_ModuleInterface->GetScriptData(scriptIndex, scriptPtr);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed obtaining script function data for %s at script index %d with status %d", ScriptFunctionName.c_str(), scriptIndex, status);
			return AURIE_EXTERNAL_ERROR;
		}
		if (numScriptCallback >= maxScriptFunctionCallbacks)
		{
			g_ModuleInterface->Print(CM_RED, "Failed to register %s since the number of script function callbacks has exceeded %d", ScriptFunctionName.c_str(), maxScriptFunctionCallbacks);
			return AURIE_EXTERNAL_ERROR;
		}
		scriptFunctionNameCallbackMap[ScriptFunctionName] = &(scriptFunctionCallbackArr[numScriptCallback] = ScriptFunctionCallbackObject(CallbackRoutineList<PFUNC_YYGMLScript>(ScriptFunctionName)));

		PVOID trampolineFunc = nullptr;
		initOrigFuncSemaphore.acquire();
		status = MmCreateHook(g_ArSelfModule, ScriptFunctionName, scriptPtr->m_Functions->m_ScriptFunction, scriptFunctionCallbackObjectArr[numScriptCallback], &trampolineFunc);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed hooking %s with status %d", ScriptFunctionName.c_str(), status);
			initOrigFuncSemaphore.release();
			return AURIE_EXTERNAL_ERROR;
		}
		scriptFunctionCallbackArr[numScriptCallback].callbackRoutineList.originalFunction = (PFUNC_YYGMLScript)trampolineFunc;
		initOrigFuncSemaphore.release();

		numScriptCallback++;
	}
	if (BeforeScriptFunctionRoutine != nullptr || AfterScriptFunctionRoutine != nullptr)
	{
		scriptFunctionNameCallbackMap[ScriptFunctionName]->callbackRoutineList.routineList.push_back(std::move(CallbackRoutine(BeforeScriptFunctionRoutine, AfterScriptFunctionRoutine, ModName)));
	}
	if (OriginalScriptFunctionRoutine != nullptr)
	{
		*OriginalScriptFunctionRoutine = scriptFunctionNameCallbackMap[ScriptFunctionName]->callbackRoutineList.originalFunction;
	}
	return AURIE_SUCCESS;
}

const int maxBuiltinFunctionCallbacks = 1000;
struct BuiltinFunctionCallbackObject
{
	CallbackRoutineList<TRoutine> callbackRoutineList;

	BuiltinFunctionCallbackObject()
	{
	}

	BuiltinFunctionCallbackObject(CallbackRoutineList<TRoutine> callbackRoutineList) : callbackRoutineList(callbackRoutineList)
	{
	}

	void HandleBuiltinFunctionCallback(
		OUT RValue* Result,
		IN CInstance* Self,
		IN CInstance* Other,
		IN int numArgs,
		IN RValue* Args
	)
	{
		// Sometimes the hook might happen before the originalFunction is set. Just wait until it is set in the register callback
		if (callbackRoutineList.originalFunction == nullptr)
		{
			initOrigFuncSemaphore.acquire();
			initOrigFuncSemaphore.release();
			if (callbackRoutineList.originalFunction == nullptr)
			{
				g_ModuleInterface->Print(CM_RED, "Still couldn't get the original function\n");
				return;
			}
		}
		bool prevCallOriginalFunctionFlag = callOriginalFunctionFlag;
		bool prevCancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
		bool callFlag = false;
		bool cancelFlag = false;
		callOriginalFunctionFlag = false;
		cancelOriginalFunctionFlag = false;
		for (CallbackRoutine<TRoutine>& routine : callbackRoutineList.routineList)
		{
			if (routine.beforeRoutine != nullptr)
			{
				routine.beforeRoutine(Result, Self, Other, numArgs, Args);
				routine.callOriginalFunctionFlag = callOriginalFunctionFlag;
				routine.cancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
				callFlag = callFlag || callOriginalFunctionFlag;
				cancelFlag = cancelFlag || cancelOriginalFunctionFlag;
				callOriginalFunctionFlag = false;
				cancelOriginalFunctionFlag = false;
			}
		}
		if (callFlag && cancelFlag)
		{
			g_ModuleInterface->Print(CM_RED, "ERROR: A CALL AND CANCEL REQUEST WAS SENT TO THE FUNCTION %s", callbackRoutineList.name.c_str());
			for (CallbackRoutine<TRoutine>& routine : callbackRoutineList.routineList)
			{
				if (routine.callOriginalFunctionFlag)
				{
					g_ModuleInterface->Print(CM_RED, "CALL REQUEST: %s", routine.modName.c_str());
				}
				if (routine.cancelOriginalFunctionFlag)
				{
					g_ModuleInterface->Print(CM_RED, "CANCEL REQUEST: %s", routine.modName.c_str());
				}
			}
		}
		else if (callFlag || !cancelFlag)
		{
			callbackRoutineList.originalFunction(Result, Self, Other, numArgs, Args);
		}
		for (CallbackRoutine<TRoutine>& routine : callbackRoutineList.routineList)
		{
			if (routine.afterRoutine != nullptr)
			{
				routine.afterRoutine(Result, Self, Other, numArgs, Args);
			}
		}
		callOriginalFunctionFlag = prevCallOriginalFunctionFlag;
		cancelOriginalFunctionFlag = prevCancelOriginalFunctionFlag;
	}
} builtinFunctionCallbackArr[maxBuiltinFunctionCallbacks];

template<size_t index>
void builtinFunctionCallbackHelper(
	OUT RValue* Result,
	IN CInstance* Self,
	IN CInstance* Other,
	IN int numArgs,
	IN RValue* Args
)
{
	builtinFunctionCallbackArr[index].HandleBuiltinFunctionCallback(Result, Self, Other, numArgs, Args);
}

template<size_t... Is>
auto MakeBuiltinFunctionCallbackArrayHelper(std::integer_sequence<size_t, Is...>)
{
	return std::array{ builtinFunctionCallbackHelper<Is>... };
}

template<size_t size>
auto MakeBuiltinFunctionCallbackArray()
{
	return MakeBuiltinFunctionCallbackArrayHelper(std::make_index_sequence<size>{});
}

std::unordered_map<std::string, BuiltinFunctionCallbackObject*> builtinFunctionNameCallbackMap;
auto builtinFunctionCallbackObjectArr = MakeBuiltinFunctionCallbackArray<maxBuiltinFunctionCallbacks>();
int numBuiltinCallback = 0;
AurieStatus CallbackManagerInterface::RegisterBuiltinFunctionCallback(
	IN const std::string& ModName,
	IN const std::string& BuiltinFunctionName,
	IN TRoutine BeforeBuiltinFunctionRoutine,
	IN TRoutine AfterBuiltinFunctionRoutine,
	OUT TRoutine* OriginalBuiltinFunctionRoutine
)
{
	AurieStatus status = AURIE_SUCCESS;
	auto callbackList = builtinFunctionNameCallbackMap.find(BuiltinFunctionName);
	if (callbackList == builtinFunctionNameCallbackMap.end())
	{
		PVOID builtinFunction = nullptr;
		status = g_ModuleInterface->GetNamedRoutinePointer(BuiltinFunctionName.c_str(), &builtinFunction);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed obtaining function pointer for %s with status %d", BuiltinFunctionName.c_str(), status);
			return AURIE_EXTERNAL_ERROR;
		}
		if (numBuiltinCallback >= maxBuiltinFunctionCallbacks)
		{
			g_ModuleInterface->Print(CM_RED, "Failed to register %s since the number of builtin function callbacks has exceeded %d", BuiltinFunctionName.c_str(), maxBuiltinFunctionCallbacks);
			return AURIE_EXTERNAL_ERROR;
		}
		builtinFunctionNameCallbackMap[BuiltinFunctionName] = &(builtinFunctionCallbackArr[numBuiltinCallback] = BuiltinFunctionCallbackObject(CallbackRoutineList<TRoutine>(BuiltinFunctionName)));

		PVOID trampolineFunc = nullptr;
		initOrigFuncSemaphore.acquire();
		status = MmCreateHook(g_ArSelfModule, BuiltinFunctionName, builtinFunction, builtinFunctionCallbackObjectArr[numBuiltinCallback], &trampolineFunc);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed hooking %s with status %d", BuiltinFunctionName.c_str(), status);
			initOrigFuncSemaphore.release();
			return AURIE_EXTERNAL_ERROR;
		}
		builtinFunctionCallbackArr[numBuiltinCallback].callbackRoutineList.originalFunction = (TRoutine)trampolineFunc;
		initOrigFuncSemaphore.release();
		numBuiltinCallback++;
	}
	if (BeforeBuiltinFunctionRoutine != nullptr || AfterBuiltinFunctionRoutine != nullptr)
	{
		builtinFunctionNameCallbackMap[BuiltinFunctionName]->callbackRoutineList.routineList.push_back(std::move(CallbackRoutine(BeforeBuiltinFunctionRoutine, AfterBuiltinFunctionRoutine, ModName)));
	}
	if (OriginalBuiltinFunctionRoutine != nullptr)
	{
		*OriginalBuiltinFunctionRoutine = builtinFunctionNameCallbackMap[BuiltinFunctionName]->callbackRoutineList.originalFunction;
	}
	return AURIE_SUCCESS;
}

AurieStatus CallbackManagerInterface::LogToFile(
	IN const std::string& ModName,
	IN const char* LogFormat,
	...
)
{
	va_list args;
	char outputBuffer[5000];
	va_start(args, LogFormat);
	vsprintf_s(outputBuffer, LogFormat, args);
	va_end(args);
	auto time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
	std::string outputStr = outputBuffer;
	outputLog << std::format("{:%F %T} {} - {}\n", time, ModName, outputStr);
	return AURIE_SUCCESS;
}

void CallbackManagerInterface::CallOriginalFunction()
{
	callOriginalFunctionFlag = true;
}

void CallbackManagerInterface::CancelOriginalFunction()
{
	cancelOriginalFunctionFlag = true;
}