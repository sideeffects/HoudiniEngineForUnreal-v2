
#include "HoudiniStringResolver.h"
#include "HoudiniEngineRuntimeUtils.h"

void FHoudiniStringResolver::GetTokensAsStringMap(TMap<FString,FString>& OutTokens) const
{
	for (auto& Elem : CachedTokens)
	{
		OutTokens.Add(Elem.Key, Elem.Value.StringValue);
	}
}

void FHoudiniStringResolver::SetToken(const FString& InName, const FString& InValue)
{
	CachedTokens.Add(InName, InValue);
}

void FHoudiniStringResolver::SetTokensFromStringMap(const TMap<FString, FString>& InTokens, bool bClearTokens)
{
	if (bClearTokens)
	{
		CachedTokens.Empty();
	}

	for (auto& Elem : InTokens)
	{
		CachedTokens.Add(Elem.Key, Elem.Value);
	}
}



FString FHoudiniStringResolver::ResolveString(
	const FString& InString) const
{
	const FString Result = FString::Format(*InString, CachedTokens);
	return Result;
}

//void FHoudiniStringResolver::SetCurrentWorld(UWorld* InWorld)
//{
//	SetAttribute("world", InWorld->GetPathName());
//}

FString FHoudiniAttributeResolver::ResolveAttribute(
	const FString& InAttrName,
	const FString& InDefaultValue) const
{
	if (!CachedAttributes.Contains(InAttrName))
	{
		return ResolveString(InDefaultValue);
	}
	FString AttrStr = CachedAttributes.FindChecked(InAttrName);
	return ResolveString(AttrStr);
}

//FString FHoudiniStringResolver::GetTempFolderArgument() const
//{
//	// The actual temp directory should have been supplied externally
//	if (Tokens.Contains(TEXT("temp")))
//		return Tokens.FindChecked(TEXT("temp"));
//
//	UE_LOG(LogTemp, Warning, TEXT("[GetBakeFolderArgument] Could not find 'temp' argument. Using fallback value."));
//	return TEXT("/Game/Content/HoudiniEngine/Temp"); // Fallback value
//}
//
//FString FHoudiniStringResolver::GetBakeFolderArgument() const
//{
//	// The actual bake directory should have been supplied externally
//	if (Tokens.Contains(TEXT("bake")))
//		return Tokens.FindChecked(TEXT("bake"));
//	UE_LOG(LogTemp, Warning, TEXT("[GetBakeFolderArgument] Could not find 'bake' argument. Using fallback value."));
//	return TEXT("/Game/Content/HoudiniEngine/Bake"); // Fallback value
//}
//
//FString FHoudiniStringResolver::GetOutputFolderForPackageMode(EPackageMode PackageMode) const
//{
//	switch (PackageMode)
//	{
//	case EPackageMode::Bake:
//		return GetBakeFolderArgument();
//	case EPackageMode::CookToLevel:
//	case EPackageMode::CookToTemp:
//		return GetTempFolderArgument();
//	}
//	return "";
//}

 void FHoudiniAttributeResolver::SetCachedAttributes(const TMap<FString,FString>& Attributes)
 { 
	 CachedAttributes = Attributes; 
 }

void FHoudiniAttributeResolver::SetAttribute(const FString& InName, const FString& InValue)
{
	CachedAttributes.Add(InName, InValue);
}

FString FHoudiniAttributeResolver::ResolveFullLevelPath() const
{
	FString OutputFolder = TEXT("/Game/Content/HoudiniEngine/Temp");

	const FStringFormatArg* BaseDir = CachedTokens.Find(TEXT("out_basedir"));
	if (BaseDir)
		OutputFolder = BaseDir->StringValue;

	FString LevelPathAttr = ResolveAttribute(HAPI_UNREAL_ATTRIB_LEVEL_PATH, TEXT("{out}"));
	if (LevelPathAttr.IsEmpty())
		return OutputFolder;

	return FHoudiniEngineRuntimeUtils::JoinPaths(OutputFolder, LevelPathAttr);
}

FString FHoudiniAttributeResolver::ResolveOutputName() const
{
	FString OutputAttribName;

	if (CachedAttributes.Contains(HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2))
		OutputAttribName = HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V2;
	else
		OutputAttribName = HAPI_UNREAL_ATTRIB_CUSTOM_OUTPUT_NAME_V1;

	return ResolveAttribute(OutputAttribName, TEXT("{object_name}"));
}

