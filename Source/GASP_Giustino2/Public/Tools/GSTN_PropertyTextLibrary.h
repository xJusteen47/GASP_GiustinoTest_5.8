#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_PropertyTextLibrary.generated.h"

/**
 * Editor scripting helper: reads or writes any reflected property of an object as text (FProperty::ExportText / ImportText), also the ones
 * Python cannot see (no BlueprintVisible), e.g. the nodes of a GameplayCameras rig. Tools only, never called by the game.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_PropertyTextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The property PropertyName of Object as text; empty when there is no such property */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Tools")
	static FString GetPropertyText(UObject* Object, FName PropertyName);

	/** Sets the property PropertyName of Object from text (Modify() first, so the package is dirtied). False when it does not exist or parse */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Tools")
	static bool SetPropertyText(UObject* Object, FName PropertyName, const FString& Text);

	/** Every property of Object's class as "Name : Type" (Blueprint variables keep their GUID suffix) */
	UFUNCTION(BlueprintCallable, Category = "GSTN|Tools")
	static TArray<FString> ListProperties(UObject* Object);
};
