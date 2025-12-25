# MagicScript 플러그인

MagicScript는 커스텀 스크립팅 언어입니다. 이 문서는 플러그인의 코드 구조와 각 모듈의 역할을 설명합니다.

## 목차

1. [개요](#개요)
2. [폴더 구조](#폴더-구조)
3. [모듈 설명](#모듈-설명)
   - [Core](#core)
   - [Runtime](#runtime)
   - [Subsystem](#subsystem)
   - [Util](#util)
   - [Analysis](#analysis)
   - [Logging](#logging)
4. [빌드 구성](#빌드-구성)

---

## 개요

MagicScript는 다음과 같은 특징을 가진 DSL(Domain Specific Language)입니다:

- **렉싱 및 파싱**: 소스 코드를 토큰화하고 AST로 변환
- **인터프리터 실행**: AST를 실행하여 스크립트를 동작시킴
- **시간/공간 복잡도 분석**: 정적 및 동적 분석을 통한 복잡도 계산
- **비동기 지원**: EventLoop를 통한 `setTimeout` 같은 비동기 작업 처리
- **네이티브 함수 통합**: C++로 작성된 빌트인 함수 제공

---

## 폴더 구조

```
Plugins/MagicScript/
├── Source/
│   └── MagicScript/
│       ├── Core/               # 핵심 컴파일러/인터프리터 컴포넌트
│       ├── Runtime/            # 런타임 실행 환경
│       ├── Subsystem/          # Unreal Engine 서브시스템 통합
│       ├── Util/               # 빌트인 함수 모음
│       ├── Analysis/           # 복잡도 분석기
│       ├── Logging/            # 로깅 시스템
│       ├── MagicScript.h       # 플러그인 모듈 인터페이스
│       ├── MagicScript.cpp
│       └── MagicScript.Build.cs # 빌드 설정
├── Content/                    # 플러그인 콘텐츠
├── Resources/                  # 플러그인 리소스
└── MagicScript.uplugin        # 플러그인 설정 파일
```

---

## 모듈 설명

### Core

핵심 컴파일러 및 인터프리터 컴포넌트입니다.

#### `MsToken.h`

- **역할**: 토큰 타입 및 소스 위치 정보 정의
- **주요 내용**:
  - `ETokenType`: 렉서가 생성하는 토큰 타입 (Identifier, Number, String, 키워드, 연산자 등)
  - `FSourceLocation`: 소스 코드의 행/열 위치 정보
  - `FToken`: 토큰 구조체 (타입, 어휘소, 위치 정보)

#### `MsLexer.h/cpp`

- **역할**: 소스 코드를 토큰 시퀀스로 변환하는 렉서
- **주요 기능**:
  - 공백 및 주석 제거
  - 숫자, 문자열, 식별자, 키워드 인식
  - 연산자 및 구분자 토큰화
  - 에러 토큰 생성 (잘못된 문자 등)

#### `MsAst.h`

- **역할**: Abstract Syntax Tree (AST) 노드 정의
- **주요 구조**:
  - `FProgram`: 프로그램 루트 (문장 배열)
  - `FStatement`: 문장 노드 (블록, 변수 선언, 함수 선언, 제어문 등)
  - `FExpression`: 표현식 노드 (이진/단항 연산, 리터럴, 식별자, 호출 등)
  - 다양한 문장/표현식 타입 (If, While, For, Switch, Call 등)

#### `MsParser.h/cpp`

- **역할**: 토큰 시퀀스를 AST로 변환하는 재귀 하향 파서
- **주요 기능**:
  - `ParseProgram()`: 프로그램 전체 파싱
  - 문장 및 표현식 파싱
  - 에러 복구 (synchronize)
  - 파싱 에러 메시지 수집

#### `MsValue.h`

- **역할**: 런타임 값 타입 정의
- **주요 내용**:
  - `EValueType`: 값 타입 (Null, Number, String, Bool, Function, Array, Object)
  - `FValue`: 런타임 값 구조체 (타입별 데이터 저장)
  - `FFunctionValue`: 함수 값 (네이티브/스크립트 함수 구분)
  - 값 생성 헬퍼 함수 (`FromNumber`, `FromString` 등)
  - `ToDebugString()`: 디버깅용 문자열 변환

#### `MsEnvironment.h/cpp`

- **역할**: 변수 스코프 및 환경 관리 (렉시컬 스코프)
- **주요 기능**:
  - `Define()`: 변수/함수 정의
  - `Assign()`: 변수 값 할당
  - `Lookup()`: 변수 검색 (부모 환경으로 상속)
  - `Clone()`: 환경 깊은 복사 (스냅샷용)

---

### Runtime

런타임 실행 환경 및 인터프리터입니다.

#### `MsInterpreter.h/cpp`

- **역할**: AST를 실행하는 인터프리터
- **주요 기능**:
  - `ExecuteProgram()`: 프로그램 전체 실행
  - `ExecuteStatement()`: 문장 실행
  - `EvaluateExpression()`: 표현식 평가
  - `CallFunction()`: 함수 호출 (스크립트/네이티브)
  - 메모리 사용량 추적 (`PeakSpaceBytes`)
  - 실행 통계 (실행 횟수, 표현식 평가 횟수, 함수 호출 횟수)
  - 재귀 호출 깊이 제한 (최대 64)
  - `EExecutionMode`: 정상 실행 / 사전 분석 모드

#### `MsEventLoop.h/cpp`

- **역할**: 비동기 작업 처리 (예: `setTimeout`)
- **주요 기능**:
  - `SetTimeout()`: 비동기 작업 등록
  - `Tick()`: Tick에서 호출하여 대기 중인 작업 실행
  - `HasPendingTasks()`: 대기 중인 작업 존재 여부 확인
  - `ClearAllTasks()`: 모든 작업 취소

---

### Subsystem

Unreal Engine 서브시스템 통합입니다.

#### `MagicScriptInterpreterSubsystem.h/cpp`

- **역할**: Unreal Engine GameInstanceSubsystem로 MagicScript 인터프리터 제공
- **주요 기능**:
  - `RunScriptFile()`: 스크립트 파일 실행
  - 스크립트 캐싱 (소스 코드, 파싱된 프로그램, 인터프리터)
  - 시간/공간 복잡도 캐시
  - `OnScriptLogAdded`: 로그 이벤트 델리게이트
  - `TickEventLoops()`: 모든 이벤트 루프 업데이트
  - 빌트인 함수 등록 관리

---

### Util

네이티브 빌트인 함수 모음입니다.

#### `MsGlobalBuiltins.h/cpp`

- **역할**: 전역 네이티브 함수 등록
- **제공 함수**:
  - `SetGlobalFloat(name, value)`: 전역 변수 설정

#### `MsArrayBuiltins.h/cpp`

- **역할**: 배열 관련 네이티브 함수 등록
- **제공 함수**:
  - `Array.push_back(array, value)`: 배열 끝에 요소 추가
  - `Array.push_front(array, value)`: 배열 앞에 요소 추가
  - `Array.pop_back(array)`: 배열 마지막 요소 제거 및 반환
  - `Array.pop_front(array)`: 배열 첫 요소 제거 및 반환
  - `Array.length(array)`: 배열 길이 반환

#### `MsConsoleBuiltins.h/cpp`

- **역할**: 콘솔 출력 관련 네이티브 함수 등록
- **제공 함수**:
  - `console.log(...args)`: 일반 로그 출력
  - `console.warn(...args)`: 경고 로그 출력
  - `console.error(...args)`: 에러 로그 출력

#### `MsMathBuiltins.h/cpp`

- **역할**: 수학 함수 등록
- **제공 함수**:
  - `math.pow(base, exp)`: 거듭제곱 계산

---

### Analysis

복잡도 분석기입니다.

#### `MsTimeComplexity.h/cpp`

- **역할**: 스크립트의 시간 복잡도를 정적 및 동적으로 분석
- **주요 내용**:
  - `FTimeComplexityResult`: 분석 결과 구조체
    - `StaticComplexityScore`: AST 기반 정적 복잡도 점수
    - `DynamicExecutionCount`: 실제 실행 횟수
    - `StatementCount`: 문장 수
    - `MaxLoopDepth`: 최대 루프 깊이
    - `FunctionCallCount`: 함수 호출 횟수
    - `ExpressionEvaluationCount`: 표현식 평가 횟수
    - 분석/실행 시간
  - `FTimeComplexityAnalyzer`: AST를 순회하며 복잡도 계산

---

### Logging

로깅 시스템입니다.

#### `MsLogging.h`

- **역할**: 스크립트 실행 중 발생하는 로그 관리
- **주요 내용**:
  - `EScriptLogType`: 로그 타입 (Default, Warning, Error)
  - `FScriptLog`: 로그 구조체 (타입, 메시지)
  - `GetScriptLogs()`: 전역 로그 배열 접근
  - `AddScriptLog()`: 로그 추가
  - `ClearScriptLogs()`: 로그 초기화

---

## 빌드 구성

### MagicScript.Build.cs

플러그인 모듈의 빌드 설정 파일입니다. 필요한 Unreal Engine 모듈 및 외부 라이브러리를 포함합니다.

### MagicScript.uplugin

플러그인 메타데이터:

- **버전**: 1.0
- **이름**: MagicScript
- **타입**: Runtime 플러그인
- **제작자**: GoldFrosch

---

## 사용 방법

### C++에서 스크립트 실행

```cpp
#include "MagicScript/Subsystem/MagicScriptInterpreterSubsystem.h"

// 서브시스템 가져오기
if (UMagicScriptInterpreterSubsystem* ScriptSubsystem = UMagicScriptInterpreterSubsystem::Get(this))
{
    // 스크립트 파일 실행
    bool bSuccess = ScriptSubsystem->RunScriptFile(
        TEXT("Scripts/MyScript.ms"),  // Content 폴더 기준 상대 경로
        TEXT("main")                   // 실행할 함수 이름
    );
}
```

### 로그 수신

```cpp
// 델리게이트 바인딩
ScriptSubsystem->OnScriptLogAdded.AddDynamic(this, &AMyActor::OnScriptLog);

// 콜백 함수
void AMyActor::OnScriptLog(const FScriptLog& Log)
{
    UE_LOG(LogTemp, Log, TEXT("%s"), *Log.LogMessage);
}
```

### 이벤트 루프 업데이트

서브시스템이 자동으로 `TickEventLoops()`를 호출하지만, 수동으로도 호출 가능합니다:

```cpp
ScriptSubsystem->TickEventLoops();
```

---

## 확장 방법

### 새로운 네이티브 함수 추가

1. 적절한 `Util` 네임스페이스 파일에 함수 등록 로직 추가
2. `RegisterNative` 람다를 사용하여 함수 등록:

```cpp
RegisterNative(TEXT("MyFunction"), 0, [](const TArray<FValue>& Args, const FScriptExecutionContext& Context) -> FValue
{
    // 함수 구현
    return FValue::FromNumber(42);
});
```

3. `MagicScriptInterpreterSubsystem::OnRegisterBuiltins()`에서 해당 네임스페이스의 `Register()` 호출

---

## 참고사항

- 모든 스크립트 코드는 `Content/Scripts/` 폴더에 `.ms` 확장자로 저장
- 함수 재귀 호출 최대 깊이: 64
- while 루프 최대 반복 횟수: 128
- 스크립트 실행 결과는 캐시되어 재사용됨
- 시간/공간 복잡도는 자동으로 분석되어 캐시됨

---
