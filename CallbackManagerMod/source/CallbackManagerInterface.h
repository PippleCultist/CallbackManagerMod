#pragma once

#include <YYToolkit/shared.hpp>
using namespace Aurie;
using namespace YYTK;

struct CallbackManagerInterface : AurieInterfaceBase
{
	virtual AurieStatus Create();
	virtual void Destroy();
	virtual void QueryVersion(
		OUT short& Major,
		OUT short& Minor,
		OUT short& Patch
	);

	/*
	* Call this to register a routine that will run before the code event happens and another routine that will run after.
	* This will run the code event after all before routines run and before any after routines run.
	*/
	virtual AurieStatus RegisterCodeEventCallback(
		IN const std::string& CodeEventName,
		IN PFUNC_YYGMLScript BeforeCodeEventRoutine,
		IN PFUNC_YYGMLScript AfterCodeEventRoutine
	);

	/*
	* Call this to register a routine that will run before the script function happens and another routine that will run after.
	* This will run the script function after all before routines run and before any after routines run.
	*/
	virtual AurieStatus RegisterScriptFunctionCallback(
		IN const std::string& ScriptFunctionName,
		IN PFUNC_YYGMLScript BeforeScriptFunctionRoutine,
		IN PFUNC_YYGMLScript AfterScriptFunctionRoutine
	);

	/*
	* Will only have an effect inside the before routine that was registered with the callback.
	* Tells the callback manager that a mod wants the original function to be called.
	* WARNING: WILL HAVE UNDEFINED BEHAVIOR IF BOTH CALL AND CANCEL OCCUR
	*/
	virtual void CallOriginalFunction();

	/*
	* Will only have an effect inside the before routine that was registered with the callback.
	* Tells the callback manager that a mod wants the original function to be cancelled.
	* WARNING: WILL HAVE UNDEFINED BEHAVIOR IF BOTH CALL AND CANCEL OCCUR
	*/
	virtual void CancelOriginalFunction();
};