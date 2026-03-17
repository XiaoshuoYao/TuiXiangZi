// Copyright Epic Games, Inc. All Rights Reserved.

#include "Edit/MCPEditEnvelope.h"

namespace MCPEditEnvelope
{

TSharedPtr<FJsonObject> MakeEditEnvelope(
	bool bOk,
	bool bDryRun,
	const TArray<TSharedPtr<FJsonValue>>& Changes,
	const TArray<FString>& Warnings,
	bool bNeedsCompile,
	bool bNeedsSave)
{
	TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();

	Envelope->SetBoolField(TEXT("ok"), bOk);
	Envelope->SetBoolField(TEXT("dry_run"), bDryRun);

	// Changes array
	Envelope->SetArrayField(TEXT("changes"), Changes);

	// Warnings array (convert FString array to JSON string values)
	TArray<TSharedPtr<FJsonValue>> WarningValues;
	WarningValues.Reserve(Warnings.Num());
	for (const FString& Warning : Warnings)
	{
		WarningValues.Add(MakeShared<FJsonValueString>(Warning));
	}
	Envelope->SetArrayField(TEXT("warnings"), WarningValues);

	// Optional flags — only set when true to keep the response compact
	if (bNeedsCompile)
	{
		Envelope->SetBoolField(TEXT("needs_compile"), true);
	}

	if (bNeedsSave)
	{
		Envelope->SetBoolField(TEXT("needs_save"), true);
	}

	return Envelope;
}

void SetEditError(
	TSharedPtr<FJsonObject>& Envelope,
	const FString& ErrorCode,
	const FString& Message)
{
	if (!Envelope.IsValid())
	{
		Envelope = MakeShared<FJsonObject>();
	}

	Envelope->SetBoolField(TEXT("ok"), false);

	TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
	ErrorObj->SetStringField(TEXT("code"), ErrorCode);
	ErrorObj->SetStringField(TEXT("message"), Message);
	Envelope->SetObjectField(TEXT("error"), ErrorObj);
}

} // namespace MCPEditEnvelope
