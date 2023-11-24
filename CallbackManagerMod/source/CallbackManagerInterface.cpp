#include "CallbackManagerInterface.h"
#include "ModuleMain.h"

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

AurieStatus CallbackManagerInterface::RegisterCodeEventCallback(
	IN const std::string& CodeEventName,
	IN PFUNC_YYGMLScript BeforeCodeEventRoutine,
	IN PFUNC_YYGMLScript AfterCodeEventRoutine
)
{
	return AURIE_SUCCESS;
}

AurieStatus CallbackManagerInterface::RegisterScriptFunctionCallback(
	IN const std::string& ScriptFunctionName,
	IN PFUNC_YYGMLScript BeforeScriptFunctionRoutine,
	IN PFUNC_YYGMLScript AfterScriptFunctionRoutine
)
{
	AurieStatus status = AURIE_SUCCESS;
	auto callbackList = scriptFunctionCallbackMap.find(ScriptFunctionName);
	if (callbackList == scriptFunctionCallbackMap.end())
	{
		ScriptFunctionCallbackRoutineList* callBackRoutineList = &(scriptFunctionCallbackMap[ScriptFunctionName] = ScriptFunctionCallbackRoutineList());
		int scriptIndex = g_RunnerInterface.Script_Find_Id(ScriptFunctionName.c_str()) - 100000;
		CScript* scriptPtr = nullptr;
		g_ModuleInterface->GetScriptData(scriptIndex, scriptPtr);
		PFUNC_YYGMLScript funcPtr = cify([callBackRoutineList, scriptPtr](
			IN CInstance* Self,
			IN CInstance* Other,
			OUT RValue* ReturnValue,
			IN int ArgumentCount,
			IN RValue** Arguments
			) -> RValue* {
				bool prevCallOriginalFunctionFlag = callOriginalFunctionFlag;
				bool prevCancelOriginalFunctionFlag = cancelOriginalFunctionFlag;
				callOriginalFunctionFlag = false;
				cancelOriginalFunctionFlag = false;
				for (CallbackRoutine& routine : callBackRoutineList->routineList)
				{
					if (routine.beforeRoutine != nullptr)
					{
						routine.beforeRoutine(Self, Other, ReturnValue, ArgumentCount, Arguments);
					}
				}
				RValue* ret = ReturnValue;
				if (callOriginalFunctionFlag && cancelOriginalFunctionFlag)
				{
					// TODO: Print out a list of mod names that sent the conflicting requests
					printf("ERROR: A CALL AND CANCEL REQUEST WAS SENT TO THE FUNCTION %s\n", scriptPtr->GetName());
				}
				else if (callOriginalFunctionFlag || !cancelOriginalFunctionFlag)
				{
					callBackRoutineList->origScriptFunction(Self, Other, ReturnValue, ArgumentCount, Arguments);
				}
				for (CallbackRoutine& routine : callBackRoutineList->routineList)
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
			g_ModuleInterface->Print(CM_RED, "Failed hooking %s with status %d\n", ScriptFunctionName.c_str(), status);
			return AURIE_EXTERNAL_ERROR;
		}
		callBackRoutineList->scriptFunctionCallbackRoutine = funcPtr;
		callBackRoutineList->origScriptFunction = (PFUNC_YYGMLScript)trampolineFunc;
	}
	scriptFunctionCallbackMap[ScriptFunctionName].routineList.push_back(std::move(CallbackRoutine(BeforeScriptFunctionRoutine, AfterScriptFunctionRoutine)));
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