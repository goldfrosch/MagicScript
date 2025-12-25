#include "MagicScript/Core/MsLexer.h"

namespace MagicScript
{
	FLexer::FLexer(const FString& InSource)
		: Source(InSource)
	{
		Location.Line = 1;
		Location.Column = 1;
	}

	bool FLexer::IsAtEnd() const
	{
		return Index >= Source.Len();
	}

	TCHAR FLexer::Peek() const
	{
		return IsAtEnd() ? TEXT('\0') : Source[Index];
	}

	TCHAR FLexer::Advance()
	{
		if (IsAtEnd())
		{
			return TEXT('\0');
		}

		TCHAR C = Source[Index++];
		if (C == TEXT('\n'))
		{
			Location.Line++;
			Location.Column = 1;
		}
		else
		{
			Location.Column++;
		}
		return C;
	}

	bool FLexer::Match(TCHAR Expected)
	{
		if (IsAtEnd() || Source[Index] != Expected)
		{
			return false;
		}
		Advance();
		return true;
	}

	void FLexer::SkipWhitespace()
	{
		for (;;)
		{
			TCHAR C = Peek();
			if (C == TEXT(' ') || C == TEXT('\t') || C == TEXT('\r') || C == TEXT('\n'))
			{
				Advance();
				continue;
			}
			break;
		}
	}

	FToken FLexer::LexComment()
	{
		FSourceLocation StartLoc = Location;
		int32 StartIndex = Index - 1; // / 문자 포함

		// 한 줄 주석 //
		if (Peek() == TEXT('/'))
		{
			Advance(); // 두 번째 /
			while (!IsAtEnd() && Peek() != TEXT('\n'))
			{
				Advance();
			}
			// \n은 포함하지 않음 (다음 토큰에서 처리)
		}
		// 블록 주석 /* ... */
		else if (Peek() == TEXT('*'))
		{
			Advance(); // *
			while (!IsAtEnd())
			{
				if (Peek() == TEXT('*') && Index + 1 < Source.Len() && Source[Index + 1] == TEXT('/'))
				{
					Advance(); // *
					Advance(); // /
					break;
				}
				Advance();
			}
		}

		FString Text = Source.Mid(StartIndex, Index - StartIndex);
		return MakeToken(ETokenType::Comment, Text, StartLoc);
	}

	FToken FLexer::MakeToken(ETokenType Type, const FString& Lexeme, const FSourceLocation& StartLocation)
	{
		return FToken(Type, Lexeme, StartLocation);
	}

	FToken FLexer::MakeErrorToken(const FString& Message, const FSourceLocation& StartLocation)
	{
		return FToken(ETokenType::Error, Message, StartLocation);
	}

	FToken FLexer::LexIdentifierOrKeyword()
	{
		const FSourceLocation StartLoc = Location;
		const int32 StartIndex = Index - 1; // 첫 글자는 이미 읽음

		auto IsIdentChar = [](TCHAR Ch)
		{
			return FChar::IsAlpha(Ch) || FChar::IsDigit(Ch) || Ch == TEXT('_');
		};

		while (!IsAtEnd() && IsIdentChar(Peek()))
		{
			Advance();
		}

		const FString Text = Source.Mid(StartIndex, Index - StartIndex);

		if (Text == TEXT("let"))      return MakeToken(ETokenType::Let, Text, StartLoc);
		if (Text == TEXT("const"))    return MakeToken(ETokenType::Const, Text, StartLoc);
		if (Text == TEXT("spell"))    return MakeToken(ETokenType::Function, Text, StartLoc);
		if (Text == TEXT("import"))   return MakeToken(ETokenType::Import, Text, StartLoc);
		if (Text == TEXT("if"))       return MakeToken(ETokenType::If, Text, StartLoc);
		if (Text == TEXT("else"))     return MakeToken(ETokenType::Else, Text, StartLoc);
		if (Text == TEXT("switch"))   return MakeToken(ETokenType::Switch, Text, StartLoc);
		if (Text == TEXT("case"))     return MakeToken(ETokenType::Case, Text, StartLoc);
		if (Text == TEXT("default"))  return MakeToken(ETokenType::Default, Text, StartLoc);
		if (Text == TEXT("for"))      return MakeToken(ETokenType::For, Text, StartLoc);
		if (Text == TEXT("while"))    return MakeToken(ETokenType::While, Text, StartLoc);
		if (Text == TEXT("return"))   return MakeToken(ETokenType::Return, Text, StartLoc);
		if (Text == TEXT("true"))     return MakeToken(ETokenType::True, Text, StartLoc);
		if (Text == TEXT("false"))    return MakeToken(ETokenType::False, Text, StartLoc);
		if (Text == TEXT("null"))     return MakeToken(ETokenType::Null, Text, StartLoc);

		return MakeToken(ETokenType::Identifier, Text, StartLoc);
	}

	FToken FLexer::LexNumber()
	{
		const FSourceLocation StartLoc = Location;
		const int32 StartIndex = Index - 1;

		while (!IsAtEnd() && FChar::IsDigit(Peek()))
		{
			Advance();
		}

		// 소수점
		if (!IsAtEnd() && Peek() == TEXT('.'))
		{
			Advance();
			while (!IsAtEnd() && FChar::IsDigit(Peek()))
			{
				Advance();
			}
		}

		FString Text = Source.Mid(StartIndex, Index - StartIndex);
		return MakeToken(ETokenType::Number, Text, StartLoc);
	}

	FToken FLexer::LexString()
	{
		const FSourceLocation StartLoc = Location;
		const int32 StartIndex = Index; // " 뒤부터

		while (!IsAtEnd() && Peek() != TEXT('"'))
		{
			TCHAR C = Advance();
			if (C == TEXT('\\') && !IsAtEnd())
			{
				// 단순히 다음 글자 하나를 스킵 (구체 escape 처리는 나중에)
				Advance();
			}
		}

		if (IsAtEnd())
		{
			return MakeErrorToken(TEXT("Unterminated string literal"), StartLoc);
		}

		// 닫는 따옴표 소비
		Advance();

		FString Raw = Source.Mid(StartIndex, Index - StartIndex - 1); // 마지막 " 제외
		return MakeToken(ETokenType::String, Raw, StartLoc);
	}

	FToken FLexer::LexOperatorOrSeparator()
	{
		const FSourceLocation StartLoc = Location;
		const TCHAR C = Advance();

		switch (C)
		{
		case TEXT('+'):
			if (Match(TEXT('+'))) return MakeToken(ETokenType::PlusPlus, TEXT("++"), StartLoc);
			if (Match(TEXT('='))) return MakeToken(ETokenType::PlusEqual, TEXT("+="), StartLoc);
			return MakeToken(ETokenType::Plus, TEXT("+"), StartLoc);
		case TEXT('-'):
			if (Match(TEXT('-'))) return MakeToken(ETokenType::MinusMinus, TEXT("--"), StartLoc);
			if (Match(TEXT('='))) return MakeToken(ETokenType::MinusEqual, TEXT("-="), StartLoc);
			return MakeToken(ETokenType::Minus, TEXT("-"), StartLoc);
		case TEXT('*'):
			if (Match(TEXT('='))) return MakeToken(ETokenType::StarEqual, TEXT("*="), StartLoc);
			return MakeToken(ETokenType::Star, TEXT("*"), StartLoc);
		case TEXT('%'):
			if (Match(TEXT('='))) return MakeToken(ETokenType::PercentEqual, TEXT("%="), StartLoc);
			return MakeToken(ETokenType::Percent, TEXT("%"), StartLoc);

		case TEXT('('): return MakeToken(ETokenType::LParen, TEXT("("), StartLoc);
		case TEXT(')'): return MakeToken(ETokenType::RParen, TEXT(")"), StartLoc);
		case TEXT('{'): return MakeToken(ETokenType::LBrace, TEXT("{"), StartLoc);
		case TEXT('}'): return MakeToken(ETokenType::RBrace, TEXT("}"), StartLoc);
		case TEXT('['): return MakeToken(ETokenType::LBracket, TEXT("["), StartLoc);
		case TEXT(']'): return MakeToken(ETokenType::RBracket, TEXT("]"), StartLoc);
		case TEXT('.'): return MakeToken(ETokenType::Dot, TEXT("."), StartLoc);
		case TEXT(','): return MakeToken(ETokenType::Comma, TEXT(","), StartLoc);
		case TEXT(';'): return MakeToken(ETokenType::Semicolon, TEXT(";"), StartLoc);
		case TEXT(':'): return MakeToken(ETokenType::Colon, TEXT(":"), StartLoc);

		case TEXT('!'):
			if (Match(TEXT('='))) return MakeToken(ETokenType::BangEqual, TEXT("!="), StartLoc);
			return MakeToken(ETokenType::Bang, TEXT("!"), StartLoc);

		case TEXT('='):
			if (Match(TEXT('='))) return MakeToken(ETokenType::EqualEqual, TEXT("=="), StartLoc);
			if (Match(TEXT('>'))) return MakeToken(ETokenType::Arrow, TEXT("=>"), StartLoc);
			return MakeToken(ETokenType::Equal, TEXT("="), StartLoc);

		case TEXT('<'):
			if (Match(TEXT('='))) return MakeToken(ETokenType::LessEqual, TEXT("<="), StartLoc);
			return MakeToken(ETokenType::Less, TEXT("<"), StartLoc);

		case TEXT('>'):
			if (Match(TEXT('='))) return MakeToken(ETokenType::GreaterEqual, TEXT(">="), StartLoc);
			return MakeToken(ETokenType::Greater, TEXT(">"), StartLoc);

		case TEXT('/'):
			// 여기까지 왔다는 것은 주석이 아니라 실제 / 토큰
			if (Match(TEXT('='))) return MakeToken(ETokenType::SlashEqual, TEXT("/="), StartLoc);
			return MakeToken(ETokenType::Slash, TEXT("/"), StartLoc);

		case TEXT('&'):
			if (Match(TEXT('&')))
			{
				return MakeToken(ETokenType::AndAnd, TEXT("&&"), StartLoc);
			}
			break;

		case TEXT('|'):
			if (Match(TEXT('|')))
			{
				return MakeToken(ETokenType::OrOr, TEXT("||"), StartLoc);
			}
			break;

		default:
			break;
		}

		const FString Msg = FString::Printf(TEXT("Unexpected character '%c'"), C);
		return MakeErrorToken(Msg, StartLoc);
	}

	TArray<FToken> FLexer::Tokenize()
	{
		TArray<FToken> Result;
		Result.Reserve(128);

		while (!IsAtEnd())
		{
			SkipWhitespace();
			if (IsAtEnd())
			{
				break;
			}

			const FSourceLocation StartLoc = Location;
			TCHAR C = Peek();

			// 주석 체크 (공백 스킵 후)
			if (C == TEXT('/') && Index + 1 < Source.Len())
			{
				TCHAR NextC = Source[Index + 1];
				if (NextC == TEXT('/') || NextC == TEXT('*'))
				{
					Advance(); // /
					Result.Add(LexComment());
					continue;
				}
			}

			// 주석이 아니면 일반 토큰 처리
			C = Advance();

			if (FChar::IsAlpha(C) || C == TEXT('_'))
			{
				// LexIdentifierOrKeyword는 첫 글자를 이미 읽었다고 가정하므로
				Index--; // 한 글자 되돌리기
				Location = StartLoc;
				Advance(); // 다시 첫 글자 소비
				Result.Add(LexIdentifierOrKeyword());
				continue;
			}

			if (FChar::IsDigit(C))
			{
				Index--;
				Location = StartLoc;
				Advance();
				Result.Add(LexNumber());
				continue;
			}

			if (C == TEXT('"'))
			{
				Result.Add(LexString());
				continue;
			}

			Index--;
			Location = StartLoc;
			Result.Add(LexOperatorOrSeparator());
		}

		Result.Add(FToken(ETokenType::EndOfFile, TEXT(""), Location));
		return Result;
	}
}

