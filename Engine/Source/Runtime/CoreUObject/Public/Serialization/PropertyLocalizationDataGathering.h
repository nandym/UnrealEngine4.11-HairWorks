// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GatherableTextData.h"

enum class EPropertyLocalizationGathererTextFlags : uint8
{
	/**
	 * Automatically detect whether text is editor-only data using the flags available on the properties.
	 */
	None = 0,

	/**
	 * Force text gathered from object properties to be treated as editor-only data.
	 * @note Does not apply to properties gathered from script data (see ForceEditorOnlyScriptData).
	 */
	ForceEditorOnlyProperties = 1<<0,

	/**
	 * Force text gathered from script data to be treated as editor-only data.
	 */
	ForceEditorOnlyScriptData = 1<<1,

	/**
	 * Force all gathered text to be treated as editor-only data.
	 */
	ForceEditorOnly = ForceEditorOnlyProperties | ForceEditorOnlyScriptData,
};
ENUM_CLASS_FLAGS(EPropertyLocalizationGathererTextFlags);

class COREUOBJECT_API FPropertyLocalizationDataGatherer
{
public:
	typedef TFunction<void (const UObject* const, FPropertyLocalizationDataGatherer&, const EPropertyLocalizationGathererTextFlags)> FLocalizationDataGatheringCallback;
	typedef TMap<const UClass*, FLocalizationDataGatheringCallback> FLocalizationDataGatheringCallbackMap;

	FPropertyLocalizationDataGatherer(TArray<FGatherableTextData>& InGatherableTextDataArray, const UPackage* const InPackage);

	void GatherLocalizationDataFromObjectWithCallbacks(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags);
	void GatherLocalizationDataFromObject(const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags);
	void GatherLocalizationDataFromObjectFields(const FString& PathToParent, const UObject* Object, const EPropertyLocalizationGathererTextFlags GatherTextFlags);
	void GatherLocalizationDataFromStructFields(const FString& PathToParent, const UStruct* Struct, const void* StructData, const EPropertyLocalizationGathererTextFlags GatherTextFlags);
	void GatherLocalizationDataFromChildTextProperties(const FString& PathToParent, const UProperty* const Property, const void* const ValueAddress, const EPropertyLocalizationGathererTextFlags GatherTextFlags);
	void GatherLocalizationDataFromTextProperty(const FString& PathToParent, const UTextProperty* const TextProperty, const void* const ValueAddress, const EPropertyLocalizationGathererTextFlags GatherTextFlags);

	void GatherTextInstance(const FText& Text, const FString& Description, const bool bIsEditorOnly);
	void GatherScriptBytecode(const FString& PathToScript, const TArray<uint8>& ScriptData, const bool bIsEditorOnly);

	static FLocalizationDataGatheringCallbackMap& GetTypeSpecificLocalizationDataGatheringCallbacks();

	FORCEINLINE TArray<FGatherableTextData>& GetGatherableTextDataArray() const
	{
		return GatherableTextDataArray;
	}

	FORCEINLINE bool IsObjectValidForGather(const UObject* Object) const
	{
		return AllObjectsInPackage.Contains(Object);
	}

private:
	struct FObjectAndGatherFlags
	{
		FObjectAndGatherFlags(const UObject* InObject, const EPropertyLocalizationGathererTextFlags InGatherTextFlags)
			: Object(InObject)
			, GatherTextFlags(InGatherTextFlags)
			, KeyHash(0)
		{
			KeyHash = HashCombine(KeyHash, GetTypeHash(Object));
			KeyHash = HashCombine(KeyHash, GetTypeHash(GatherTextFlags));
		}

		FORCEINLINE bool operator==(const FObjectAndGatherFlags& Other) const
		{
			return Object == Other.Object 
				&& GatherTextFlags == Other.GatherTextFlags;
		}

		FORCEINLINE bool operator!=(const FObjectAndGatherFlags& Other) const
		{
			return !(*this == Other);
		}

		friend inline uint32 GetTypeHash(const FObjectAndGatherFlags& Key)
		{
			return Key.KeyHash;
		}

		const UObject* Object;
		EPropertyLocalizationGathererTextFlags GatherTextFlags;
		uint32 KeyHash;
	};

	TArray<FGatherableTextData>& GatherableTextDataArray;
	const UPackage* Package;
	TSet<const UObject*> AllObjectsInPackage;
	TSet<FObjectAndGatherFlags> ProcessedObjects;
};
