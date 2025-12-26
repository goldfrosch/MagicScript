#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsValue.h"

namespace MagicScript
{
	// 스코프 단위 즉 렉시컬 환경을 의미함.
	class MAGICSCRIPT_API FEnvironment : public TSharedFromThis<FEnvironment>
	{
	public:
		explicit FEnvironment(const TSharedPtr<FEnvironment>& InParent = nullptr)
			: Parent(InParent)
		{
		}

		struct FEntry
		{
			FValue Value;
			bool   bIsConst = false;
		};

		bool Define(const FString& Name, const FValue& Value, bool bIsConst);
		bool Assign(const FString& Name, const FValue& Value);
		FEntry* Lookup(const FString& Name);

		// Environment의 깊은 복사본 생성 - 스냅샷 전용 로직
		TSharedPtr<FEnvironment> Clone() const;

	private:
		TSharedPtr<FEnvironment> Parent;
		TMap<FString, FEntry>    Table;
	};
}

