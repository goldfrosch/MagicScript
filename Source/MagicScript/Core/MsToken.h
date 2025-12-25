#pragma once

#include "CoreMinimal.h"

namespace MagicScript
{
	/** 토큰 종류 */
	enum class ETokenType : uint8
	{
		// 특별
		EndOfFile,
		Error,

		// 리터럴 / 식별자
		Identifier,
		Number,
		String,
		Comment,    // 주석 (// 또는 /* */)

		// 키워드
		Let,
		Const,
		Function,
		Import,
		If,
		Else,
		Switch,
		Case,
		Default,
		For,
		While,
		Return,
		True,
		False,
		Null,

		// 연산자 / 구분자
		Plus,       // +
		Minus,      // -
		Star,       // *
		Slash,      // /
		Percent,    // %
		PlusPlus,   // ++
		MinusMinus, // --

		Equal,      // =
		EqualEqual, // ==
		PlusEqual,  // +=
		MinusEqual, // -=
		StarEqual,  // *=
		SlashEqual, // /=
		PercentEqual, // %=
		Bang,       // !
		BangEqual,  // !=

		Less,       // <
		LessEqual,  // <=
		Greater,    // >
		GreaterEqual,// >=

		AndAnd,     // &&
		OrOr,       // ||

		LParen,     // (
		RParen,     // )
		LBrace,     // {
		RBrace,     // }
		LBracket,   // [
		RBracket,   // ]
		Dot,        // .
		Comma,      // ,
		Semicolon,  // ;
		Colon,      // :
		Arrow       // =>
	};

	/** 소스 코드 상의 위치 정보 */
	struct FSourceLocation
	{
		int32 Line = 1;
		int32 Column = 1;
	};

	/** 렉서 결과 토큰 */
	struct FToken
	{
		ETokenType Type = ETokenType::Error;
		FString    Lexeme;
		FSourceLocation Location;

		FToken() = default;

		FToken(ETokenType InType, const FString& InLexeme, const FSourceLocation& InLocation)
			: Type(InType)
			, Lexeme(InLexeme)
			, Location(InLocation)
		{
		}

		FString ToString() const
		{
			return Lexeme;
		}
	};
}

