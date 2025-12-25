#pragma once

#include "CoreMinimal.h"

class UMagicScriptInterpreterSubsystem;

namespace MagicScript
{
	class FEnvironment;
	
	namespace MsMathBuiltins
	{
		// math.* 함수들을 등록
		void Register(TSharedPtr<FEnvironment> Env, UMagicScriptInterpreterSubsystem* Subsystem);
	}
}

