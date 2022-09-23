// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#include "CesiumCustomGlobeAnchorComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumActors.h"
#include "CesiumCustomVersion.h"
#include "CesiumGeoreference.h"
#include "CesiumRuntime.h"
#include "CesiumTransforms.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VecMath.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/matrix_decompose.hpp>

// These are the "changes" that can happen to this component, how it detects
// them, and what it does about them:
//
// ## Actor Transform Changed
//
// * Detected by subscribing to the `TransformUpdated` event of the root
// component of the Actor to which this component is attached. The subscription
// is added in `OnRegister` and removed in `OnUnregister`.
// * Updates the ECEF transform from the new Actor transform.
// * If `AdjustOrientationForGlobeWhenMoving` is enabled, also applies a
// rotation based on the change in surface normal.
//
// ## Globe (ECEF) Position Changed
//
// * Happens when MoveToECEF (or similar) is called explicitly, or position
// properties are changed in the Editor.
// * Updates the Actor transform from the new ECEF transform.
// * If `AdjustOrientationForGlobeWhenMoving` is enabled, also applies a
// rotation based on the change in surface normal.
//
// ## Georeference Changed
//
// * Detected by subscribing to the `GeoreferenceUpdated` event. The
// subscription is added when a new Georeference is resolved in
// `ResolveGeoreference` (in `OnRegister` at the latest) and removed in
// `InvalidateResolvedGeoreference` (in `OnUnregister` and when the
// Georeference property is changed).
// * Updates the Actor transform from the existing ECEF transform.
// * Ignores `AdjustOrientationForGlobeWhenMoving` because the globe position is
// not changing.
//
// ## Origin Rebased
//
// * Detected by a call to `ApplyWorldOffset`.
// * Updates the Actor transform from the existing ECEF transform.
// * Ignores `AdjustOrientationForGlobeWhenMoving` because the globe position is
// not changing.

ACesium3DTileset* UCesiumCustomGlobeAnchorComponent::GetTileset() const {
  return this->Tileset;
}

void UCesiumCustomGlobeAnchorComponent::SetTileset(
    ACesium3DTileset* NewTileset) {
  this->_unregisterTileset();
  this->Tileset = NewTileset;
  this->InvalidateResolvedGeoreference();
  this->_registerTileset();
  this->ResolveGeoreference();
}

ACesiumGeoreference* UCesiumCustomGlobeAnchorComponent::GetGeoreference() const {
  return this->Georeference;
}

void UCesiumCustomGlobeAnchorComponent::SetGeoreference(
    ACesiumGeoreference* NewGeoreference) {
  this->Georeference = NewGeoreference;
  this->InvalidateResolvedGeoreference();
  this->ResolveGeoreference();
}

FVector UCesiumCustomGlobeAnchorComponent::GetECEF() const {
  if (!this->_worldToECEFIsValid) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "CesiumCustomGlobeAnchorComponent %s globe position is invalid because the component is not yet registered."),
        *this->GetName());
    return FVector(0.0);
  }

  return VecMath::createVector(glm::dvec3(this->_worldToECEF[3]));
}

void UCesiumCustomGlobeAnchorComponent::MoveToECEF(const glm::dvec3& newPosition) {
  this->ECEF_X = newPosition.x;
  this->ECEF_Y = newPosition.y;
  this->ECEF_Z = newPosition.z;
  this->_applyCartesianProperties();
}

void UCesiumCustomGlobeAnchorComponent::MoveToECEF(const FVector& TargetEcef) {
  this->MoveToECEF(VecMath::createVector3D(TargetEcef));
}

ACesiumGeoreference* UCesiumCustomGlobeAnchorComponent::ResolveGeoreference() {
  if (IsValid(this->ResolvedGeoreference)) {
    return this->ResolvedGeoreference;
  }

  if (IsValid(this->Tileset)) {
    this->ResolvedGeoreference = this->Tileset->GetGeoreference();
  } else if (IsValid(this->Georeference)) {
    this->ResolvedGeoreference = this->Georeference;
  } else {
    this->ResolvedGeoreference =
        ACesiumGeoreference::GetDefaultGeoreference(this);
  }

  if (this->ResolvedGeoreference) {
    this->ResolvedGeoreference->OnGeoreferenceUpdated.AddUniqueDynamic(
        this,
        &UCesiumCustomGlobeAnchorComponent::_onGeoreferenceChanged);
  }

  this->_onGeoreferenceChanged();

  return this->ResolvedGeoreference;
}

void UCesiumCustomGlobeAnchorComponent::_registerTileset() {
  if (IsValid(this->Tileset)) {
    USceneComponent* pTileSetRoot = this->Tileset->GetRootComponent();
    if (pTileSetRoot) {
      pTileSetRoot->TransformUpdated.AddUObject(
          this,
          &UCesiumCustomGlobeAnchorComponent::_onGlobeTransformChanged);
    }
  }
}

void UCesiumCustomGlobeAnchorComponent::_unregisterTileset() {
  if (IsValid(this->Tileset)) {
    USceneComponent* pTilesetRoot = this->Tileset->GetRootComponent();
    if (pTilesetRoot) {
      pTilesetRoot->TransformUpdated.RemoveAll(this);
    }
  }
}

void UCesiumCustomGlobeAnchorComponent::InvalidateResolvedGeoreference() {
  if (IsValid(this->ResolvedGeoreference)) {
    this->ResolvedGeoreference->OnGeoreferenceUpdated.RemoveAll(this);
  }
  this->ResolvedGeoreference = nullptr;
}

FVector UCesiumCustomGlobeAnchorComponent::GetLongitudeLatitudeHeight() const {
  if (!this->_worldToECEFIsValid || !this->ResolvedGeoreference) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "CesiumCustomGlobeAnchorComponent %s globe position is invalid because the component is not yet registered."),
        *this->GetName());
    return FVector(0.0);
  }

  return this->ResolvedGeoreference->TransformEcefToLongitudeLatitudeHeight(
      this->GetECEF());
}

void UCesiumCustomGlobeAnchorComponent::MoveToLongitudeLatitudeHeight(
    const glm::dvec3& TargetLongitudeLatitudeHeight) {
  if (!this->_worldToECEFIsValid || !this->ResolvedGeoreference) {
    UE_LOG(
        LogCesium,
        Error,
        TEXT(
            "CesiumCustomGlobeAnchorComponent %s cannot move to a globe position because the component is not yet registered."),
        *this->GetName());
    return;
  }

  this->MoveToECEF(
      this->ResolvedGeoreference->TransformLongitudeLatitudeHeightToEcef(
          TargetLongitudeLatitudeHeight));
}

void UCesiumCustomGlobeAnchorComponent::MoveToLongitudeLatitudeHeight(
    const FVector& TargetLongitudeLatitudeHeight) {
  return this->MoveToLongitudeLatitudeHeight(
      VecMath::createVector3D(TargetLongitudeLatitudeHeight));
}

void UCesiumCustomGlobeAnchorComponent::ApplyWorldOffset(
    const FVector& InOffset,
    bool bWorldShift) {
  // By the time this is called, all of the Actor's SceneComponents (including
  // its RootComponent) will already have had ApplyWorldOffset called on them.
  // So the root component's transform already reflects the shifted origin. It's
  // imprecise, though.
  //
  // Fortunately, this process does _not_ trigger the `TransformUpdated` event.
  // So our _worldToECEF transform still represents the precise globe transform
  // of the Actor.
  //
  // We simply need to convert the globe transform to a new Actor transform
  // based on the updated OriginLocation. The only slightly tricky part of this
  // is that the OriginLocation hasn't actually been updated yet.
  UActorComponent::ApplyWorldOffset(InOffset, bWorldShift);

  const UWorld* pWorld = this->GetWorld();
  if (!IsValid(pWorld)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("CesiumCustomGlobeAnchorComponent %s is not spawned in world"),
        *this->GetName());
    return;
  }

  // Compute the position that the world origin will have
  // after the rebase, indeed by SUBTRACTING the offset
  const glm::dvec3 oldWorldOriginLocation =
      VecMath::createVector3D(pWorld->OriginLocation);
  const glm::dvec3 offset = VecMath::createVector3D(InOffset);
  const glm::dvec3 newWorldOriginLocation = oldWorldOriginLocation - offset;

  // Update the Actor transform from the globe transform with the new origin
  // location explicitly provided.
  this->_updateActorTransformFromGlobeTransform(newWorldOriginLocation);
}

void UCesiumCustomGlobeAnchorComponent::Serialize(FArchive& Ar) {
  Super::Serialize(Ar);

  Ar.UsingCustomVersion(FCesiumCustomVersion::GUID);

  const int32 CesiumVersion = Ar.CustomVer(FCesiumCustomVersion::GUID);

  if (CesiumVersion < FCesiumCustomVersion::GeoreferenceRefactoring) {
    // In previous versions, there was no _worldToECEFIsValid flag. But we can
    // assume that the previously-stored ECEF transform was valid.
    this->_worldToECEFIsValid = true;
  }
}

void UCesiumCustomGlobeAnchorComponent::OnComponentCreated() {
  Super::OnComponentCreated();
  this->_worldToECEFIsValid = false;
}

#if WITH_EDITOR
void UCesiumCustomGlobeAnchorComponent::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);

  if (!PropertyChangedEvent.Property) {
    return;
  }

  FName propertyName = PropertyChangedEvent.Property->GetFName();

  if (propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Longitude) ||
      propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Latitude) ||
      propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Height)) {
    this->_applyCartographicProperties();
  } else if (
      propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, ECEF_X) ||
      propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, ECEF_Y) ||
      propertyName ==
          GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, ECEF_Z)) {
    this->_applyCartesianProperties();
  } else if (
      propertyName ==
      GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Georeference)) {
    this->InvalidateResolvedGeoreference();
  }
}
#endif

void UCesiumCustomGlobeAnchorComponent::OnRegister() {
  Super::OnRegister();

  const AActor* pAnchorOwner = this->GetOwner();
  if (!IsValid(pAnchorOwner)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("CesiumCustomGlobeAnchorComponent %s does not have a valid owner"),
        *this->GetName());
    return;
  }

  this->_registerTileset();

  // Resolve the georeference, which will also subscribe to the new georeference
  // (if there is one) and call _onGeoreferenceChanged.
  // This will update the actor transform with the globe position, but only if
  // the globe transform is valid.
  this->ResolveGeoreference();

  // If the globe transform is not yet valid, compute it from the actor
  // transform now.
  if (!this->_worldToECEFIsValid) {
    this->_updateGlobeTransformFromActorTransform();
  }
}

void UCesiumCustomGlobeAnchorComponent::OnUnregister() {
  Super::OnUnregister();

  // Unsubscribe from the ResolvedGeoreference.
  this->InvalidateResolvedGeoreference();

  this->_unregisterTileset();
}

void UCesiumCustomGlobeAnchorComponent::_onGlobeTransformChanged(
      USceneComponent* InRootComponent,
      EUpdateTransformFlags UpdateTransformFlags,
      ETeleportType Teleport) {

  UE_LOG(
    LogCesium,
    Display,
    TEXT("CesiumCustomGlobeAnchorComponent %s globe is moved."),
    *this->GetName());

  if (this->_worldToECEFIsValid) {
    this->_updateActorTransformFromGlobeTransform();
  }
}

void UCesiumCustomGlobeAnchorComponent::_onGeoreferenceChanged() {
  if (this->_worldToECEFIsValid) {
    this->_updateActorTransformFromGlobeTransform();
  }
}

const glm::dmat4&
UCesiumCustomGlobeAnchorComponent::_updateGlobeTransformFromActorTransform() {
  if (!this->ResolvedGeoreference) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "CesiumCustomGlobeAnchorComponent %s cannot update globe transform from actor transform because there is no valid Georeference."),
        *this->GetName());
    this->_worldToECEFIsValid = false;
    return this->_worldToECEF;
  }

  const AActor* pRootActor = this->GetOwner();
  if (!IsValid(pRootActor)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("UCesiumCustomGlobeAnchorComponent %s does not have a valid root actor"),
        *this->GetName());
    this->_worldToECEFIsValid = false;
    return this->_worldToECEF;
  }

  glm::dmat4 worldTransform(1.0);
  worldTransform[3] += CesiumActors::getWorldOrigin4D(pRootActor);
  worldTransform[3].w = 1.0;

  // Convert to ECEF
  const glm::dmat4& absoluteUnrealToEcef =
    this->ResolvedGeoreference->GetGeoTransforms()
          .GetAbsoluteUnrealWorldToEllipsoidCenteredTransform();

  this->_worldToECEF = absoluteUnrealToEcef * worldTransform;
  this->_worldToECEFIsValid = true;

  this->_updateCartesianProperties();
  // this->_updateCartographicProperties();

#if WITH_EDITOR
  // In the Editor, mark this component modified so Undo works properly.
  this->Modify();
#endif

  return this->_worldToECEF;
}

FTransform UCesiumCustomGlobeAnchorComponent::_updateActorTransformFromGlobeTransform(
    const std::optional<glm::dvec3>& newWorldOrigin) {
  const AActor* pAnchorOwner = this->GetOwner();
  if (!IsValid(pAnchorOwner)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("UCesiumCustomGlobeAnchorComponent %s does not have a valid owner"),
        *this->GetName());
    return FTransform();
  }

  USceneComponent* pAnchorOwnerRoot = pAnchorOwner->GetRootComponent();
  if (!IsValid(pAnchorOwnerRoot)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "The owner of UCesiumCustomGlobeAnchorComponent %s does not have a valid root component"),
        *this->GetName());
    return FTransform();
  }

  if (!this->_worldToECEFIsValid) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "UCesiumCustomGlobeAnchorComponent %s cannot update Actor transform from Globe transform because the Globe transform is not known."),
        *this->GetName());
    return pAnchorOwnerRoot->GetComponentTransform();
  }

  const GeoTransforms& geoTransforms =
      this->ResolveGeoreference()->GetGeoTransforms();

  glm::dmat4 tilesetTransform(1.0);
  if (IsValid(this->Tileset)) {
    tilesetTransform = VecMath::createMatrix4D(this->Tileset->GetTransform().ToMatrixWithScale());
  }

  // Transform ECEF to UE absolute world
  const glm::dmat4& ecefToAbsoluteUnreal =
      geoTransforms.GetEllipsoidCenteredToAbsoluteUnrealWorldTransform();
  glm::dmat4 actorToUnreal = tilesetTransform * ecefToAbsoluteUnreal * this->_worldToECEF;


  // Transform UE absolute world to UE relative world
  actorToUnreal[3] -= newWorldOrigin ? glm::dvec4(*newWorldOrigin, 1.0)
                                     : CesiumActors::getWorldOrigin4D(pAnchorOwner);
  actorToUnreal[3].w = 1.0;

  FTransform actorTransform = FTransform(VecMath::createMatrix(actorToUnreal));

#if WITH_EDITOR
  // In the Editor, mark the root component modified so Undo works properly.
  pAnchorOwnerRoot->Modify();
#endif

  // Set the Actor transform
  pAnchorOwnerRoot->SetWorldTransform(
      actorTransform,
      false,
      nullptr,
      this->TeleportWhenUpdatingTransform ? ETeleportType::TeleportPhysics
                                          : ETeleportType::None);
  return actorTransform;
}

const glm::dmat4& UCesiumCustomGlobeAnchorComponent::_setGlobeTransform(
    const glm::dmat4& newTransform) {
#if WITH_EDITOR
  // In the Editor, mark this component modified so Undo works properly.
  this->Modify();
#endif


  this->_worldToECEF = newTransform;
  this->_updateActorTransformFromGlobeTransform();
  return this->_worldToECEF;
}

void UCesiumCustomGlobeAnchorComponent::_applyCartesianProperties() {
  // If we don't yet know our globe transform, compute it from the Actor
  // transform now. But restore the ECEF position properties afterward.
  if (!this->_worldToECEFIsValid) {
    double x = this->ECEF_X;
    double y = this->ECEF_Y;
    double z = this->ECEF_Z;
    this->_updateGlobeTransformFromActorTransform();
    this->ECEF_X = x;
    this->ECEF_Y = y;
    this->ECEF_Z = z;
  }

  glm::dmat4 transform = this->_worldToECEF;
  transform[3] = glm::dvec4(this->ECEF_X, this->ECEF_Y, this->ECEF_Z, 1.0);
  this->_setGlobeTransform(transform);

  this->_updateCartographicProperties();
}

void UCesiumCustomGlobeAnchorComponent::_updateCartesianProperties() {
  if (!this->_worldToECEFIsValid) {
    return;
  }

  this->ECEF_X = this->_worldToECEF[3].x;
  this->ECEF_Y = this->_worldToECEF[3].y;
  this->ECEF_Z = this->_worldToECEF[3].z;
}

void UCesiumCustomGlobeAnchorComponent::_applyCartographicProperties() {
  // If we don't yet know our globe transform, compute it from the Actor
  // transform now. But restore the LLH position properties afterward.
  if (!this->_worldToECEFIsValid) {
    double longitude = this->Longitude;
    double latitude = this->Latitude;
    double height = this->Height;
    this->_updateGlobeTransformFromActorTransform();
    this->Longitude = longitude;
    this->Latitude = latitude;
    this->Height = height;
  }

  ACesiumGeoreference* pGeoreference = this->ResolveGeoreference();
  if (!pGeoreference) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "The UCesiumCustomGlobeAnchorComponent %s does not have a valid Georeference"),
        *this->GetName());
  }

  glm::dmat4 transform = this->_worldToECEF;
  glm::dvec3 newEcef =
      pGeoreference->GetGeoTransforms().TransformLongitudeLatitudeHeightToEcef(
          glm::dvec3(this->Longitude, this->Latitude, this->Height));
  transform[3] = glm::dvec4(newEcef, 1.0);
  this->_setGlobeTransform(transform);

  this->_updateCartesianProperties();
}

void UCesiumCustomGlobeAnchorComponent::_updateCartographicProperties() {
  if (!this->_worldToECEFIsValid) {
    return;
  }

  ACesiumGeoreference* pGeoreference = this->ResolveGeoreference();
  if (!pGeoreference) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
            "The UCesiumCustomGlobeAnchorComponent %s does not have a valid Georeference"),
        *this->GetName());
  }

  glm::dvec3 llh =
      pGeoreference->GetGeoTransforms().TransformEcefToLongitudeLatitudeHeight(
          glm::dvec3(this->_worldToECEF[3]));

  this->Longitude = llh.x;
  this->Latitude = llh.y;
  this->Height = llh.z;
}
