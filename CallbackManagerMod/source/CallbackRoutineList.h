#pragma once
#include <YYToolkit/Shared.hpp>
#include <vector>

struct CallbackRoutine
{
	PFUNC_YYGMLScript beforeRoutine;
	PFUNC_YYGMLScript afterRoutine;
	CallbackRoutine(PFUNC_YYGMLScript beforeRoutine, PFUNC_YYGMLScript afterRoutine) : beforeRoutine(beforeRoutine), afterRoutine(afterRoutine)
	{
	}
};

struct ScriptFunctionCallbackRoutineList
{
	std::vector<CallbackRoutine> routineList;
	PFUNC_YYGMLScript scriptFunctionCallbackRoutine;
	PFUNC_YYGMLScript origScriptFunction;
	ScriptFunctionCallbackRoutineList() : scriptFunctionCallbackRoutine(nullptr), origScriptFunction(nullptr)
	{
	}
};