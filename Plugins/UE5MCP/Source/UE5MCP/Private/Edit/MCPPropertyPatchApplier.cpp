// Copyright Epic Games, Inc. All Rights Reserved.

#include "Edit/MCPPropertyPatchApplier.h"
#include "Edit/MCPEditErrors.h"

#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogMCPPatchApplier, Log, All);

// ============================================================================
// Forward declarations for internal helpers
// ============================================================================

namespace
{
	bool WriteJsonToProperty(FProperty* Property, void* ContainerPtr, const TSharedPtr<FJsonValue>& JsonValue, FString& OutError);
}

// ============================================================================
// ResolvePropertyPath
// ============================================================================

MCPPropertyPatchApplier::FResolvedProperty MCPPropertyPatchApplier::ResolvePropertyPath(
	UObject* Object, const FString& PropertyPath)
{
	FResolvedProperty Result;

	if (!Object)
	{
		Result.ErrorMessage = TEXT("Object is null");
		return Result;
	}

	if (PropertyPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Property path is empty");
		return Result;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);

	if (Segments.Num() == 0)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid property path: '%s'"), *PropertyPath);
		return Result;
	}

	// Start from the object's class
	UStruct* CurrentStruct = Object->GetClass();
	void* CurrentContainer = Object;

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const FString& Segment = Segments[i];

		FProperty* Prop = CurrentStruct->FindPropertyByName(*Segment);
		if (!Prop)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("Property '%s' not found on '%s' (full path: '%s')"),
				*Segment, *CurrentStruct->GetName(), *PropertyPath);
			return Result;
		}

		if (i == Segments.Num() - 1)
		{
			// Final segment — this is the target property
			Result.Property = Prop;
			Result.ContainerPtr = CurrentContainer;
			return Result;
		}

		// Intermediate segment — must be a struct property to drill into
		FStructProperty* StructProp = CastField<FStructProperty>(Prop);
		if (!StructProp)
		{
			Result.ErrorMessage = FString::Printf(
				TEXT("'%s' is not a struct property; cannot resolve further segments in path '%s'"),
				*Segment, *PropertyPath);
			return Result;
		}

		CurrentContainer = StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
		CurrentStruct = StructProp->Struct;
	}

	// Should not reach here
	Result.ErrorMessage = TEXT("Unexpected end of property path resolution");
	return Result;
}

// ============================================================================
// ReadPropertyAsJson
// ============================================================================

TSharedPtr<FJsonValue> MCPPropertyPatchApplier::ReadPropertyAsJson(
	FProperty* Property, const void* ContainerPtr)
{
	if (!Property || !ContainerPtr)
	{
		return MakeShared<FJsonValueNull>();
	}

	const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

	// --- Bool ---
	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
	}

	// --- Integer types ---
	if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(static_cast<double>(IntProp->GetPropertyValue(ValuePtr)));
	}
	if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
	{
		return MakeShared<FJsonValueNumber>(static_cast<double>(Int64Prop->GetPropertyValue(ValuePtr)));
	}

	// --- Float types ---
	if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(static_cast<double>(FloatProp->GetPropertyValue(ValuePtr)));
	}
	if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
	{
		return MakeShared<FJsonValueNumber>(DoubleProp->GetPropertyValue(ValuePtr));
	}

	// --- String types ---
	if (Property->GetClass()->GetFName() == TEXT("StrProperty"))
	{
		return MakeShared<FJsonValueString>(*static_cast<const FString*>(ValuePtr));
	}
	if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
	}
	if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
	}

	// --- Enum (FByteProperty with Enum) ---
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
			FString EnumName = ByteProp->Enum->GetNameStringByValue(static_cast<int64>(ByteValue));
			return MakeShared<FJsonValueString>(EnumName);
		}
		return MakeShared<FJsonValueNumber>(static_cast<double>(ByteProp->GetPropertyValue(ValuePtr)));
	}

	// --- Enum (FEnumProperty) ---
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			FString EnumName = Enum->GetNameStringByValue(EnumValue);
			return MakeShared<FJsonValueString>(EnumName);
		}
		return MakeShared<FJsonValueNumber>(static_cast<double>(EnumValue));
	}

	// --- Soft Object ---
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		FSoftObjectPath Path = SoftPtr.GetUniqueID();
		if (Path.IsNull())
		{
			return MakeShared<FJsonValueNull>();
		}
		return MakeShared<FJsonValueString>(Path.ToString());
	}

	// --- Soft Class ---
	if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		FSoftObjectPath Path = SoftPtr.GetUniqueID();
		if (Path.IsNull())
		{
			return MakeShared<FJsonValueNull>();
		}
		return MakeShared<FJsonValueString>(Path.ToString());
	}

	// --- Object reference (after soft types) ---
	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		UObject* ObjValue = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (!ObjValue)
		{
			return MakeShared<FJsonValueNull>();
		}
		return MakeShared<FJsonValueString>(ObjValue->GetPathName());
	}

	// --- Struct ---
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		UScriptStruct* Struct = StructProp->Struct;
		const FString StructName = Struct->GetFName().ToString();

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
			Obj->SetObjectField(TEXT("location"), LocObj);

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
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* ChildProp = *It;
				TSharedPtr<FJsonValue> ChildVal = ReadPropertyAsJson(ChildProp, ValuePtr);
				Obj->SetField(ChildProp->GetName(), ChildVal);
			}
			return MakeShared<FJsonValueObject>(Obj);
		}
	}

	// --- Array ---
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		JsonArray.Reserve(ArrayHelper.Num());

		for (int32 i = 0; i < ArrayHelper.Num(); ++i)
		{
			const void* ElementPtr = ArrayHelper.GetRawPtr(i);
			// For array elements, the element IS the value (not a container with offset)
			// We need to read using the inner property with the element as the "container"
			// But FProperty::ContainerPtrToValuePtr expects the container, so we use the raw ptr
			// directly via the inner property's value read functions.
			TSharedPtr<FJsonValue> ElemVal = ReadPropertyAsJson(ArrayProp->Inner, ArrayHelper.GetRawPtr(i));
			JsonArray.Add(ElemVal);
		}
		return MakeShared<FJsonValueArray>(JsonArray);
	}

	// Fallback
	FString ExportedValue;
	Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
	return MakeShared<FJsonValueString>(ExportedValue);
}

// ============================================================================
// WriteJsonToProperty (internal helper)
// ============================================================================

namespace
{
	/**
	 * Attempts to extract a double from a JSON value, handling both Number and String types.
	 */
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

	/**
	 * Write a JSON value into the given FProperty at ContainerPtr.
	 * Returns true on success. On failure, sets OutError.
	 */
	bool WriteJsonToProperty(FProperty* Property, void* ContainerPtr,
		const TSharedPtr<FJsonValue>& JsonValue, FString& OutError)
	{
		if (!Property || !ContainerPtr || !JsonValue.IsValid())
		{
			OutError = TEXT("Invalid arguments to WriteJsonToProperty");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

		// --- Bool ---
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			if (JsonValue->Type == EJson::Boolean)
			{
				BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsBool());
				return true;
			}
			// Accept number: 0 = false, nonzero = true
			if (JsonValue->Type == EJson::Number)
			{
				BoolProp->SetPropertyValue(ValuePtr, JsonValue->AsNumber() != 0.0);
				return true;
			}
			OutError = TEXT("Expected bool or number for bool property");
			return false;
		}

		// --- Int ---
		if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				IntProp->SetPropertyValue(ValuePtr, static_cast<int32>(NumVal));
				return true;
			}
			OutError = TEXT("Expected number for int32 property");
			return false;
		}

		// --- Int64 ---
		if (FInt64Property* Int64Prop = CastField<FInt64Property>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				Int64Prop->SetPropertyValue(ValuePtr, static_cast<int64>(NumVal));
				return true;
			}
			OutError = TEXT("Expected number for int64 property");
			return false;
		}

		// --- Float ---
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				FloatProp->SetPropertyValue(ValuePtr, static_cast<float>(NumVal));
				return true;
			}
			OutError = TEXT("Expected number for float property");
			return false;
		}

		// --- Double ---
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
		{
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				DoubleProp->SetPropertyValue(ValuePtr, NumVal);
				return true;
			}
			OutError = TEXT("Expected number for double property");
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
			OutError = TEXT("Expected string for FString property");
			return false;
		}

		// --- FName ---
		if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				NameProp->SetPropertyValue(ValuePtr, FName(*JsonValue->AsString()));
				return true;
			}
			OutError = TEXT("Expected string for FName property");
			return false;
		}

		// --- FText ---
		if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				TextProp->SetPropertyValue(ValuePtr, FText::FromString(JsonValue->AsString()));
				return true;
			}
			OutError = TEXT("Expected string for FText property");
			return false;
		}

		// --- FByteProperty (with Enum) ---
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (ByteProp->Enum)
			{
				if (JsonValue->Type == EJson::String)
				{
					FString EnumStr = JsonValue->AsString();
					int64 EnumVal = ByteProp->Enum->GetValueByNameString(EnumStr);
					if (EnumVal == INDEX_NONE)
					{
						OutError = FString::Printf(TEXT("'%s' is not a valid value for enum '%s'"),
							*EnumStr, *ByteProp->Enum->GetName());
						return false;
					}
					ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(EnumVal));
					return true;
				}
				if (JsonValue->Type == EJson::Number)
				{
					ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(JsonValue->AsNumber()));
					return true;
				}
				OutError = TEXT("Expected string (enum name) or number for enum byte property");
				return false;
			}
			// Plain byte
			double NumVal;
			if (TryGetNumber(JsonValue, NumVal))
			{
				ByteProp->SetPropertyValue(ValuePtr, static_cast<uint8>(NumVal));
				return true;
			}
			OutError = TEXT("Expected number for uint8 property");
			return false;
		}

		// --- FEnumProperty ---
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();

			if (JsonValue->Type == EJson::String)
			{
				FString EnumStr = JsonValue->AsString();
				int64 EnumVal = Enum ? Enum->GetValueByNameString(EnumStr) : INDEX_NONE;
				if (EnumVal == INDEX_NONE)
				{
					OutError = FString::Printf(TEXT("'%s' is not a valid value for enum '%s'"),
						*EnumStr, Enum ? *Enum->GetName() : TEXT("unknown"));
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
			OutError = TEXT("Expected string (enum name) or number for enum property");
			return false;
		}

		// --- Soft Object ---
		if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FSoftObjectPath Path(JsonValue->AsString());
				FSoftObjectPtr SoftPtr(Path);
				SoftObjProp->SetPropertyValue(ValuePtr, SoftPtr);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				SoftObjProp->SetPropertyValue(ValuePtr, FSoftObjectPtr());
				return true;
			}
			OutError = TEXT("Expected string (asset path) or null for soft object property");
			return false;
		}

		// --- Soft Class ---
		if (FSoftClassProperty* SoftClassProp = CastField<FSoftClassProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FSoftObjectPath Path(JsonValue->AsString());
				FSoftObjectPtr SoftPtr(Path);
				SoftClassProp->SetPropertyValue(ValuePtr, SoftPtr);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				SoftClassProp->SetPropertyValue(ValuePtr, FSoftObjectPtr());
				return true;
			}
			OutError = TEXT("Expected string (class path) or null for soft class property");
			return false;
		}

		// --- Class property (TSubclassOf) ---
		if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FString ClassPath = JsonValue->AsString();
				UClass* LoadedClass = StaticLoadClass(UObject::StaticClass(), nullptr, *ClassPath);
				if (!LoadedClass)
				{
					OutError = FString::Printf(TEXT("Could not load class '%s'"), *ClassPath);
					return false;
				}
				if (ClassProp->MetaClass && !LoadedClass->IsChildOf(ClassProp->MetaClass))
				{
					OutError = FString::Printf(TEXT("Class '%s' is not a subclass of '%s'"),
						*ClassPath, *ClassProp->MetaClass->GetName());
					return false;
				}
				ClassProp->SetObjectPropertyValue(ValuePtr, LoadedClass);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				ClassProp->SetObjectPropertyValue(ValuePtr, nullptr);
				return true;
			}
			OutError = TEXT("Expected string (class path) or null for class property");
			return false;
		}

		// --- Object reference (after soft/class types) ---
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
		{
			if (JsonValue->Type == EJson::String)
			{
				FString ObjPath = JsonValue->AsString();
				UObject* LoadedObj = StaticLoadObject(
					ObjProp->PropertyClass ? static_cast<UClass*>(ObjProp->PropertyClass) : UObject::StaticClass(),
					nullptr, *ObjPath);
				if (!LoadedObj)
				{
					OutError = FString::Printf(TEXT("Could not load object '%s'"), *ObjPath);
					return false;
				}
				ObjProp->SetObjectPropertyValue(ValuePtr, LoadedObj);
				return true;
			}
			if (JsonValue->Type == EJson::Null)
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
				return true;
			}
			OutError = TEXT("Expected string (object path) or null for object property");
			return false;
		}

		// --- Struct ---
		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (JsonValue->Type != EJson::Object)
			{
				OutError = TEXT("Expected JSON object for struct property");
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

				if (JsonObj->HasField(TEXT("location")))
				{
					const TSharedPtr<FJsonObject>& LocObj = JsonObj->GetObjectField(TEXT("location"));
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
			for (const auto& Pair : JsonObj->Values)
			{
				FProperty* ChildProp = Struct->FindPropertyByName(*Pair.Key);
				if (!ChildProp)
				{
					UE_LOG(LogMCPPatchApplier, Warning,
						TEXT("Struct '%s' has no property '%s'; skipping."),
						*Struct->GetName(), *Pair.Key);
					continue;
				}

				FString ChildError;
				if (!WriteJsonToProperty(ChildProp, ValuePtr, Pair.Value, ChildError))
				{
					OutError = FString::Printf(TEXT("Struct field '%s': %s"), *Pair.Key, *ChildError);
					return false;
				}
			}
			return true;
		}

		// --- Array ---
		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			if (JsonValue->Type != EJson::Array)
			{
				OutError = TEXT("Expected JSON array for array property");
				return false;
			}

			const TArray<TSharedPtr<FJsonValue>>& JsonArray = JsonValue->AsArray();
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);

			// Clear and resize
			ArrayHelper.EmptyAndAddValues(JsonArray.Num());

			for (int32 i = 0; i < JsonArray.Num(); ++i)
			{
				void* ElementPtr = ArrayHelper.GetRawPtr(i);
				FString ElemError;
				// For array elements, the element pointer IS the container for the inner property
				// since Inner has offset 0 within the array element
				if (!WriteJsonToProperty(ArrayProp->Inner, ElementPtr, JsonArray[i], ElemError))
				{
					OutError = FString::Printf(TEXT("Array element [%d]: %s"), i, *ElemError);
					return false;
				}
			}
			return true;
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
			OutError = FString::Printf(TEXT("ImportText failed for property '%s' with value '%s'"),
				*Property->GetName(), *ImportStr);
			return false;
		}

		OutError = FString::Printf(TEXT("Unsupported property type '%s' for JSON type %d"),
			*Property->GetClass()->GetName(), static_cast<int32>(JsonValue->Type));
		return false;
	}
} // anonymous namespace

// ============================================================================
// ApplyPatch
// ============================================================================

MCPPropertyPatchApplier::FPatchResult MCPPropertyPatchApplier::ApplyPatch(
	UObject* Object, const FString& PropertyPath, const TSharedPtr<FJsonValue>& JsonValue)
{
	FPatchResult Result;

	if (!Object)
	{
		Result.ErrorCode = MCPEditErrors::PropertyNotFound;
		Result.ErrorMessage = TEXT("Object is null");
		return Result;
	}

	if (!JsonValue.IsValid())
	{
		Result.ErrorCode = MCPEditErrors::TypeMismatch;
		Result.ErrorMessage = TEXT("JSON value is null/invalid");
		return Result;
	}

	// Resolve property path
	FResolvedProperty Resolved = ResolvePropertyPath(Object, PropertyPath);
	if (!Resolved.IsValid())
	{
		Result.ErrorCode = MCPEditErrors::PropertyNotFound;
		Result.ErrorMessage = Resolved.ErrorMessage;
		return Result;
	}

	// Read old value
	Result.OldValue = ReadPropertyAsJson(Resolved.Property, Resolved.ContainerPtr);

	// Notify the object that it is about to be modified
	Object->PreEditChange(Resolved.Property);

	// Write new value
	FString WriteError;
	if (!WriteJsonToProperty(Resolved.Property, Resolved.ContainerPtr, JsonValue, WriteError))
	{
		// Determine error code
		if (WriteError.Contains(TEXT("not a valid value for enum")) ||
			WriteError.Contains(TEXT("Could not load")))
		{
			Result.ErrorCode = MCPEditErrors::InvalidObjectReference;
		}
		else
		{
			Result.ErrorCode = MCPEditErrors::TypeMismatch;
		}
		Result.ErrorMessage = WriteError;

		// Still call PostEditChangeProperty to keep the object in a consistent state
		FPropertyChangedEvent ChangedEvent(Resolved.Property);
		Object->PostEditChangeProperty(ChangedEvent);
		return Result;
	}

	// Notify the object that a property has changed
	FPropertyChangedEvent ChangedEvent(Resolved.Property);
	Object->PostEditChangeProperty(ChangedEvent);

	// Read new value
	Result.NewValue = ReadPropertyAsJson(Resolved.Property, Resolved.ContainerPtr);
	Result.bSuccess = true;

	return Result;
}

// ============================================================================
// ApplyPatches (batch)
// ============================================================================

MCPPropertyPatchApplier::FBatchPatchResult MCPPropertyPatchApplier::ApplyPatches(
	UObject* Object, const TArray<TPair<FString, TSharedPtr<FJsonValue>>>& Patches)
{
	FBatchPatchResult BatchResult;

	for (const auto& Patch : Patches)
	{
		FPatchResult PatchResult = ApplyPatch(Object, Patch.Key, Patch.Value);
		if (!PatchResult.bSuccess)
		{
			BatchResult.bAllSuccess = false;
		}
		BatchResult.Results.Add(MoveTemp(PatchResult));
	}

	return BatchResult;
}
