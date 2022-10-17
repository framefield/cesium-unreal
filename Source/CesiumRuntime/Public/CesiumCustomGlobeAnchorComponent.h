// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#pragma once

#include "Components/ActorComponent.h"
#include "Delegates/IDelegateInstance.h"
#include <glm/gtx/quaternion.hpp>
#include <optional>
#include "CesiumCustomGlobeAnchorComponent.generated.h"

class ACesiumGeoreference;
class ACesium3DTileset;

/**
 * This component can be added to a movable actor to anchor it to the globe
 * and maintain precise placement. When the owning actor is transformed through
 * normal Unreal Engine mechanisms, the internal geospatial coordinates will be
 * automatically updated. The actor position can also be set in terms of
 * Earth-Centered, Eath-Fixed coordinates (ECEF) or Longitude, Latitude, and
 * Height relative to the ellipsoid.
 */
UCLASS(ClassGroup = (Cesium), meta = (BlueprintSpawnableComponent))
class CESIUMRUNTIME_API UCesiumCustomGlobeAnchorComponent
    : public UActorComponent {
  GENERATED_BODY()

private:
  /**
   * The designated tileset actor ensuring that the tileset transformation is
   * applied to the owning actor as well.
   *
   * If this is null, the Component will find and use the first Georeference
   * Actor in the level, or create one if necessary. To get the active/effective
   * Tileset from Blueprints or C++, use ResolvedTileset instead.
   */
  UPROPERTY(
    EditAnywhere,
    BlueprintReadWrite,
    BlueprintGetter = GetTileset,
    BlueprintSetter = SetTileset,
    Category = "Cesium|Tileset",
    Meta = (AllowPrivateAccess))
  ACesium3DTileset* Tileset = nullptr;

  UPROPERTY(
    EditAnywhere,
    BlueprintReadWrite,
    BlueprintGetter = GetTilesetTag,
    BlueprintSetter = SetTilesetTag,
    Category = "Cesium|Tileset",
    Meta = (AllowPrivateAccess))
  FName TilesetTag = FName("World");

  /**
   * The resolved tileset used by this component. This is not serialized
   * because it may point to a Tileset in the PersistentLevel while this
   * component is in a sublevel. If the Tileset property is specified,
   * however then this property will have the same value.
   *
   * This property will be null before ResolveTileset is called, which
   * happens automatically when the component is registered.
   */
  UPROPERTY(
    Transient,
    BlueprintReadOnly,
    Category = "Cesium",
    Meta = (AllowPrivateAccess))
  ACesium3DTileset* ResolvedTileset = nullptr;

  /**
   * The resolved georeference used by this component. This is not serialized
   * because it may point to a Georeference in the PersistentLevel while this
   * component is in a sublevel. If the Georeference property is specified,
   * however then this property will have the same value.
   *
   * This property will be null before ResolveGeoreference is called, which
   * happens automatically when the component is registered.
   */
  UPROPERTY(
    Transient,
    BlueprintReadOnly,
    Category = "Cesium",
    Meta = (AllowPrivateAccess))
  ACesiumGeoreference* ResolvedGeoreference = nullptr;

public:
  /** @copydoc UCesiumCustomGlobeAnchorComponent::Tileset */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  ACesium3DTileset* GetTileset() const;

  /** @copydoc UCesiumCustomGlobeAnchorComponent::Tileset */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  void SetTileset(ACesium3DTileset* NewTileset);

  /** @copydoc UCesiumCustomGlobeAnchorComponent::TilesetTag */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  FName GetTilesetTag() const;

  /** @copydoc UCesiumCustomGlobeAnchorComponent::TilesetTag */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  void SetTilesetTag(FName NewTag);

  /**
 * Resolves the Cesium Georeference to use with this Component. Returns
 * the value of the Georeference property if it is set. Otherwise, finds a
 * Georeference in the World and returns it, creating it if necessary. The
 * resolved Georeference is cached so subsequent calls to this function will
 * return the same instance.
 */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  ACesium3DTileset* ResolveTileset();

  /**
   * Invalidates the cached resolved georeference, unsubscribing from it and
   * setting it to null. The next time ResolveGeoreference is called, the
   * Georeference will be re-resolved and re-subscribed.
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  void InvalidateResolvedTileset();

private:
  /**
   * Resolves the Cesium Georeference to use with this Component. Returns
   * the value of the Georeference property if it is set. Otherwise, finds a
   * Georeference in the World and returns it, creating it if necessary. The
   * resolved Georeference is cached so subsequent calls to this function will
   * return the same instance.
   */
  ACesiumGeoreference* ResolveGeoreference();

  /**
   * Invalidates the cached resolved georeference, unsubscribing from it and
   * setting it to null. The next time ResolveGeoreference is called, the
   * Georeference will be re-resolved and re-subscribed.
   */
  void InvalidateResolvedGeoreference();

  /**
   * The latitude in degrees of this component, in the range [-90, 90]
   */
  UPROPERTY(
    EditAnywhere,
    Category = "Cesium|Georeference",
    Meta = (AllowPrivateAccess, ClampMin = -90.0, ClampMax = 90.0))
  double Latitude = 0.0;

  /**
   * The longitude in degrees of this component, in the range [-180, 180]
   */
  UPROPERTY(
    EditAnywhere,
    Category = "Cesium|Georeference",
    meta = (AllowPrivateAccess, ClampMin = -180.0, ClampMax = 180.0))
  double Longitude = 0.0;

  /**
   * The height in meters above the ellipsoid (usually WGS84) of this component.
   * Do not confuse this with a geoid height or height above mean sea level,
   * which can be tens of meters higher or lower depending on where in the world
   * the object is located.
   */
  UPROPERTY(
    EditAnywhere,
    Category = "Cesium|Georeference",
    Meta = (AllowPrivateAccess))
  double Height = 0.0;

  UPROPERTY(
    EditAnywhere,
    Category = "Cesium|Georeference",
    Meta = (AllowPrivateAccess))
  bool AdaptScale = false;

  UPROPERTY(
    EditAnywhere,
    Category = "Cesium|Georeference",
    Meta = (AllowPrivateAccess))
  bool AdaptOrientation = true;


  UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cesium|Georeference")
  void PrintDebug() const;

public:
  /**
   * Returns the longitude in degrees (X), latitude in degrees (Y),
   * and height in meters (Z) of the actor.
   *
   * Returns a zero vector if the component is not yet registered.
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  FVector GetLongitudeLatitudeHeight() const;

  /**
   * Move the actor to the specified longitude in degrees (x), latitude
   * in degrees (y), and height in meters (z).
   *
   * If `AdjustOrientationForGlobeWhenMoving` is enabled, the Actor's
   * orientation will also be adjusted to account for globe curvature.
   */
  void MoveToLongitudeLatitudeHeight(
      const glm::dvec3& TargetLongitudeLatitudeHeight);

  /**
   * Move the actor to the specified longitude in degrees (x), latitude
   * in degrees (y), and height in meters (z).
   *
   * If `AdjustOrientationForGlobeWhenMoving` is enabled, the Actor's
   * orientation will also be adjusted to account for globe curvature.
   */
  UFUNCTION(BlueprintCallable, Category = "Cesium")
  void
  MoveToLongitudeLatitudeHeight(const FVector& TargetLongitudeLatitudeHeight);

public:
  /**
   * Using the teleport flag will move objects to the updated transform
   * immediately and without affecting their velocity. This is useful when
   * working with physics actors that maintain an internal velocity which we do
   * not want to change when updating location.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cesium|Georeference")
  bool TeleportWhenUpdatingTransform = true;

  //
  // Base class overrides
  //

  /**
   * Called by the owner actor when the world's OriginLocation changes (i.e.
   * during origin rebasing). The Component will recompute the Actor's
   * transform based on the new OriginLocation and on this component's
   * globe transform. The Actor's orientation is unaffected.
   */
  virtual void
  ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;

  /**
   * Called when a component is created (not loaded). This can happen in the
   * editor or during gameplay.
   *
   * This method is invoked after this component is pasted and just prior to
   * registration. We mark the globe transform invalid here because we can't
   * assume the globe transform is still valid when the component is pasted into
   * another Actor, or even if the Actor was changed since the Component was
   * copied.
   */
  virtual void OnComponentCreated() override;

#if WITH_EDITOR
  virtual void
  PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
  /**
   * Called when a component is registered. This can be viewed as "enabling"
   * this Component on the Actor to which it is attached.
   *
   * In the Editor, this is called in a many different situations, such as on
   * changes to properties.
   */
  virtual void OnRegister() override;

  /**
   * Called when a component is unregistered. This can be viewed as
   * "disabling" this Component on the Actor to which it is attached.
   *
   * In the Editor, this is called in a many different situations, such as on
   * changes to properties.
   */
  virtual void OnUnregister() override;

private:
  /**
   * Called when the Component switches to a new Georeference Actor or the
   * existing Georeference is given a new origin Longitude, Latitude, or
   * Height. The Actor's position and orientation are recomputed from the
   * Component's globe (ECEF) position and orientation.
   */
  UFUNCTION()
  void _onGeoreferenceChanged();

  /**
   * Updates the Unreal world Actor position from the current globe position.
   */
  void _updateActorTransform();

  void _onGlobeTransformChanged(
      USceneComponent* InRootComponent,
      EUpdateTransformFlags UpdateTransformFlags,
      ETeleportType Teleport);
  void _unregisterTileset(ACesium3DTileset* pTileset);
  void _registerTileset(ACesium3DTileset* pTileset);
};
