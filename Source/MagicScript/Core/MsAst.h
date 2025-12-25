#pragma once

#include "CoreMinimal.h"
#include "MagicScript/Core/MsToken.h"

namespace MagicScript
{
	struct FStatement;
	struct FExpression;

	using FStatementPtr = TSharedPtr<FStatement>;
	using FExpressionPtr = TSharedPtr<FExpression>;

	struct FProgram
	{
		TArray<FStatementPtr> Statements;
	};

	enum class EStatementKind : uint8
	{
		Block,
		VarDecl,
		FuncDecl,
		Import,
		If,
		Switch,
		While,
		For,
		Return,
		Expr
	};

	enum class EExpressionKind : uint8
	{
		Binary,
		Unary,
		Literal,
		Identifier,
		Assignment,
		Call,
		Grouping,
		ArrayLiteral,
		ObjectLiteral,
		Index,
		MemberAccess,
		ArrowFunction,
		PostfixIncrement,  // x++
		PostfixDecrement   // x--
	};

	struct FStatement
	{
		virtual ~FStatement() = default;
		EStatementKind Kind;

	protected:
		explicit FStatement(EStatementKind InKind)
			: Kind(InKind)
		{
		}
	};

	struct FExpression
	{
		virtual ~FExpression() = default;
		EExpressionKind Kind;

	protected:
		explicit FExpression(EExpressionKind InKind)
			: Kind(InKind)
		{
		}
	};

	// === 문장 노드 ===
	struct FBlockStatement : FStatement
	{
		TArray<FStatementPtr> Statements;

		FBlockStatement()
			: FStatement(EStatementKind::Block)
		{
		}
	};

	struct FVarDeclStatement : FStatement
	{
		bool bIsConst = false;
		FString Name;
		FExpressionPtr Initializer; // null 허용

		FVarDeclStatement()
			: FStatement(EStatementKind::VarDecl)
		{
		}
	};

	struct FFuncDeclStatement : FStatement
	{
		FString Name;
		TArray<FString> Parameters;
		TSharedPtr<FBlockStatement> Body;

		FFuncDeclStatement()
			: FStatement(EStatementKind::FuncDecl)
		{
		}
	};

	struct FImportStatement : FStatement
	{
		FString Path; // ex: Scripts/Util.ms

		FImportStatement()
			: FStatement(EStatementKind::Import)
		{
		}
	};

	struct FIfStatement : FStatement
	{
		FExpressionPtr Condition;
		FStatementPtr ThenBranch;
		FStatementPtr ElseBranch; // else는 null일 수 있음

		FIfStatement()
			: FStatement(EStatementKind::If)
		{
		}
	};

	struct FSwitchCase
	{
		FExpressionPtr Value;  // case 값 (null이면 default)
		TArray<FStatementPtr> Statements;
	};

	struct FSwitchStatement : FStatement
	{
		FExpressionPtr Expression;  // switch (expression)
		TArray<FSwitchCase> Cases;   // case/default 목록

		FSwitchStatement()
			: FStatement(EStatementKind::Switch)
		{
		}
	};

	struct FWhileStatement : FStatement
	{
		FExpressionPtr Condition;
		FStatementPtr Body;

		FWhileStatement()
			: FStatement(EStatementKind::While)
		{
		}
	};

	struct FForStatement : FStatement
	{
		FStatementPtr Init;        // VarDecl 또는 ExprStmt, null 허용
		FExpressionPtr Condition;  // null -> true
		FExpressionPtr Increment;  // null 허용
		FStatementPtr Body;

		FForStatement()
			: FStatement(EStatementKind::For)
		{
		}
	};

	struct FReturnStatement : FStatement
	{
		FExpressionPtr Value; // null 허용

		FReturnStatement()
			: FStatement(EStatementKind::Return)
		{
		}
	};

	struct FExpressionStatement : FStatement
	{
		FExpressionPtr Expr;

		FExpressionStatement()
			: FStatement(EStatementKind::Expr)
		{
		}
	};

	// 표현식 노드 enum 값
	enum class EBinaryOp : uint8
	{
		Add, Sub, Mul, Div, Mod,
		Equal, NotEqual,
		Less, LessEqual,
		Greater, GreaterEqual,
		And, Or
	};

	enum class EUnaryOp : uint8
	{
		Negate,
		Not,
		PreIncrement,   // ++x
		PreDecrement    // --x
	};

	enum class EAssignmentOp : uint8
	{
		Assign,     // =
		AddAssign,  // +=
		SubAssign,  // -=
		MulAssign,  // *=
		DivAssign,  // /=
		ModAssign   // %=
	};

	struct FBinaryExpression : FExpression
	{
		EBinaryOp Op;
		FExpressionPtr Left;
		FExpressionPtr Right;

		FBinaryExpression()
			: FExpression(EExpressionKind::Binary)
		{
		}
	};

	struct FUnaryExpression : FExpression
	{
		EUnaryOp Op;
		FExpressionPtr Operand;

		FUnaryExpression()
			: FExpression(EExpressionKind::Unary)
		{
		}
	};

	struct FLiteralExpression : FExpression
	{
		// 실제 값은 인터프리터 단계에서 MsValue로 변환
		FToken LiteralToken;

		FLiteralExpression()
			: FExpression(EExpressionKind::Literal)
		{
		}
	};

	struct FIdentifierExpression : FExpression
	{
		FString Name;

		FIdentifierExpression()
			: FExpression(EExpressionKind::Identifier)
		{
		}
	};

	struct FAssignmentExpression : FExpression
	{
		EAssignmentOp Op = EAssignmentOp::Assign;
		FString TargetName;
		FExpressionPtr Value;
		FExpressionPtr Index;  // 배열 인덱싱 할당을 위한 인덱스 표현식 (null이면 일반 변수 할당)
		FString MemberName;    // 객체 멤버 할당을 위한 멤버 이름 (빈 문자열이면 일반 변수 할당)

		FAssignmentExpression()
			: FExpression(EExpressionKind::Assignment)
		{
		}
	};

	struct FCallExpression : FExpression
	{
		FString CalleeName;
		TArray<FExpressionPtr> Arguments;
		FExpressionPtr ThisValue;  // 멤버 메서드 호출을 위한 this 값 (배열 등)

		FCallExpression()
			: FExpression(EExpressionKind::Call)
		{
		}
	};

	struct FGroupingExpression : FExpression
	{
		FExpressionPtr Inner;

		FGroupingExpression()
			: FExpression(EExpressionKind::Grouping)
		{
		}
	};

	struct FArrayLiteralExpression : FExpression
	{
		TArray<FExpressionPtr> Elements;

		FArrayLiteralExpression()
			: FExpression(EExpressionKind::ArrayLiteral)
		{
		}
	};

	struct FObjectProperty
	{
		FString Key;              // 속성 이름 (식별자 또는 문자열)
		FExpressionPtr Value;     // 속성 값 표현식
	};

	struct FObjectLiteralExpression : FExpression
	{
		TArray<FObjectProperty> Properties;  // 객체 속성 목록

		FObjectLiteralExpression()
			: FExpression(EExpressionKind::ObjectLiteral)
		{
		}
	};

	struct FIndexExpression : FExpression
	{
		FExpressionPtr Target;  // 배열 변수 (Identifier 또는 다른 표현식)
		FExpressionPtr Index;   // 인덱스 표현식

		FIndexExpression()
			: FExpression(EExpressionKind::Index)
		{
		}
	};

	struct FMemberAccessExpression : FExpression
	{
		FExpressionPtr Target;  // 객체 (배열 등)
		FString MemberName;      // 멤버 이름 (push_back, pop_front 등)

		FMemberAccessExpression()
			: FExpression(EExpressionKind::MemberAccess)
		{
		}
	};

	struct FArrowFunctionExpression : FExpression
	{
		TArray<FString> Parameters;  // 파라미터 목록
		FExpressionPtr Body;         // 단일 표현식 (x => x + 1)
		TSharedPtr<FBlockStatement> BodyBlock;  // 블록 문장 (x => { ... })

		FArrowFunctionExpression()
			: FExpression(EExpressionKind::ArrowFunction)
		{
		}
	};

	struct FPostfixExpression : FExpression
	{
		FExpressionPtr Operand;  // x++ 또는 x--에서 x
		bool bIsIncrement;      // true면 ++, false면 --

		FPostfixExpression()
			: FExpression(EExpressionKind::PostfixIncrement)
			, bIsIncrement(true)
		{
		}
	};
}

