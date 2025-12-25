#include "MagicScript/Core/MsEnvironment.h"

namespace MagicScript
{
	bool FEnvironment::Define(const FString& Name, const FValue& Value, bool bIsConst)
	{
		if (Table.Contains(Name))
		{
			// 이미 존재: 재정의 불가 (간단 처리)
			return false;
		}

		FEntry Entry;
		Entry.Value = Value;
		Entry.bIsConst = bIsConst;
		Table.Add(Name, Entry);
		return true;
	}

	bool FEnvironment::Assign(const FString& Name, const FValue& Value)
	{
		FEntry* Entry = Lookup(Name);
		if (!Entry || Entry->bIsConst)
		{
			return false;
		}

		Entry->Value = Value;
		return true;
	}

	FEnvironment::FEntry* FEnvironment::Lookup(const FString& Name)
	{
		if (FEntry* Found = Table.Find(Name))
		{
			return Found;
		}

		if (Parent.IsValid())
		{
			return Parent->Lookup(Name);
		}

		return nullptr;
	}

	TSharedPtr<FEnvironment> FEnvironment::Clone() const
	{
		// Parent도 복사 (재귀적으로 전체 체인 복사)
		TSharedPtr<FEnvironment> ClonedParent = nullptr;
		if (Parent.IsValid())
		{
			ClonedParent = Parent->Clone();
		}

		// 새 Environment 생성
		TSharedPtr<FEnvironment> Cloned = MakeShared<FEnvironment>(ClonedParent);
		
		// Table 복사 (TMap은 값 복사이므로 자동으로 깊은 복사됨)
		Cloned->Table = Table;
		
		return Cloned;
	}
}

