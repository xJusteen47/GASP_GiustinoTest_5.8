#include "Tools/GSTN_PropertyTextLibrary.h"

FString UGSTN_PropertyTextLibrary::GetPropertyText(UObject* Object, FName PropertyName)
{
	const FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
	if (!Property)
	{
		return FString();
	}
	FString Text;
	Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Object), nullptr, Object, PPF_None);
	return Text;
}

bool UGSTN_PropertyTextLibrary::SetPropertyText(UObject* Object, FName PropertyName, const FString& Text)
{
	const FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
	if (!Property)
	{
		return false;
	}
	Object->Modify();
	return Property->ImportText_Direct(*Text, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None) != nullptr;
}

TArray<FString> UGSTN_PropertyTextLibrary::ListProperties(UObject* Object)
{
	TArray<FString> Out;
	if (Object)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			Out.Add(FString::Printf(TEXT("%s : %s"), *It->GetName(), *It->GetCPPType()));
		}
	}
	return Out;
}

