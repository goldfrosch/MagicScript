#include "MagicScript/Util/MsGlobalBuiltins.h"
#include "MagicScript/Core/MsValue.h"

namespace MagicScript
{
	bool GetObjectParamBool(
		const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key)
	{
		const FValue* RowValue = Params.Get()->Find(Key);
		if (!RowValue)
		{
			return false;
		}

		if (RowValue->Type != EValueType::Bool)
		{
			return false;
		}
		
		return RowValue->Bool;
	}

	float GetObjectParamFloat(
		const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key)
	{
		const FValue* RowValue = Params.Get()->Find(Key);
		if (!RowValue)
		{
			return 0.f;
		}
		
		if (RowValue->Type != EValueType::Number)
		{
			return 0.f;
		}
		
		return RowValue->Number;
	}

	FVector GetObjectParamVector(
		const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key)
	{
		const FValue* RowValue = Params.Get()->Find(Key);
		if (!RowValue)
		{
			return FVector();
		}
		
		if (RowValue->Type != EValueType::Array)
		{
			return FVector();
		}

		const TArray<FValue> VectorArray = *RowValue->Array.Get();
		if (VectorArray.Num() < 3)
		{
			return FVector();
		}

		const float X = VectorArray[0].Number;
		const float Y = VectorArray[1].Number;
		const float Z = VectorArray[2].Number;
		
		return FVector(X, Y, Z);
	}
	
	void SetObjectParamToVector(const TSharedPtr<TMap<FString, FValue>>& Params, const FVector& Value)
	{
		Params.Get()->Add("x", FValue::FromNumber(Value.X));
		Params.Get()->Add("y", FValue::FromNumber(Value.Y));
		Params.Get()->Add("z", FValue::FromNumber(Value.Z));
	}
}


