#include "GSTN_MotionToolsPython.h"

#include "IPythonScriptPlugin.h"
#include "Misc/Base64.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	TSharedRef<FJsonObject> ErrorReply(const FString& Message)
	{
		TSharedRef<FJsonObject> Reply = MakeShared<FJsonObject>();
		Reply->SetBoolField(TEXT("ok"), false);
		Reply->SetStringField(TEXT("error"), Message);
		return Reply;
	}
}

TSharedRef<FJsonObject> GSTNMotionToolsPython::Call(const TSharedRef<FJsonObject>& Request)
{
	IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
	if (Python == nullptr || !Python->IsPythonAvailable())
	{
		return ErrorReply(TEXT("Python non disponibile: abilita il plugin Python Editor Script."));
	}

	FString RequestText;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&RequestText);
	FJsonSerializer::Serialize(Request, Writer);
	const FTCHARToUTF8 RequestUtf8(*RequestText);
	TArray<uint8> RequestBytes;
	RequestBytes.Append(reinterpret_cast<const uint8*>(RequestUtf8.Get()), RequestUtf8.Length());
	const FString RequestB64 = FBase64::Encode(RequestBytes);

	FPythonCommandEx Command;
	Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
	Command.FileExecutionScope = EPythonFileExecutionScope::Public;
	Command.Command = FString::Printf(TEXT("__import__('gstn_motion_tools.panel', fromlist=['run']).run('%s')"), *RequestB64);
	if (!Python->ExecPythonCommandEx(Command))
	{
		return ErrorReply(FString::Printf(TEXT("Errore Python (vedi Output Log): %s"), *Command.CommandResult.Left(300)));
	}

	// EvaluateStatement returns repr() of the str: 'base64'. Base64 has no characters that repr() escapes.
	FString Reply = Command.CommandResult.TrimStartAndEnd();
	Reply.TrimQuotesInline();
	Reply.ReplaceInline(TEXT("'"), TEXT(""));
	TArray<uint8> ReplyBytes;
	if (!FBase64::Decode(Reply, ReplyBytes))
	{
		return ErrorReply(TEXT("Risposta Python illeggibile (vedi Output Log)."));
	}
	ReplyBytes.Add(0);
	const FString ReplyText = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(ReplyBytes.GetData()));

	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ReplyText);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		return ErrorReply(TEXT("Risposta Python non è JSON (vedi Output Log)."));
	}
	return Parsed.ToSharedRef();
}
