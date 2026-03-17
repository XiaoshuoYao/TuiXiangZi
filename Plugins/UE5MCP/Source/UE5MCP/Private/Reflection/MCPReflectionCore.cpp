// Copyright Epic Games, Inc. All Rights Reserved.

#include "Reflection/MCPReflectionCore.h"

#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#if __has_include("UObject/StrProperty.h")
#include "UObject/StrProperty.h"
#endif
#include "UObject/SoftObjectPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPReflection, Log, All);

// ============================================================================
// ResolveClass
// ============================================================================

UClass* MCPReflection::ResolveClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return nullptr;
	}

	// Strategy 1: /Script/... path — direct FindObject
	if (ClassName.StartsWith(TEXT("/Script/")))
	{
		UClass* Found = FindObject<UClass>(nullptr, *ClassName);
		if (Found)
		{
			return Found;
		}
	}

	// Strategy 2: /Game/... path (Blueprint class) — use FSoftClassPath
	if (ClassName.StartsWith(TEXT("/Game/")) || ClassName.StartsWith(TEXT("/")) )
	{
		FSoftClassPath ClassPath(ClassName);
		UClass* Found = ClassPath.TryLoadClass<UObject>();
		if (Found)
		{
			return Found;
		}

		// Try appending _C if not already present
		if (!ClassName.EndsWith(TEXT("_C")))
		{
			FSoftClassPath ClassPathC(ClassName + TEXT("_C"));
			Found = ClassPathC.TryLoadClass<UObject>();
			if (Found)
			{
				return Found;
			}
		}
	}

	// Strategy 3: Short name — iterate all loaded UClass objects
	if (!ClassName.Contains(TEXT("/")))
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->GetName() == ClassName)
			{
				return *It;
			}
		}
	}

	return nullptr;
}

// ============================================================================
// MapPropertyType
// ============================================================================

FString MCPReflection::MapPropertyType(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("unknown");
	}

	// --- Scalar types ---
	if (CastField<FBoolProperty>(Property))
	{
		return TEXT("bool");
	}
	if (CastField<FIntProperty>(Property))
	{
		return TEXT("int32");
	}
	if (CastField<FInt64Property>(Property))
	{
		return TEXT("int64");
	}
	if (CastField<FFloatProperty>(Property))
	{
		return TEXT("float");
	}
	if (CastField<FDoubleProperty>(Property))
	{
		return TEXT("double");
	}
	if (Property->GetClass()->GetFName() == TEXT("StrProperty"))
	{
		return TEXT("FString");
	}
	if (CastField<FNameProperty>(Property))
	{
		return TEXT("FName");
	}
	if (CastField<FTextProperty>(Property))
	{
		return TEXT("FText");
	}

	// FByteProperty — check for enum
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			return FString::Printf(TEXT("TEnumAsByte<%s>"), *ByteProp->Enum->GetPathName());
		}
		return TEXT("uint8");
	}

	// FEnumProperty
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (EnumProp->GetEnum())
		{
			return EnumProp->GetEnum()->GetPathName();
		}
		return TEXT("enum");
	}

	// --- Container types (recursive) ---
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		return FString::Printf(TEXT("TArray<%s>"), *MapPropertyType(ArrayProp->Inner));
	}
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		return FString::Printf(TEXT("TSet<%s>"), *MapPropertyType(SetProp->ElementProp));
	}
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		return FString::Printf(TEXT("TMap<%s, %s>"),
			*MapPropertyType(MapProp->KeyProp),
			*MapPropertyType(MapProp->ValueProp));
	}

	// --- Struct ---
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (StructProp->Struct)
		{
			return StructProp->Struct->GetPathName();
		}
		return TEXT("struct");
	}

	// --- Object reference types (order matters: specific before base) ---
	if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
	{
		if (ClassProp->MetaClass)
		{
			return FString::Printf(TEXT("TSubclassOf<%s>"), *ClassProp->MetaClass->GetPathName());
		}
		return TEXT("TSubclassOf<UObject>");
	}
	if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		if (SoftClassProp->MetaClass)
		{
			return FString::Printf(TEXT("TSoftClassPtr<%s>"), *SoftClassProp->MetaClass->GetPathName());
		}
		return TEXT("TSoftClassPtr<UObject>");
	}
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		if (SoftObjProp->PropertyClass)
		{
			return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *SoftObjProp->PropertyClass->GetPathName());
		}
		return TEXT("TSoftObjectPtr<UObject>");
	}
	if (FWeakObjectProperty* WeakProp = CastField<FWeakObjectProperty>(Property))
	{
		if (WeakProp->PropertyClass)
		{
			return FString::Printf(TEXT("TWeakObjectPtr<%s>"), *WeakProp->PropertyClass->GetPathName());
		}
		return TEXT("TWeakObjectPtr<UObject>");
	}
	if (FLazyObjectProperty* LazyProp = CastField<FLazyObjectProperty>(Property))
	{
		if (LazyProp->PropertyClass)
		{
			return FString::Printf(TEXT("TLazyObjectPtr<%s>"), *LazyProp->PropertyClass->GetPathName());
		}
		return TEXT("TLazyObjectPtr<UObject>");
	}
	if (FInterfaceProperty* InterfaceProp = CastField<FInterfaceProperty>(Property))
	{
		if (InterfaceProp->InterfaceClass)
		{
			return FString::Printf(TEXT("TScriptInterface<%s>"), *InterfaceProp->InterfaceClass->GetPathName());
		}
		return TEXT("TScriptInterface<UInterface>");
	}
	// FObjectPropertyBase — general object pointer (after specific subclasses)
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		if (ObjProp->PropertyClass)
		{
			return FString::Printf(TEXT("%s*"), *ObjProp->PropertyClass->GetPathName());
		}
		return TEXT("UObject*");
	}

	// --- Fallback ---
	FString CppType = Property->GetCPPType();
	UE_LOG(LogMCPReflection, Warning,
		TEXT("MapPropertyType: unrecognized property '%s', falling back to GetCPPType() = '%s'"),
		*Property->GetName(), *CppType);
	return CppType;
}

// ============================================================================
// Flags & Metadata Whitelists
// ============================================================================

namespace
{
	struct FFlagMapping
	{
		EPropertyFlags Flag;
		const TCHAR* Name;
	};

	static const FFlagMapping PropertyFlagWhitelist[] =
	{
		{ CPF_Edit,                   TEXT("Edit") },
		{ CPF_BlueprintVisible,       TEXT("BlueprintVisible") },
		{ CPF_BlueprintReadOnly,      TEXT("BlueprintReadOnly") },
		{ CPF_Transient,              TEXT("Transient") },
		{ CPF_Config,                 TEXT("Config") },
		{ CPF_SaveGame,               TEXT("SaveGame") },
		{ CPF_InstancedReference,     TEXT("InstancedReference") },
		{ CPF_ExportObject,           TEXT("ExportObject") },
		{ CPF_EditConst,              TEXT("EditConst") },
		{ CPF_DisableEditOnInstance,  TEXT("DisableEditOnInstance") },
		{ CPF_DisableEditOnTemplate,  TEXT("DisableEditOnTemplate") },
		{ CPF_Net,                    TEXT("Net") },
		{ CPF_RepNotify,              TEXT("RepNotify") },
		{ CPF_Interp,                 TEXT("Interp") },
		{ CPF_AdvancedDisplay,        TEXT("AdvancedDisplay") },
		{ CPF_Deprecated,             TEXT("Deprecated") },
		{ CPF_EditorOnly,             TEXT("EditorOnly") },
	};

	static const TCHAR* MetadataKeyWhitelist[] =
	{
		TEXT("DisplayName"),
		TEXT("Category"),
		TEXT("ToolTip"),
		TEXT("ClampMin"),
		TEXT("ClampMax"),
		TEXT("UIMin"),
		TEXT("UIMax"),
		TEXT("Units"),
		TEXT("EditCondition"),
		TEXT("BlueprintGetter"),
		TEXT("BlueprintSetter"),
		TEXT("ExposeOnSpawn"),
		TEXT("AllowPrivateAccess"),
	};
}

// ============================================================================
// BuildPropertyDesc
// ============================================================================

TSharedPtr<FJsonObject> MCPReflection::BuildPropertyDesc(FProperty* Property, bool bIncludeMetadata)
{
	if (!Property)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Desc = MakeShared<FJsonObject>();

	// name (required)
	Desc->SetStringField(TEXT("name"), Property->GetName());

	// type (required)
	Desc->SetStringField(TEXT("type"), MapPropertyType(Property));

	// flags (optional — only emit if non-empty)
	TArray<TSharedPtr<FJsonValue>> FlagsArray;
	const EPropertyFlags PropFlags = Property->PropertyFlags;
	for (const FFlagMapping& Mapping : PropertyFlagWhitelist)
	{
		if (PropFlags & Mapping.Flag)
		{
			FlagsArray.Add(MakeShared<FJsonValueString>(Mapping.Name));
		}
	}
	if (FlagsArray.Num() > 0)
	{
		Desc->SetArrayField(TEXT("flags"), FlagsArray);
	}

	// metadata (optional — only when requested and non-empty)
	if (bIncludeMetadata)
	{
		TSharedPtr<FJsonObject> MetaObj = MakeShared<FJsonObject>();
		bool bHasAnyMeta = false;

		for (const TCHAR* Key : MetadataKeyWhitelist)
		{
			if (Property->HasMetaData(Key))
			{
				FString Value = Property->GetMetaData(Key);
				MetaObj->SetStringField(Key, Value);
				bHasAnyMeta = true;
			}
		}

		if (bHasAnyMeta)
		{
			Desc->SetObjectField(TEXT("metadata"), MetaObj);
		}
	}

	return Desc;
}

// ============================================================================
// BuildFunctionDesc
// ============================================================================

TSharedPtr<FJsonObject> MCPReflection::BuildFunctionDesc(UFunction* Function)
{
	if (!Function)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Desc = MakeShared<FJsonObject>();

	// name (required)
	Desc->SetStringField(TEXT("name"), Function->GetName());

	// params (required) and returns (required)
	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	FString ReturnType = TEXT("void");

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Param = *It;
		if (!(Param->PropertyFlags & CPF_Parm))
		{
			continue;
		}

		// ReturnParm → set returns, skip from params
		if (Param->PropertyFlags & CPF_ReturnParm)
		{
			ReturnType = MapPropertyType(Param);
			continue;
		}

		TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
		ParamObj->SetStringField(TEXT("name"), Param->GetName());
		ParamObj->SetStringField(TEXT("type"), MapPropertyType(Param));

		// Determine direction
		if (Param->PropertyFlags & CPF_OutParm)
		{
			if (Param->PropertyFlags & CPF_ReferenceParm)
			{
				ParamObj->SetStringField(TEXT("direction"), TEXT("inout"));
			}
			else
			{
				ParamObj->SetStringField(TEXT("direction"), TEXT("out"));
			}
		}
		else
		{
			ParamObj->SetStringField(TEXT("direction"), TEXT("in"));
		}

		ParamsArray.Add(MakeShared<FJsonValueObject>(ParamObj));
	}

	Desc->SetArrayField(TEXT("params"), ParamsArray);
	Desc->SetStringField(TEXT("returns"), ReturnType);

	return Desc;
}
