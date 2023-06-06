// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#pragma once

#include "CesiumFeatureTable.h"
#include "CesiumFeatureIdAttribute.h"
#include "CesiumMetadataGenericValue.h"
#include "CesiumMetadataModel.h"
#include "CesiumMetadataPrimitive.h"
#include "Containers/UnrealString.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"
#include "CesiumMetadataUtilityBlueprintLibrary.generated.h"

struct FCesiumFeatureIdAttribute;
struct FCesiumFeatureIdTexture;

UCLASS()
class CESIUMRUNTIME_API UCesiumMetadataUtilityBlueprintLibrary
    : public UBlueprintFunctionLibrary {
  GENERATED_BODY()

public:
  /**
   * Gets the model metadata of a glTF primitive component. If component is
   * not a Cesium glTF primitive component, the returned metadata is empty
   */
  UFUNCTION(
      BlueprintCallable,
      BlueprintPure,
      Category = "Cesium|Metadata|Utility")
  static const FCesiumMetadataModel&
  GetModelMetadata(const UPrimitiveComponent* component);

  /**
   * Gets the primitive metadata of a glTF primitive component. If component is
   * not a Cesium glTF primitive component, the returned metadata is empty
   */
  UFUNCTION(
      BlueprintCallable,
      BlueprintPure,
      Category = "Cesium|Metadata|Utility")
  static const FCesiumMetadataPrimitive&
  GetPrimitiveMetadata(const UPrimitiveComponent* component);

  /**
   * Gets the metadata of a face of a glTF primitive component. If the component
   * is not a Cesium glTF primitive component, the returned metadata is empty.
   * If the primitive has multiple feature tables, the metadata in the first
   * table is returned.
   */
  UFUNCTION(
      BlueprintCallable,
      BlueprintPure,
      Category = "Cesium|Metadata|Utility")
  static TMap<FString, FCesiumMetadataGenericValue>
  GetMetadataValuesForFace(const UPrimitiveComponent* component, int64 faceID);

  /**
   * Gets the metadata as string of a face of a glTF primitive component. If the
   * component is not a Cesium glTF primitive component, the returned metadata
   * is empty. If the primitive has multiple feature tables, the metadata in the
   * first table is returned.
   */
  UFUNCTION(
      BlueprintCallable,
      BlueprintPure,
      Category = "Cesium|Metadata|Utility")
  static TMap<FString, FString> GetMetadataValuesAsStringForFace(
      const UPrimitiveComponent* component,
      int64 faceID);

  PRAGMA_DISABLE_DEPRECATION_WARNINGS
  /**
   * Gets the feature ID associated with a given face for a feature id
   * attribute.
   */
  UFUNCTION(
      BlueprintCallable,
      BlueprintPure,
      Category = "Cesium|Metadata|Utility",
      Meta =
          (DeprecatedFunction,
           DeprecatedMessage =
               "UCesiumMetadataUtilityBlueprintLibrary.GetFeatureIDFromFaceID is deprecated. Use UCesiumPrimitiveFeaturesBlueprintLibrary.GetFeatureIDFromFace instead."))
  static int64 GetFeatureIDFromFaceID(
      UPARAM(ref) const FCesiumMetadataPrimitive& Primitive,
      UPARAM(ref) const FCesiumFeatureIdAttribute& FeatureIDAttribute,
      int64 FaceID);
  PRAGMA_ENABLE_DEPRECATION_WARNINGS
};
