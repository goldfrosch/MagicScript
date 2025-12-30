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

	FVector2D GetObjectParamVector(
		const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key)
	{
		const FValue* RowValue = Params.Get()->Find(Key);
		if (!RowValue)
		{
			return FVector2D();
		}
		
		if (RowValue->Type != EValueType::Array)
		{
			return FVector2D();
		}

		const TArray<FValue> VectorArray = *RowValue->Array.Get();
		if (VectorArray.Num() < 2)
		{
			return FVector2D();
		}

		const float X = VectorArray[0].Number;
		const float Y = VectorArray[1].Number;
		
		return FVector2D(X, Y);
	}

	UObject* GetObjectParamNativeObject(
		const TSharedPtr<TMap<FString, FValue>>& Params, const FString& Key)
	{
		const FValue* RowValue = Params.Get()->Find(Key);
		if (!RowValue)
		{
			return nullptr;
		}

		if (!RowValue->NativeObjectPtr.IsValid())
		{
			return nullptr;
		}

		return RowValue->NativeObjectPtr.Get();
	}

	void SetObjectParamToVector(const TSharedPtr<TMap<FString, FValue>>& Params, const FVector& Value)
	{
		Params.Get()->Add("x", FValue::FromNumber(Value.X));
		Params.Get()->Add("y", FValue::FromNumber(Value.Y));
		Params.Get()->Add("z", FValue::FromNumber(Value.Z));
	}
}


