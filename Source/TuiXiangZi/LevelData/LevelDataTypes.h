#pragma once

#include "CoreMinimal.h"
#include "LevelDataTypes.generated.h"

USTRUCT(BlueprintType)
struct FMechanismGroupStyleData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GroupId = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor BaseColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ActiveColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DisplayName;
};

USTRUCT(BlueprintType)
struct FCellData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint GridPos = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CellType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName VisualStyleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 GroupId = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ExtraParam = 0;
};

USTRUCT(BlueprintType)
struct FLevelData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FCellData> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint PlayerStart = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FIntPoint> BoxPositions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FMechanismGroupStyleData> GroupStyles;
};
