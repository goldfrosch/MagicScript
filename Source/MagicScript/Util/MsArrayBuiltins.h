#pragma once

#include "CoreMinimal.h"

class UMagicScriptInterpreterSubsystem;

namespace MagicScript
{
	class FEnvironment;
	
	namespace MsArrayBuiltins
	{
		/** Array.* 메서드들을 등록 */
		void Register(TSharedPtr<FEnvironment> Env, UMagicScriptInterpreterSubsystem* Subsystem);
	}
}

