
#pragma once

#include "CoreMinimal.h"

#include "HoudiniStringResolver.generated.h"

USTRUCT()
struct HOUDINIENGINE_API FHoudiniStringResolver
{

	GENERATED_USTRUCT_BODY();

protected:

	// Named arguments that will be substituted into attribute values upon retrieval.
	TMap<FString, FStringFormatArg> CachedTokens;


public:
	// ----------------------------------
	// Named argument accessors
	// ----------------------------------

	TMap<FString, FStringFormatArg>& GetCachedTokens() { return CachedTokens; }


	// Set a named argument that will be used for argument replacement during GetAttribute calls.
	void SetToken(const FString& InName, const FString& InValue);
	
	void GetTokensAsStringMap(TMap<FString, FString>& OutTokens) const;

	void SetTokensFromStringMap(const TMap<FString, FString>& InValue, bool bClearTokens=true);

	// Resolve a string by substituting `Tokens` as named arguments during string formatting.
	FString ResolveString(const FString& InStr) const;

};


USTRUCT()
struct HOUDINIENGINE_API FHoudiniAttributeResolver : public FHoudiniStringResolver
{
	GENERATED_USTRUCT_BODY();

protected:
	TMap<FString, FString> CachedAttributes;

public:

	void SetCachedAttributes(const TMap<FString,FString>& Attributes);

	// Return a mutable reference to the cached attributes.
	TMap<FString, FString>& GetCachedAttributes() { return CachedAttributes; }

	// Return immutable reference to cached attributes.
	const TMap<FString, FString>& GetCachedAttributes() const { return CachedAttributes; }

	// Set an attribute with the given name and value in the attribute cache.
	void SetAttribute(const FString& InName, const FString& InValue);

	// Try to resolve an attribute with the given name. If the attribute could not be 
	// found, use DefaultValue as a fallback.
	FString ResolveAttribute(const FString& InAttrName, const FString& InDefaultValue) const;

	// ----------------------------------
	// Helpers
	// ----------------------------------

	// Helper for resolving the `unreal_level_path` attribute.
	FString ResolveFullLevelPath() const;

	// Helper for resolver custom output name attributes.
	FString ResolveOutputName() const;

};