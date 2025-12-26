#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsToken.h"

namespace MagicScript
{
	/**
	 * .ms 스크립트용 간단 렉서
	 * - 공백/주석 제거
	 * - 토큰 시퀀스 생성
	 */
	class MAGICSCRIPT_API FLexer
	{
	public:
		explicit FLexer(const FString& InSource);

		TArray<FToken> Tokenize();

	private:
		FString Source;
		int32 Index = 0;
		FSourceLocation Location;

		bool IsAtEnd() const;
		TCHAR Peek() const;
		TCHAR Advance();
		bool Match(TCHAR Expected);

		void SkipWhitespace();

		static FToken MakeToken(ETokenType Type, const FString& Lexeme, const FSourceLocation& StartLocation);
		static FToken MakeErrorToken(const FString& Message, const FSourceLocation& StartLocation);

		FToken LexIdentifierOrKeyword();
		FToken LexNumber();
		FToken LexString();
		FToken LexComment();
		FToken LexOperatorOrSeparator();
	};
}

