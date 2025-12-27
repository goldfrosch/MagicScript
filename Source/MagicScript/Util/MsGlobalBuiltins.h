#pragma once

#include "CoreMinimal.h"

class UMagicScriptInterpreterSubsystem;

namespace MagicScript
{
	struct FValue;
	class FEnvironment;
	
	bool GetObjectParamBool(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
	float GetObjectParamFloat(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
	FVector GetObjectParamVector(const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key);
}

