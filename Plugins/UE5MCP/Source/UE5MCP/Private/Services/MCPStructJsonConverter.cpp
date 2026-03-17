// Copyright Epic Games, Inc. All Rights Reserved.

#include "Services/MCPStructJsonConverter.h"

#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/DataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPStructJsonConverter, Log, All);

static constexpr int32 MaxRecursionDepth = 5;

// ============================================================================
// Internal Helpers
// ============================================================================

namespace
{
	bool TryGetNumber(const TSharedPtr<FJsonValue>& JsonValue, double& OutValue)
	{
		if (JsonValue->Type == EJson::Number)
		{
			OutValue = JsonValue->AsNumber();
			return true;
		}
		if (JsonValue->Type == EJson::String)
		{
			FString Str = JsonValue->AsString();
			if (Str.IsNumeric())
			{
				OutValue = FCString::Atod(*Str);
				return true;
			}
		}
		return false;
	}

	// Forward declarations for recursive helpers
	TSharedPtr<FJsonValue> PropertyToJsonValueInternal(
		const FProperty* Property, const void* ValuePtr, int32 Depth);

	bool JsonValueToPropertyInternal(
		const TSharedPtr<FJsonValue>& JsonValue, const FProperty* Property,
		void* ValuePtr, TArray<FString>& OutErrors, const FString& FieldPath, int32 Depth);
}

// ============================================================================
// MapPropertyTypeString
// ============================================================================

FString MCPStructJsonConverter::MapPropertyTypeString(const FProperty* Property)
{
	if (!Property) return TEXT("unknown");

	if (CastField<FBoolProperty>(Property)) return TEXT("bool");
	if (CastField<FIntProperty>(Property)) return TEXT("int32");
	if (CastField<FInt64Property>(Property)) return TEXT("int64");
	if (CastField<FFloatProperty>(Property)) return TEXT("float");
	if (CastField<FDoubleProperty>(Property)) return TEXT("double");
	if (Property->GetClass()->GetFName() == TEXT("StrProperty")) return TEXT("string");
	if (CastField<FNameProperty>(Property)) return TEXT("name");
	if (CastField<FTextProperty>(Property)) return TEXT("text");

	if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		return ByteProp->Enum ? TEXT("enum") : TEXT("uint8");
	}
	if (CastField<FEnumProperty>(Property)) return TEXT("enum");

	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		if (Struct == TBaseStructure<FVector>::Get()) return TEXT("vector");
		if (Struct == TBaseStructure<FVector2D>::Get()) return TEXT("vector2d");
		if (Struct == TBaseStructure<FRotator>::Get()) return TEXT("rotator");
		if (Struct == TBaseStructure<FTransform>::Get()) return TEXT("transform");
		if (Struct == TBaseStructure<FLinearColor>::Get()) return TEXT("linear_color");
		if (Struct == TBaseStructure<FColor>::Get()) return TEXT("color");
		return TEXT("struct");
	}

	if (CastField<FClassProperty>(Property)) return TEXT("class_reference");
	if (CastField<FSoftClassProperty>(Property)) return TEXT("soft_class_reference");
	if (CastField<FSoftObjectProperty>(Property)) return TEXT("soft_object_reference");
	if (CastField<FObjectPropertyBase>(Property)) return TEXT("object_reference");

	if (CastField<FArrayProperty>(Property)) return TEXT("array");
	if (CastField<FMapProperty>(Property)) return TEXT("map");
	if (CastField<FSetProperty>(Property)) return TEXT("set");

	return TEXT("unknown");
}

// ============================================================================
// PropertyToJsonValue
// ============================================================================

namespace
{
	TSharedPtr<FJsonValue> PropertyToJsonValueInternal(
		const FProperty* Property, const void* ValuePtr, int32 Depth)
	{
		if (!Property || !ValuePtr)
		{
			return MakeShared<FJsonValueNull>();
		}

		if (Depth > MaxRecursionDepth)
		{
			UE_LOG(LogMCPStructJsonConverter, Warning,
				TEXT("Max recursion depth reached for property '%s'"), *Property->GetName());
			return MakeShared<FJsonValueString>(TEXT("<max_depth>"));
		}

		// --- Bool ---
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
		}

		// --- Integer types ---
		if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(IntProp->GetPropertyValue(ValuePtr)));
		}
		if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
		}

		// --- Float types ---
		if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(static_cast<double>(FloatProp->GetPropertyValue(ValuePtr)));
		}
		if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
		}

		// --- String types ---
		if (Property->GetClass()->GetFName() == TEXT("StrProperty"))
		{
			return MakeShared<FJsonValueString>(*static_cast<const FString*>(ValuePtr));
		}
		if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
		}
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
		}

		// --- FByteProperty (with Enum) ---
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum)
			{
				uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
				FString EnumName = ByteProp->Enum->GetNameStringByValue(static_cast<int64>(ByteValue));
				return MakeShared<FJsonValueString>(EnumName);
			}
			return MakeShared<FJsonValueNumber>(static_cast<double>(ByteProp->GetPropertyValue(ValuePtr)));
		}

		// --- FEnumProperty ---
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			const FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
			UEnum* Enum = EnumProp->GetEnum();
			if (Enum)
			{
				return MakeShared<FJsonValueString>(Enum->GetNameStringByValue(EnumValue));
			}
			return MakeShared<FJsonValueNumber>(static_cast<double>(EnumValue));
		}

		// --- Soft Object ---
		if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
			FSoftObjectPath Path = SoftPtr.GetUniqueID();
			if (Path.IsNull()) { return MakeShared<FJsonValueNull>(); }
			return MakeShared<FJsonValueString>(Path.ToString());
		}

		// --- Soft Class ---
		if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
			FSoftObjectPath Path = SoftPtr.GetUniqueID();
			if (Path.IsNull()) { return MakeShared<FJsonValueNull>(); }
			return MakeShared<FJsonValueString>(Path.ToString());
		}

		// --- Class property ---
		if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			UObject* ObjValue = ClassProp->GetObjectPropertyValue(ValuePtr);
			if (!ObjValue) { return MakeShared<FJsonValueNull>(); }
			return MakeShared<FJsonValueString>(ObjValue->GetPathName());
		}

		// --- Object reference ---
		if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (!ObjValue) { return MakeShared<FJsonValueNull>(); }
			return MakeShared<FJsonValueString>(ObjValue->GetPathName());
		}

		// --- Struct ---
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			UScriptStruct* Struct = StructProp->Struct;

			// FVector
			if (Struct == TBaseStructure<FVector>::Get())
			{
				const FVector& Vec = *static_cast<const FVector*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetNumberField(TEXT("x"), Vec.X);
				Obj->SetNumberField(TEXT("y"), Vec.Y);
				Obj->SetNumberField(TEXT("z"), Vec.Z);
				return MakeShared<FJsonValueObject>(Obj);
			}

			// FVector2D
			if (Struct == TBaseStructure<FVector2D>::Get())
			{
				const FVector2D& Vec = *static_cast<const FVector2D*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetNumberField(TEXT("x"), Vec.X);
				Obj->SetNumberField(TEXT("y"), Vec.Y);
				return MakeShared<FJsonValueObject>(Obj);
			}

			// FRotator
			if (Struct == TBaseStructure<FRotator>::Get())
			{
				const FRotator& Rot = *static_cast<const FRotator*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetNumberField(TEXT("pitch"), Rot.Pitch);
				Obj->SetNumberField(TEXT("yaw"), Rot.Yaw);
				Obj->SetNumberField(TEXT("roll"), Rot.Roll);
				return MakeShared<FJsonValueObject>(Obj);
			}

			// FLinearColor
			if (Struct == TBaseStructure<FLinearColor>::Get())
			{
				const FLinearColor& Color = *static_cast<const FLinearColor*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetNumberField(TEXT("r"), Color.R);
				Obj->SetNumberField(TEXT("g"), Color.G);
				Obj->SetNumberField(TEXT("b"), Color.B);
				Obj->SetNumberField(TEXT("a"), Color.A);
				return MakeShared<FJsonValueObject>(Obj);
			}

			// FColor
			if (Struct == TBaseStructure<FColor>::Get())
			{
				const FColor& Color = *static_cast<const FColor*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
				Obj->SetNumberField(TEXT("r"), static_cast<double>(Color.R));
				Obj->SetNumberField(TEXT("g"), static_cast<double>(Color.G));
				Obj->SetNumberField(TEXT("b"), static_cast<double>(Color.B));
				Obj->SetNumberField(TEXT("a"), static_cast<double>(Color.A));
				return MakeShared<FJsonValueObject>(Obj);
			}

			// FTransform
			if (Struct == TBaseStructure<FTransform>::Get())
			{
				const FTransform& Transform = *static_cast<const FTransform*>(ValuePtr);
				TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

				const FVector Loc = Transform.GetLocation();
				TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
				LocObj->SetNumberField(TEXT("x"), Loc.X);
				LocObj->SetNumberField(TEXT("y"), Loc.Y);
				LocObj->SetNumberField(TEXT("z"), Loc.Z);
				Obj->SetObjectField(TEXT("translation"), LocObj);

				const FRotator Rot = Transform.Rotator();
				TSharedPtr<FJsonObject> RotObj = MakeShared<FJsonObject>();
				RotObj->SetNumberField(TEXT("pitch"), Rot.Pitch);
				RotObj->SetNumberField(TEXT("yaw"), Rot.Yaw);
				RotObj->SetNumberField(TEXT("roll"), Rot.Roll);
				Obj->SetObjectField(TEXT("rotation"), RotObj);

				const FVector Scale = Transform.GetScale3D();
				TSharedPtr<FJsonObject> ScaleObj = MakeShared<FJsonObject>();
				ScaleObj->SetNumberField(TEXT("x"), Scale.X);
				ScaleObj->SetNumberField(TEXT("y"), Scale.Y);
				ScaleObj->SetNumberField(TEXT("z"), Scale.Z);
				Obj->SetObjectField(TEXT("scale"), ScaleObj);

				return MakeShared<FJsonValueObject>(Obj);
			}

			// Generic struct — recursively read all fields
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				const FProperty* ChildProp = *It;
				const void* ChildValuePtr = ChildProp->ContainerPtrToValuePtr<void>(ValuePtr);
				TSharedPtr<FJsonValue> ChildVal = PropertyToJsonValueInternal(ChildProp, ChildValuePtr, Depth + 1);
				Obj->SetField(ChildProp->GetName(), ChildVal);
			}
			return MakeShared<FJsonValueObject>(Obj);
		}

		// --- Array ---
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			TArray<TSharedPtr<FJsonValue>> JsonArray;
			JsonArray.Reserve(ArrayHelper.Num());

			for (int32 i = 0; i < ArrayHelper.Num(); ++i)
			{
				const void* ElementPtr = ArrayHelper.GetRawPtr(i);
				TSharedPtr<FJsonValue> ElemVal = PropertyToJsonValueInternal(ArrayProp->Inner, ElementPtr, Depth + 1);
				JsonArray.Add(ElemVal);
			}
			return MakeShared<FJsonValueArray>(JsonArray);
		}

		// Fallback — export as string
		FString ExportedValue;
		Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
		return MakeShared<FJsonValueString>(ExportedValue);
	}
}

TSharedPtr<FJsonValue> MCPStructJsonConverter::PropertyToJsonValue(
	const FProperty* Property, const void* ValuePtr)
{
	return PropertyToJsonValueInternal(Property, ValuePtr, 0);
}

// ============================================================================
// JsonValueToProperty
// ============================================================================

namespace
{
	bool JsonValueToPropertyInternal(
		const TSharedPtr<FJsonValue>& JsonValue, const FProperty* Property,
		void* ValuePtr, TArray<FString>& OutErrors, const FString& FieldPath, int32 Depth)
	{
		if (!Property || !ValuePtr || !JsonValue.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: invalid arguments"), *FieldPath));
			return false;
		}

		if (Depth > MaxRecursionDepth)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: max recursion depth exceeded"), *FieldPath));
			return false;
		}

		// --- Bool ---
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			if (JsonValue->Type == EJson::Boolean)
			{
				const_cast<FBoolProperty*>(BoolProp)->SetPropertyValue(ValuePtr, JsonValue->AsBool());
				return true;
			}
			if (JsonValue->Type == EJson::Number)
			{
				const_cast<FBoolProperty*>(BoolProp)->SetPropertyValue(ValuePtr, JsonValue->AsNumber() != 0.0);
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected bool, got %d"), *FieldPath, static_cast<int32>(JsonValue->Type)));
			return false;
		}

		// --- Int ---
		if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				const_cast<FIntProperty*>(IntProp)->SetPropertyValue(ValuePtr, static_cast<int32>(NumVal));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected number for int32"), *FieldPath));
			return false;
		}

		// --- Int64 ---
		if (const FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				const_cast<FInt64Property*>(Int64Prop)->SetPropertyValue(ValuePtr, static_cast<int64>(NumVal));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected number for int64"), *FieldPath));
			return false;
		}

		// --- Float ---
		if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				const_cast<FFloatProperty*>(FloatProp)->SetPropertyValue(ValuePtr, static_cast<float>(NumVal));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected number for float"), *FieldPath));
			return false;
		}

		// --- Double ---
		if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				const_cast<FDoubleProperty*>(DoubleProp)->SetPropertyValue(ValuePtr, NumVal);
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected number for double"), *FieldPath));
			return false;
		}

		// --- FString ---
		if (Property->GetClass()->GetFName() == TEXT("StrProperty"))
		{
			if (JsonValue->Type == EJson::String)
			{
				*static_cast<FString*>(ValuePtr) = JsonValue->AsString();
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string"), *FieldPath));
			return false;
		}

		// --- FName ---
		if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				const_cast<FNameProperty*>(NameProp)->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string for FName"), *FieldPath));
			return false;
		}

		// --- FText ---
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				const_cast<FTextProperty*>(TextProp)->SetPropertyValue(ValuePtr, FText::FromString(JsonValue->AsString()));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string for FText"), *FieldPath));
			return false;
		}

		// --- FByteProperty (with Enum) ---
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum)
			{
				if (JsonValue->Type == EJson::String)
				{
					FString EnumStr = JsonValue->AsString();
					int64 EnumVal = ByteProp->Enum->GetValueByNameString(EnumStr);
					if (EnumVal == INDEX_NONE)
					{
						OutErrors.Add(FString::Printf(TEXT("%s: '%s' is not a valid value for enum '%s'"),
							*FieldPath, *EnumStr, *ByteProp->Enum->GetName()));
						return false;
					}
					const_cast<FByteProperty*>(ByteProp)->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumVal));
					return true;
				}
				if (JsonValue->Type == EJson::Number)
				{
					const_cast<FByteProperty*>(ByteProp)->SetPropertyValue(ValuePtr, static_cast<uint8>(JsonValue->AsNumber()));
					return true;
				}
				OutErrors.Add(FString::Printf(TEXT("%s: expected string or number for enum"), *FieldPath));
				return false;
			}
			// Plain byte
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				const_cast<FByteProperty*>(ByteProp)->SetPropertyValue(ValuePtr, static_cast<uint8>(NumVal));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected number for uint8"), *FieldPath));
			return false;
		}

		// --- FEnumProperty ---
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();

			if (JsonValue->Type == EJson::String)
			{
				FString EnumStr = JsonValue->AsString();
				int64 EnumVal = Enum ? Enum->GetValueByNameString(EnumStr) : INDEX_NONE;
				if (EnumVal == INDEX_NONE)
				{
					OutErrors.Add(FString::Printf(TEXT("%s: '%s' is not a valid enum value for '%s'"),
						*FieldPath, *EnumStr, Enum ? *Enum->GetName() : TEXT("unknown")));
					return false;
				}
				UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumVal);
				return true;
			}
			if (JsonValue->Type == EJson::Number)
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(JsonValue->AsNumber()));
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string or number for enum"), *FieldPath));
			return false;
		}

		// --- Soft Object ---
		if (const FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FSoftObjectPath Path(JsonValue->AsString());
				FSoftObjectPtr SoftPtr(Path);
				const_cast<FSoftObjectProperty*>(SoftObjProp)->SetPropertyValue(ValuePtr, SoftPtr);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				const_cast<FSoftObjectProperty*>(SoftObjProp)->SetPropertyValue(ValuePtr, FSoftObjectPtr());
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string or null for soft object"), *FieldPath));
			return false;
		}

		// --- Soft Class ---
		if (const FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FSoftObjectPath Path(JsonValue->AsString());
				FSoftObjectPtr SoftPtr(Path);
				const_cast<FSoftClassProperty*>(SoftClassProp)->SetPropertyValue(ValuePtr, SoftPtr);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				const_cast<FSoftClassProperty*>(SoftClassProp)->SetPropertyValue(ValuePtr, FSoftObjectPtr());
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string or null for soft class"), *FieldPath));
			return false;
		}

		// --- Class property (TSubclassOf) ---
		if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FString ClassPath = JsonValue->AsString();
				UClass* LoadedClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
				if (!LoadedClass)
				{
					OutErrors.Add(FString::Printf(TEXT("%s: could not load class '%s'"), *FieldPath, *ClassPath));
					return false;
				}
				const_cast<FClassProperty*>(ClassProp)->SetObjectPropertyValue(ValuePtr, LoadedClass);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				const_cast<FClassProperty*>(ClassProp)->SetObjectPropertyValue(ValuePtr, nullptr);
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string or null for class ref"), *FieldPath));
			return false;
		}

		// --- Object reference ---
		if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FString ObjPath = JsonValue->AsString();
				UObject* LoadedObj = StaticLoadObject(
					ObjProp->PropertyClass ? static_cast<UClass*>(ObjProp->PropertyClass) : UObject::StaticClass(),
					nullptr, *ObjPath);
				if (!LoadedObj)
				{
					OutErrors.Add(FString::Printf(TEXT("%s: could not load object '%s'"), *FieldPath, *ObjPath));
					return false;
				}
				const_cast<FObjectPropertyBase*>(ObjProp)->SetObjectPropertyValue(ValuePtr, LoadedObj);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				const_cast<FObjectPropertyBase*>(ObjProp)->SetObjectPropertyValue(ValuePtr, nullptr);
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: expected string or null for object ref"), *FieldPath));
			return false;
		}

		// --- Struct ---
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (JsonValue->Type != EJson::Object)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: expected object for struct"), *FieldPath));
				return false;
			}

			const TSharedPtr<FJsonObject>& JsonObj = JsonValue->AsObject();
			UScriptStruct* Struct = StructProp->Struct;

			// FVector
			if (Struct == TBaseStructure<FVector>::Get())
			{
				FVector& Vec = *static_cast<FVector*>(ValuePtr);
				if (JsonObj->HasField(TEXT("x"))) Vec.X = JsonObj->GetNumberField(TEXT("x"));
				if (JsonObj->HasField(TEXT("y"))) Vec.Y = JsonObj->GetNumberField(TEXT("y"));
				if (JsonObj->HasField(TEXT("z"))) Vec.Z = JsonObj->GetNumberField(TEXT("z"));
				return true;
			}

			// FVector2D
			if (Struct == TBaseStructure<FVector2D>::Get())
			{
				FVector2D& Vec = *static_cast<FVector2D*>(ValuePtr);
				if (JsonObj->HasField(TEXT("x"))) Vec.X = JsonObj->GetNumberField(TEXT("x"));
				if (JsonObj->HasField(TEXT("y"))) Vec.Y = JsonObj->GetNumberField(TEXT("y"));
				return true;
			}

			// FRotator
			if (Struct == TBaseStructure<FRotator>::Get())
			{
				FRotator& Rot = *static_cast<FRotator*>(ValuePtr);
				if (JsonObj->HasField(TEXT("pitch"))) Rot.Pitch = JsonObj->GetNumberField(TEXT("pitch"));
				if (JsonObj->HasField(TEXT("yaw")))   Rot.Yaw   = JsonObj->GetNumberField(TEXT("yaw"));
				if (JsonObj->HasField(TEXT("roll")))  Rot.Roll  = JsonObj->GetNumberField(TEXT("roll"));
				return true;
			}

			// FLinearColor
			if (Struct == TBaseStructure<FLinearColor>::Get())
			{
				FLinearColor& Color = *static_cast<FLinearColor*>(ValuePtr);
				if (JsonObj->HasField(TEXT("r"))) Color.R = static_cast<float>(JsonObj->GetNumberField(TEXT("r")));
				if (JsonObj->HasField(TEXT("g"))) Color.G = static_cast<float>(JsonObj->GetNumberField(TEXT("g")));
				if (JsonObj->HasField(TEXT("b"))) Color.B = static_cast<float>(JsonObj->GetNumberField(TEXT("b")));
				if (JsonObj->HasField(TEXT("a"))) Color.A = static_cast<float>(JsonObj->GetNumberField(TEXT("a")));
				return true;
			}

			// FColor
			if (Struct == TBaseStructure<FColor>::Get())
			{
				FColor& Color = *static_cast<FColor*>(ValuePtr);
				if (JsonObj->HasField(TEXT("r"))) Color.R = static_cast<uint8>(JsonObj->GetNumberField(TEXT("r")));
				if (JsonObj->HasField(TEXT("g"))) Color.G = static_cast<uint8>(JsonObj->GetNumberField(TEXT("g")));
				if (JsonObj->HasField(TEXT("b"))) Color.B = static_cast<uint8>(JsonObj->GetNumberField(TEXT("b")));
				if (JsonObj->HasField(TEXT("a"))) Color.A = static_cast<uint8>(JsonObj->GetNumberField(TEXT("a")));
				return true;
			}

			// FTransform
			if (Struct == TBaseStructure<FTransform>::Get())
			{
				FTransform& Transform = *static_cast<FTransform*>(ValuePtr);

				if (JsonObj->HasField(TEXT("translation")))
				{
					const TSharedPtr<FJsonObject>& LocObj = JsonObj->GetObjectField(TEXT("translation"));
					FVector Loc = Transform.GetLocation();
					if (LocObj->HasField(TEXT("x"))) Loc.X = LocObj->GetNumberField(TEXT("x"));
					if (LocObj->HasField(TEXT("y"))) Loc.Y = LocObj->GetNumberField(TEXT("y"));
					if (LocObj->HasField(TEXT("z"))) Loc.Z = LocObj->GetNumberField(TEXT("z"));
					Transform.SetLocation(Loc);
				}

				if (JsonObj->HasField(TEXT("rotation")))
				{
					const TSharedPtr<FJsonObject>& RotObj = JsonObj->GetObjectField(TEXT("rotation"));
					FRotator Rot = Transform.Rotator();
					if (RotObj->HasField(TEXT("pitch"))) Rot.Pitch = RotObj->GetNumberField(TEXT("pitch"));
					if (RotObj->HasField(TEXT("yaw")))   Rot.Yaw   = RotObj->GetNumberField(TEXT("yaw"));
					if (RotObj->HasField(TEXT("roll")))  Rot.Roll  = RotObj->GetNumberField(TEXT("roll"));
					Transform.SetRotation(FQuat(Rot));
				}

				if (JsonObj->HasField(TEXT("scale")))
				{
					const TSharedPtr<FJsonObject>& ScaleObj = JsonObj->GetObjectField(TEXT("scale"));
					FVector Scale = Transform.GetScale3D();
					if (ScaleObj->HasField(TEXT("x"))) Scale.X = ScaleObj->GetNumberField(TEXT("x"));
					if (ScaleObj->HasField(TEXT("y"))) Scale.Y = ScaleObj->GetNumberField(TEXT("y"));
					if (ScaleObj->HasField(TEXT("z"))) Scale.Z = ScaleObj->GetNumberField(TEXT("z"));
					Transform.SetScale3D(Scale);
				}

				return true;
			}

			// Generic struct — recursively write each field present in JSON
			bool bAllSuccess = true;
			for (const auto& Pair : JsonObj->Values)
			{
				FProperty* ChildProp = Struct->FindPropertyByName(*Pair.Key);
				if (!ChildProp)
				{
					OutErrors.Add(FString::Printf(TEXT("%s.%s: property not found on struct '%s'"),
						*FieldPath, *Pair.Key, *Struct->GetName()));
					bAllSuccess = false;
					continue;
				}

				void* ChildValuePtr = ChildProp->ContainerPtrToValuePtr<void>(ValuePtr);
				FString ChildPath = FieldPath.IsEmpty() ? Pair.Key : FString::Printf(TEXT("%s.%s"), *FieldPath, *Pair.Key);
				if (!JsonValueToPropertyInternal(Pair.Value, ChildProp, ChildValuePtr, OutErrors, ChildPath, Depth + 1))
				{
					bAllSuccess = false;
				}
			}
			return bAllSuccess;
		}

		// --- Array ---
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			if (JsonValue->Type != EJson::Array)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: expected array"), *FieldPath));
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonValue->AsArray();
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			ArrayHelper.EmptyAndAddValues(JsonArray.Num());

			bool bAllSuccess = true;
			for (int32 i = 0; i < JsonArray.Num(); ++i)
			{
				void* ElementPtr = ArrayHelper.GetRawPtr(i);
				FString ElemPath = FString::Printf(TEXT("%s[%d]"), *FieldPath, i);
				if (!JsonValueToPropertyInternal(JsonArray[i], ArrayProp->Inner, ElementPtr, OutErrors, ElemPath, Depth + 1))
				{
					bAllSuccess = false;
				}
			}
			return bAllSuccess;
		}

		// Fallback: try ImportText
		if (JsonValue->Type == EJson::String)
		{
			FString ImportStr = JsonValue->AsString();
			const TCHAR* ImportResult = Property->ImportText_Direct(*ImportStr, ValuePtr, nullptr, PPF_None);
			if (ImportResult)
			{
				return true;
			}
			OutErrors.Add(FString::Printf(TEXT("%s: ImportText failed with value '%s'"), *FieldPath, *ImportStr));
			return false;
		}

		OutErrors.Add(FString::Printf(TEXT("%s: unsupported property type '%s'"),
			*FieldPath, *Property->GetClass()->GetName()));
		return false;
	}
}

bool MCPStructJsonConverter::JsonValueToProperty(
	const TSharedPtr<FJsonValue>& JsonValue,
	const FProperty* Property,
	void* ValuePtr,
	TArray<FString>& OutErrors)
{
	return JsonValueToPropertyInternal(JsonValue, Property, ValuePtr, OutErrors, Property->GetName(), 0);
}

// ============================================================================
// StructToJson
// ============================================================================

bool MCPStructJsonConverter::StructToJson(
	const UScriptStruct* StructDefinition,
	const void* StructData,
	TSharedPtr<FJsonObject>& OutJson,
	TArray<FString>& OutErrors)
{
	if (!StructDefinition || !StructData)
	{
		OutErrors.Add(TEXT("StructToJson: null StructDefinition or StructData"));
		return false;
	}

	OutJson = MakeShared<FJsonObject>();

	for (TFieldIterator<FProperty> It(StructDefinition); It; ++It)
	{
		const FProperty* Prop = *It;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructData);
		TSharedPtr<FJsonValue> JsonVal = PropertyToJsonValueInternal(Prop, ValuePtr, 0);
		OutJson->SetField(Prop->GetName(), JsonVal);
	}

	return true;
}

// ============================================================================
// JsonToStruct
// ============================================================================

bool MCPStructJsonConverter::JsonToStruct(
	const TSharedPtr<FJsonObject>& JsonObject,
	const UScriptStruct* StructDefinition,
	void* StructData,
	TArray<FString>& OutErrors,
	bool bPartialPatch)
{
	if (!JsonObject.IsValid() || !StructDefinition || !StructData)
	{
		OutErrors.Add(TEXT("JsonToStruct: null arguments"));
		return false;
	}

	bool bAllSuccess = true;

	if (bPartialPatch)
	{
		// Only write fields present in JSON
		for (const auto& Pair : JsonObject->Values)
		{
			FProperty* Prop = StructDefinition->FindPropertyByName(*Pair.Key);
			if (!Prop)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: property not found on struct '%s'"),
					*Pair.Key, *StructDefinition->GetName()));
				bAllSuccess = false;
				continue;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructData);
			if (!JsonValueToPropertyInternal(Pair.Value, Prop, ValuePtr, OutErrors, Pair.Key, 0))
			{
				bAllSuccess = false;
			}
		}
	}
	else
	{
		// Full write — process all JSON fields, report unknown fields
		for (const auto& Pair : JsonObject->Values)
		{
			FProperty* Prop = StructDefinition->FindPropertyByName(*Pair.Key);
			if (!Prop)
			{
				OutErrors.Add(FString::Printf(TEXT("%s: property not found on struct '%s'"),
					*Pair.Key, *StructDefinition->GetName()));
				bAllSuccess = false;
				continue;
			}

			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructData);
			if (!JsonValueToPropertyInternal(Pair.Value, Prop, ValuePtr, OutErrors, Pair.Key, 0))
			{
				bAllSuccess = false;
			}
		}
	}

	return bAllSuccess;
}

// ============================================================================
// BuildFieldSchemaArray
// ============================================================================

namespace
{
	TSharedPtr<FJsonObject> BuildSingleFieldSchema(const FProperty* Property)
	{
		TSharedPtr<FJsonObject> FieldObj = MakeShared<FJsonObject>();
		FieldObj->SetStringField(TEXT("name"), Property->GetName());
		FieldObj->SetStringField(TEXT("type"), MCPStructJsonConverter::MapPropertyTypeString(Property));

		bool bEditable = !!(Property->PropertyFlags & (CPF_Edit | CPF_BlueprintVisible));
		bool bReadOnly = !!(Property->PropertyFlags & CPF_EditConst);
		FieldObj->SetBoolField(TEXT("editable"), bEditable && !bReadOnly);

		// Enum values
		UEnum* Enum = nullptr;
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			Enum = ByteProp->Enum;
		}
		else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			Enum = EnumProp->GetEnum();
		}

		if (Enum)
		{
			TArray<TSharedPtr<FJsonValue>> EnumValues;
			for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip _MAX
			{
				if (!Enum->HasMetaData(TEXT("Hidden"), i))
				{
					EnumValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
				}
			}
			FieldObj->SetArrayField(TEXT("enum_values"), EnumValues);
			FieldObj->SetStringField(TEXT("enum_type"), Enum->GetPathName());
		}

		// Object/class reference base class
		if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			if (ObjProp->PropertyClass)
			{
				FieldObj->SetStringField(TEXT("allowed_base_class"), ObjProp->PropertyClass->GetPathName());
			}
		}
		if (const FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			if (ClassProp->MetaClass)
			{
				FieldObj->SetStringField(TEXT("allowed_base_class"), ClassProp->MetaClass->GetPathName());
			}
		}

		// Array inner type
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FieldObj->SetStringField(TEXT("inner_type"), MCPStructJsonConverter::MapPropertyTypeString(ArrayProp->Inner));
		}

		// Struct path for struct properties
		if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct)
			{
				FieldObj->SetStringField(TEXT("struct_path"), StructProp->Struct->GetPathName());
			}
		}

		return FieldObj;
	}
}

TArray<TSharedPtr<FJsonValue>> MCPStructJsonConverter::BuildFieldSchemaArray(
	const UScriptStruct* StructDefinition)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	if (!StructDefinition) return Result;

	for (TFieldIterator<FProperty> It(StructDefinition); It; ++It)
	{
		Result.Add(MakeShared<FJsonValueObject>(BuildSingleFieldSchema(*It)));
	}

	return Result;
}

// ============================================================================
// BuildFieldSchemaArrayForClass
// ============================================================================

TArray<TSharedPtr<FJsonValue>> MCPStructJsonConverter::BuildFieldSchemaArrayForClass(
	const UClass* Class)
{
	TArray<TSharedPtr<FJsonValue>> Result;
	if (!Class) return Result;

	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		const FProperty* Prop = *It;

		// Skip base UObject and UDataAsset properties
		UStruct* Owner = Prop->GetOwnerStruct();
		if (Owner == UObject::StaticClass() || Owner == UDataAsset::StaticClass())
		{
			continue;
		}

		// Only include editable properties
		if (!(Prop->PropertyFlags & (CPF_Edit | CPF_BlueprintVisible)))
		{
			continue;
		}

		Result.Add(MakeShared<FJsonValueObject>(BuildSingleFieldSchema(Prop)));
	}

	return Result;
}

// ============================================================================
// ValidateJsonAgainstStruct
// ============================================================================

TArray<TSharedPtr<FJsonObject>> MCPStructJsonConverter::ValidateJsonAgainstStruct(
	const TSharedPtr<FJsonObject>& JsonObject,
	const UScriptStruct* StructDefinition)
{
	TArray<TSharedPtr<FJsonObject>> Errors;

	if (!JsonObject.IsValid() || !StructDefinition)
	{
		TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
		Err->SetStringField(TEXT("code"), TEXT("InvalidArgument"));
		Err->SetStringField(TEXT("field"), TEXT(""));
		Err->SetStringField(TEXT("message"), TEXT("Null JSON or struct definition"));
		Errors.Add(Err);
		return Errors;
	}

	// Check for unknown fields
	for (const auto& Pair : JsonObject->Values)
	{
		FProperty* Prop = StructDefinition->FindPropertyByName(*Pair.Key);
		if (!Prop)
		{
			TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
			Err->SetStringField(TEXT("code"), TEXT("FieldNotFound"));
			Err->SetStringField(TEXT("field"), Pair.Key);
			Err->SetStringField(TEXT("message"),
				FString::Printf(TEXT("Field '%s' does not exist on struct '%s'"),
					*Pair.Key, *StructDefinition->GetName()));
			Errors.Add(Err);
			continue;
		}

		// Validate type compatibility
		FString TypeStr = MapPropertyTypeString(Prop);
		const TSharedPtr<FJsonValue>& Val = Pair.Value;

		if (TypeStr == TEXT("bool"))
		{
			if (Val->Type != EJson::Boolean && Val->Type != EJson::Number)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected bool, got JSON type %d"), *Pair.Key, static_cast<int32>(Val->Type)));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("int32") || TypeStr == TEXT("int64") ||
				 TypeStr == TEXT("float") || TypeStr == TEXT("double") || TypeStr == TEXT("uint8"))
		{
			double Dummy;
			if (!TryGetNumber(Val, Dummy))
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected number for %s"), *Pair.Key, *TypeStr));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("string") || TypeStr == TEXT("name") || TypeStr == TEXT("text"))
		{
			if (Val->Type != EJson::String)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected string"), *Pair.Key));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("enum"))
		{
			if (Val->Type == EJson::String)
			{
				// Validate enum value name
				UEnum* Enum = nullptr;
				if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
				{
					Enum = ByteProp->Enum;
				}
				else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
				{
					Enum = EnumProp->GetEnum();
				}

				if (Enum)
				{
					FString EnumStr = Val->AsString();
					int64 EnumVal = Enum->GetValueByNameString(EnumStr);
					if (EnumVal == INDEX_NONE)
					{
						TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
						Err->SetStringField(TEXT("code"), TEXT("InvalidEnumValue"));
						Err->SetStringField(TEXT("field"), Pair.Key);
						Err->SetStringField(TEXT("message"),
							FString::Printf(TEXT("%s: '%s' is not a valid value for enum '%s'"),
								*Pair.Key, *EnumStr, *Enum->GetName()));
						Errors.Add(Err);
					}
				}
			}
			else if (Val->Type != EJson::Number)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected string or number for enum"), *Pair.Key));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("vector") || TypeStr == TEXT("vector2d") ||
				 TypeStr == TEXT("rotator") || TypeStr == TEXT("linear_color") ||
				 TypeStr == TEXT("color") || TypeStr == TEXT("transform") ||
				 TypeStr == TEXT("struct"))
		{
			if (Val->Type != EJson::Object)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected object for %s"), *Pair.Key, *TypeStr));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("object_reference") || TypeStr == TEXT("class_reference") ||
				 TypeStr == TEXT("soft_object_reference") || TypeStr == TEXT("soft_class_reference"))
		{
			if (Val->Type != EJson::String && Val->Type != EJson::Null)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected string or null for %s"), *Pair.Key, *TypeStr));
				Errors.Add(Err);
			}
		}
		else if (TypeStr == TEXT("array"))
		{
			if (Val->Type != EJson::Array)
			{
				TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>();
				Err->SetStringField(TEXT("code"), TEXT("TypeMismatch"));
				Err->SetStringField(TEXT("field"), Pair.Key);
				Err->SetStringField(TEXT("message"),
					FString::Printf(TEXT("%s: expected array"), *Pair.Key));
				Errors.Add(Err);
			}
		}
	}

	return Errors;
}
