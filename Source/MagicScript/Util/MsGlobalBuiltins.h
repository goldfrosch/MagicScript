#pragma once

#include "CoreMinimal.h"

class UMagicScriptInterpreterSubsystem;

namespace MagicScript
{
	class FEnvironment;
	
	namespace MsGlobalBuiltins
	{
		/** 전역 변수 관련 함수들을 등록 */
		void Register(TSharedPtr<FEnvironment> Env, UMagicScriptInterpreterSubsystem* Subsystem);
	}
}

