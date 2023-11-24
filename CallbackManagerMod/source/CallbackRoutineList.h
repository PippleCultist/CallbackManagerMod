#pragma once
#include <YYToolkit/Shared.hpp>
#include <vector>

template <typename RoutineType>
struct CallbackRoutine
{
	RoutineType beforeRoutine;
	RoutineType afterRoutine;
	std::string modName;
	bool callOriginalFunctionFlag;
	bool cancelOriginalFunctionFlag;
	CallbackRoutine(RoutineType beforeRoutine, RoutineType afterRoutine, std::string modName) : beforeRoutine(beforeRoutine), afterRoutine(afterRoutine), modName(modName),
		callOriginalFunctionFlag(false), cancelOriginalFunctionFlag(false)
	{
	}
};

template <typename RoutineType>
struct CallbackRoutineList
{
	std::vector<CallbackRoutine<RoutineType>> routineList;
	RoutineType callbackRoutine;
	RoutineType originalFunction;
	CallbackRoutineList() : callbackRoutine(nullptr), originalFunction(nullptr)
	{
	}
};