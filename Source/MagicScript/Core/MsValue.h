#pragma once

#include "CoreMinimal.h"

namespace MagicScript
{
	class FEnvironment;
	struct FStatement;
	struct FExpression;
	struct FProgram;
	struct FValue;
	struct FScriptExecutionContext;

	using FStatementPtr = TSharedPtr<FStatement>;
	using FExpressionPtr = TSharedPtr<FExpression>;

	// 값 타입
	enum class EValueType : uint8
	{
		Null,
		Number,
		String,
		Bool,
		Function,
		Array,
		Object,
		NativeObject
	};

	// 런타임 함수 표현 (AST + 환경)
	struct MAGICSCRIPT_API FFunctionValue
	{
		FString Name;
		TArray<FString> Parameters;
		TSharedPtr<FStatement> Body;
		TSharedPtr<FEnvironment> Closure;

		// 네이티브(C++) 함수 여부 및 구현
		bool bIsNative = false;
		TFunction<FValue(const TArray<FValue>&, const FScriptExecutionContext&)> NativeImpl;

		int32 SpaceCostBytes = 0;
		int32 TimeComplexityAdditionalScore = 0;
	};

	// 인터프리터 런타임 값
	struct MAGICSCRIPT_API FValue
	{
		EValueType Type = EValueType::Null;
		double     Number = 0.0;
		bool       Bool = false;
		FString    String;
		TSharedPtr<FFunctionValue> Function;
		TSharedPtr<TArray<FValue>> Array;
		TSharedPtr<TMap<FString, FValue>> Object;  // 객체: 키-값 쌍
		TWeakObjectPtr<> NativeObjectPtr;

		static FValue Null()
		{
			return FValue();
		}

		static FValue FromNumber(double InNumber)
		{
			FValue V;
			V.Type = EValueType::Number;
			V.Number = InNumber;
			return V;
		}

		static FValue FromBool(bool bInBool)
		{
			FValue V;
			V.Type = EValueType::Bool;
			V.Bool = bInBool;
			return V;
		}

		static FValue FromString(const FString& InString)
		{
			FValue V;
			V.Type = EValueType::String;
			V.String = InString;
			return V;
		}

		static FValue FromFunction(const TSharedPtr<FFunctionValue>& InFunc)
		{
			FValue V;
			V.Type = EValueType::Function;
			V.Function = InFunc;
			return V;
		}

		static FValue FromArray(const TSharedPtr<TArray<FValue>>& InArray)
		{
			FValue V;
			V.Type = EValueType::Array;
			V.Array = InArray;
			return V;
		}

		static FValue FromObject(const TSharedPtr<TMap<FString, FValue>>& InObject)
		{
			FValue V;
			V.Type = EValueType::Object;
			V.Object = InObject;
			return V;
		}

		static FValue FromNativeObject(UObject* InObject)
		{
			FValue V;
			V.Type = EValueType::NativeObject;
			V.NativeObjectPtr = InObject;

			return V;
		}

		FString ToDebugString() const
		{
			switch (Type)
			{
			case EValueType::Null:   return TEXT("null");
			case EValueType::Number: return FString::SanitizeFloat(Number);
			case EValueType::Bool:   return Bool ? TEXT("true") : TEXT("false");
			case EValueType::String: return FString::Printf(TEXT("\"%s\""), *String);
			case EValueType::Function: return FString::Printf(TEXT("<spell %s>"), *Function->Name);
			case EValueType::Array:
			{
				if (!Array.IsValid())
				{
					return TEXT("[]");
				}
				FString Result = TEXT("[");
				for (int32 i = 0; i < Array->Num(); ++i)
				{
					if (i > 0)
					{
						Result += TEXT(", ");
					}
					Result += Array->operator[](i).ToDebugString();
				}
				Result += TEXT("]");
				return Result;
			}
			case EValueType::Object:
			{
				if (!Object.IsValid())
				{
					return TEXT("{}");
				}
				FString Result = TEXT("{ ");
				bool bFirst = true;
				for (const auto& Pair : *Object)
				{
					if (!bFirst)
					{
						Result += TEXT(", ");
					}
					bFirst = false;
					Result += FString::Printf(TEXT("%s: %s"), *Pair.Key, *Pair.Value.ToDebugString());
				}
				Result += TEXT(" }");
				return Result;
			}
			case EValueType::NativeObject: return FString::Printf(TEXT("%s"), *NativeObjectPtr.Get()->GetName());
			default: return TEXT("<unknown>");
			}
		}
	};
}

