#include "CallbackManagerInterface.h"
#include "ModuleMain.h"
#include <YYToolkit/shared.hpp>

using namespace Aurie;
using namespace YYTK;

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

// Copied from https://stackoverflow.com/questions/7852101/c-lambda-with-captures-as-a-function-pointer

template <class F>
struct lambda_traits : lambda_traits<decltype(&F::operator())>
{ };

template <typename F, typename R, typename... Args>
struct lambda_traits<R(F::*)(Args...)> : lambda_traits<R(F::*)(Args...) const>
{ };

template <class F, class R, class... Args>
struct lambda_traits<R(F::*)(Args...) const> {
	using pointer = typename std::add_pointer<R(Args...)>::type;

	static pointer cify(F&& f) {
		static F fn = std::forward<F>(f);
		return [](Args... args) {
			return fn(std::forward<Args>(args)...);
		};
	}
};

template <class F>
inline typename lambda_traits<F>::pointer cify(F&& f) {
	return lambda_traits<F>::cify(std::forward<F>(f));
}

bool callOriginalFunctionFlag = false;
bool cancelOriginalFunctionFlag = false;

std::unordered_map<int, CallbackRoutineList<CodeEvent>*> codeIndexToCallbackMap;
std::unordered_map<std::string, CallbackRoutineList<CodeEvent>> codeEventCallbackMap;

void CodeCallback(FWCodeEvent& CodeContext)
{
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
		if (codeEventCallbackMap.find(Code->GetName()) == codeEventCallbackMap.end())
		{
			callbackRoutineList = &(codeEventCallbackMap[Code->GetName()] = CallbackRoutineList<CodeEvent>());
		}
		else
		{
			callbackRoutineList = &codeEventCallbackMap[Code->GetName()];
		}
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
		status = g_ModuleInterface->CreateCallback(g_AurieModule, EVENT_OBJECT_CALL, CodeCallback, 0);

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
	codeEventCallbackMap[CodeEventName].routineList.push_back(std::move(CallbackRoutine(BeforeCodeEventRoutine, AfterCodeEventRoutine, ModName)));
	return AURIE_SUCCESS;
}

std::unordered_map<std::string, CallbackRoutineList<PFUNC_YYGMLScript>> scriptFunctionCallbackMap;

AurieStatus CallbackManagerInterface::RegisterScriptFunctionCallback(
	IN const std::string& ModName,
	IN const std::string& ScriptFunctionName,
	IN PFUNC_YYGMLScript BeforeScriptFunctionRoutine,
	IN PFUNC_YYGMLScript AfterScriptFunctionRoutine
)
{
	AurieStatus status = AURIE_SUCCESS;
	auto callbackList = scriptFunctionCallbackMap.find(ScriptFunctionName);
	if (callbackList == scriptFunctionCallbackMap.end())
	{
		CallbackRoutineList<PFUNC_YYGMLScript>* callbackRoutineList = &(scriptFunctionCallbackMap[ScriptFunctionName] = CallbackRoutineList<PFUNC_YYGMLScript>());
		int scriptIndex = g_RunnerInterface.Script_Find_Id(ScriptFunctionName.c_str()) - 100000;
		CScript* scriptPtr = nullptr;
		status = g_ModuleInterface->GetScriptData(scriptIndex, scriptPtr);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed obtaining script function data for %s at script index %d with status %d", ScriptFunctionName.c_str(), scriptIndex, status);
			return AURIE_EXTERNAL_ERROR;
		}
		PFUNC_YYGMLScript funcPtr = cify([callbackRoutineList, scriptPtr](
			IN CInstance* Self,
			IN CInstance* Other,
			OUT RValue* ReturnValue,
			IN int ArgumentCount,
			IN RValue** Arguments
			) -> RValue* {
				bool prevCallOriginalFunctionFlag = callOriginalFunctionFlag;
				bool prevCancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
				bool callFlag = false;
				bool cancelFlag = false;
				callOriginalFunctionFlag = false;
				cancelOriginalFunctionFlag = false;
				for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList->routineList)
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
				RValue* ret = ReturnValue;
				if (callFlag && cancelFlag)
				{
					g_ModuleInterface->Print(CM_RED, "ERROR: A CALL AND CANCEL REQUEST WAS SENT TO THE FUNCTION %s", scriptPtr->GetName());
					for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList->routineList)
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
					callbackRoutineList->originalFunction(Self, Other, ReturnValue, ArgumentCount, Arguments);
				}
				for (CallbackRoutine<PFUNC_YYGMLScript>& routine : callbackRoutineList->routineList)
				{
					if (routine.afterRoutine != nullptr)
					{
						routine.afterRoutine(Self, Other, ReturnValue, ArgumentCount, Arguments);
					}
				}
				callOriginalFunctionFlag = prevCallOriginalFunctionFlag;
				cancelOriginalFunctionFlag = prevCancelOriginalFunctionFlag;
				return ret;
			}
		);
		
		PVOID trampolineFunc = nullptr;
		status = MmCreateHook(g_AurieModule, ScriptFunctionName, scriptPtr->m_Functions->m_ScriptFunction, funcPtr, &trampolineFunc);
		if (!AurieSuccess(status))
		{
			g_ModuleInterface->Print(CM_RED, "Failed hooking %s with status %d", ScriptFunctionName.c_str(), status);
			return AURIE_EXTERNAL_ERROR;
		}
		callbackRoutineList->callbackRoutine = funcPtr;
		callbackRoutineList->originalFunction = (PFUNC_YYGMLScript)trampolineFunc;
	}
	scriptFunctionCallbackMap[ScriptFunctionName].routineList.push_back(std::move(CallbackRoutine(BeforeScriptFunctionRoutine, AfterScriptFunctionRoutine, ModName)));
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