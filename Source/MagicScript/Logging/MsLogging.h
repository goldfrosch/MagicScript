#pragma once

struct FScriptLog;
enum class EScriptLogType : uint8;

namespace MagicScript
{
	// 전역 로그 배열 가져오기
	TArray<FScriptLog>& GetScriptLogs();

	// 스크립트 로그를 전역 배열에 추가하는 헬퍼 함수
	void AddScriptLog(const EScriptLogType ScriptLogType, const FString& ScriptLogMessage);

	// 전역 로그 배열 초기화
	void ClearScriptLogs();
}