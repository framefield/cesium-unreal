// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#include "CesiumCustomGlobeAnchorComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumActors.h"
#include "CesiumCustomVersion.h"
#include "CesiumGeoreference.h"
#include "CesiumRuntime.h"
#include "CesiumTransforms.h"
#include "EngineUtils.h"
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
  this->Tileset = NewTileset;
  this->InvalidateResolvedTileset();
  this->ResolveTileset();
}

FName UCesiumCustomGlobeAnchorComponent::GetTilesetTag() const {
  return this->TilesetTag;
}

void UCesiumCustomGlobeAnchorComponent::SetTilesetTag(FName NewTilesetTag) {
  if (NewTilesetTag != this->TilesetTag) {
    return;
  }
  this->TilesetTag = NewTilesetTag;

  if (!IsValid(this->Tileset)) {
    this->InvalidateResolvedTileset();
    this->ResolveTileset();
  }
}

ACesiumGeoreference* UCesiumCustomGlobeAnchorComponent::ResolveGeoreference() {
  if (IsValid(this->ResolvedGeoreference)) {
    return this->ResolvedGeoreference;
  }

  if (IsValid(this->ResolvedTileset)) {
    this->ResolvedGeoreference = this->ResolvedTileset->GetGeoreference();
  } else {
    this->ResolvedGeoreference = nullptr;
  }

  if (this->ResolvedGeoreference) {
    this->ResolvedGeoreference->OnGeoreferenceUpdated.AddUniqueDynamic(
        this,
        &UCesiumCustomGlobeAnchorComponent::_onGeoreferenceChanged);

    const glm::dmat4& absoluteUnrealToEcef =
        this->ResolvedGeoreference->GetGeoTransforms()
            .GetAbsoluteUnrealWorldToEllipsoidCenteredTransform();
  }

  this->_onGeoreferenceChanged();

  return this->ResolvedGeoreference;
}

void UCesiumCustomGlobeAnchorComponent::InvalidateResolvedGeoreference() {
  if (IsValid(this->ResolvedGeoreference)) {
    this->ResolvedGeoreference->OnGeoreferenceUpdated.RemoveAll(this);
  }
  this->ResolvedGeoreference = nullptr;
}


ACesium3DTileset* UCesiumCustomGlobeAnchorComponent::ResolveTileset() {
  if (IsValid(this->ResolvedTileset)) {
    return this->ResolvedTileset;
  }

  if (IsValid(this->Tileset)) {
    this->ResolvedTileset = this->Tileset;
  } else {
    EActorIteratorFlags flags = EActorIteratorFlags::OnlyActiveLevels |
                                EActorIteratorFlags::SkipPendingKill;
    for (TActorIterator<AActor> actorIterator(
             this->GetWorld(),
             ACesium3DTileset::StaticClass(),
             flags);
         actorIterator;
         ++actorIterator) {
      AActor* actor = *actorIterator;
      if (actor->ActorHasTag(this->TilesetTag)) {
        this->ResolvedTileset = Cast<ACesium3DTileset>(actor);

        if (IsValid(this->ResolvedTileset)) {
          this->ResolvedGeoreference = nullptr;

          UE_LOG(
              LogCesium,
              Display,
              TEXT(
                "CesiumCustomGlobeAnchorComponent %s found tileset %s using tag %s."
              ),
              *this->GetOwner()->GetName(),
              *this->ResolvedTileset->GetName(),
              *this->TilesetTag.ToString());
          break;
        } else {
          UE_LOG(
              LogCesium,
              Warning,
              TEXT(
                "CesiumCustomGlobeAnchorComponent %s found invalid tileset %s using tag %s."
              ),
              *this->GetOwner()->GetName(),
              *actor->GetName(),
              *this->TilesetTag.ToString());
        }
      }
    }
  }

  if (!IsValid(this->ResolvedTileset)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("CesiumCustomGlobeAnchorComponent %s has no tileset."),
        *this->GetOwner()->GetName());
  }

  _registerTileset(this->ResolvedTileset);
  this->ResolveGeoreference();

  return this->ResolvedTileset;
}

void UCesiumCustomGlobeAnchorComponent::InvalidateResolvedTileset() {
  if (IsValid(this->ResolvedTileset)) {
    _unregisterTileset(this->ResolvedTileset);
  }
  this->ResolvedTileset = nullptr;
  this->InvalidateResolvedGeoreference();
}

void UCesiumCustomGlobeAnchorComponent::_registerTileset(
    ACesium3DTileset* pTileset) {
  if (IsValid(pTileset)) {
    USceneComponent* pTileSetRoot = pTileset->GetRootComponent();
    if (pTileSetRoot) {
      pTileSetRoot->TransformUpdated.AddUObject(
          this,
          &UCesiumCustomGlobeAnchorComponent::_onGlobeTransformChanged);
    }
  }
}

void UCesiumCustomGlobeAnchorComponent::_unregisterTileset(
    ACesium3DTileset* pTileset) {
  if (IsValid(pTileset)) {
    USceneComponent* pTilesetRoot = pTileset->GetRootComponent();
    if (pTilesetRoot) {
      pTilesetRoot->TransformUpdated.RemoveAll(this);
    }
  }
}


void UCesiumCustomGlobeAnchorComponent::PrintDebug() const {
  UE_LOG(
      LogCesium,
      Warning,
      TEXT("UCesiumCustomGlobeAnchorComponent %s debug"),
      *this->GetOwner()->GetName());

  if (IsValid(this->ResolvedTileset) && IsValid(this->ResolvedGeoreference)) {
    const auto& GeoPos = this->ResolvedGeoreference->
                               GetGeoreferenceOriginLongitudeLatitudeHeight();
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("ResolvedTileset: %s ResolvedGeoReference: %s %s"),
        *this->ResolvedTileset->GetActorNameOrLabel(),
        *this->ResolvedGeoreference->GetActorNameOrLabel(),
        *GeoPos.ToString());

    const auto& AbsoluteUnrealToEcef = VecMath::createMatrix(
        this->ResolvedGeoreference->GetGeoTransforms()
            .GetAbsoluteUnrealWorldToEllipsoidCenteredTransform());

    UE_LOG(
        LogCesium,
        Warning,
        TEXT("AbsoluteUnrealToEcef %s"),
        *AbsoluteUnrealToEcef.ToString());

  } else {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("ResolvedTileset: %s ResolvedGeoReference: %s"),
        IsValid(this->ResolvedTileset) ? *this->ResolvedTileset->GetName() :
        TEXT("Invalid"),
        IsValid(this->ResolvedGeoreference) ? *this->ResolvedGeoreference->
        GetName() : TEXT("Invalid"));
  }
}

FVector UCesiumCustomGlobeAnchorComponent::GetLongitudeLatitudeHeight() const {
  return FVector(this->Longitude, this->Latitude, this->Height);
}

void UCesiumCustomGlobeAnchorComponent::MoveToLongitudeLatitudeHeight(
    const glm::dvec3& TargetLongitudeLatitudeHeight) {
  this->Longitude = TargetLongitudeLatitudeHeight.x;
  this->Latitude = TargetLongitudeLatitudeHeight.y;
  this->Height = TargetLongitudeLatitudeHeight.z;

  this->_updateActorTransform();
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
        *this->GetOwner()->GetName());
    return;
  }

  // Update the Actor transform from the globe transform with the new origin
  // location explicitly provided.
  this->_updateActorTransform();
}

void UCesiumCustomGlobeAnchorComponent::OnComponentCreated() {
  Super::OnComponentCreated();
}

#if WITH_EDITOR
void UCesiumCustomGlobeAnchorComponent::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent) {
  Super::PostEditChangeProperty(PropertyChangedEvent);

  if (!PropertyChangedEvent.Property) {
    return;
  }

  const FName PropertyName = PropertyChangedEvent.Property->GetFName();

  if (PropertyName ==
      GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Longitude) ||
      PropertyName ==
      GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Latitude) ||
      PropertyName ==
      GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Height)) {
    this->_updateActorTransform();
  } else if (
    PropertyName ==
    GET_MEMBER_NAME_CHECKED(UCesiumCustomGlobeAnchorComponent, Tileset)) {
    this->InvalidateResolvedTileset();
    this->ResolveTileset();
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
        *this->GetOwner()->GetName());
    return;
  }

  // Resolve the tileset, which will also subscribe to the new georeference
  // (if there is one) and call _onGeoreferenceChanged.
  // This will update the actor transform with the globe position, but only if
  // the globe transform is valid.
  this->ResolveTileset();

  this->_updateActorTransform();
}

void UCesiumCustomGlobeAnchorComponent::OnUnregister() {
  Super::OnUnregister();

  // Unsubscribe from the ResolvedGeoreference.
  this->InvalidateResolvedTileset();
}

void UCesiumCustomGlobeAnchorComponent::_onGlobeTransformChanged(
    USceneComponent* InRootComponent,
    EUpdateTransformFlags UpdateTransformFlags,
    ETeleportType Teleport) {
  this->_updateActorTransform();
}

void UCesiumCustomGlobeAnchorComponent::_onGeoreferenceChanged() {
  this->_updateActorTransform();
}

FTransform UCesiumCustomGlobeAnchorComponent::_updateActorTransform() {
  const AActor* AnchorOwner = this->GetOwner();
  if (!IsValid(AnchorOwner)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT("UCesiumCustomGlobeAnchorComponent %s does not have a valid owner"
        ),
        *this->GetOwner()->GetName());
    return FTransform();
  }

  USceneComponent* AnchorOwnerRoot = AnchorOwner->GetRootComponent();
  if (!IsValid(AnchorOwnerRoot)) {
    UE_LOG(
        LogCesium,
        Warning,
        TEXT(
          "The owner of UCesiumCustomGlobeAnchorComponent %s does not have a valid root component"
        ),
        *this->GetOwner()->GetName());
    return FTransform();
  }

  this->ResolveTileset();

  const GeoTransforms& GeoTransforms =
      this->ResolveGeoreference()->GetGeoTransforms();

  glm::dmat4 TilesetTransform(1.0);
  if (IsValid(this->ResolvedTileset)) {
    TilesetTransform = VecMath::createMatrix4D(
        this->ResolvedTileset->GetTransform().ToMatrixWithScale());
  }

  // apply lon/lat transform
  auto AbsoluteUnrealToECEF = this->ResolvedGeoreference->GetGeoTransforms()
                                  .GetAbsoluteUnrealWorldToEllipsoidCenteredTransform();
  const auto& ECEF = this->ResolvedGeoreference->
                           TransformLongitudeLatitudeHeightToEcef(
                               glm::dvec3(
                                   this->Longitude,
                                   this->Latitude,
                                   this->Height));
  AbsoluteUnrealToECEF[3] = glm::dvec4(ECEF.x, ECEF.y, ECEF.z, 1.0);

  // Transform ECEF to UE absolute world
  const glm::dmat4& ECEFToAbsoluteUnreal =
      GeoTransforms.GetEllipsoidCenteredToAbsoluteUnrealWorldTransform();
  glm::dmat4 ActorToUnreal = ECEFToAbsoluteUnreal * AbsoluteUnrealToECEF;
  ActorToUnreal = TilesetTransform * ActorToUnreal;


  const auto& Center =  ActorToUnreal * glm::dvec4(0,0, 0, 1);
  const auto& UpECEF = this->ResolvedGeoreference->
                           TransformLongitudeLatitudeHeightToEcef(
                               glm::dvec3(
                                   this->Longitude,
                                   this->Latitude,
                                   this->Height - 100));
  AbsoluteUnrealToECEF[3] = glm::dvec4(UpECEF.x, UpECEF.y, UpECEF.z, 1.0);
  glm::dmat4 UpToUnreal = ECEFToAbsoluteUnreal * AbsoluteUnrealToECEF;
  UpToUnreal = TilesetTransform * UpToUnreal;

  const auto& ForwardECEF = this->ResolvedGeoreference->
                         TransformLongitudeLatitudeHeightToEcef(
                             glm::dvec3(
                                 this->Longitude,
                                 fabs(this->Latitude - 0.001),
                                 this->Height));
  AbsoluteUnrealToECEF[3] = glm::dvec4(ForwardECEF.x, ForwardECEF.y, ForwardECEF.z, 1.0);
  glm::dmat4 ForwardToUnreal = ECEFToAbsoluteUnreal * AbsoluteUnrealToECEF;
  ForwardToUnreal = TilesetTransform * ForwardToUnreal;


  const auto& UpCenter =  UpToUnreal * glm::dvec4(0,0, 0, 1);
  const auto& ForwardCenter =  ForwardToUnreal * glm::dvec4(0,0, 0, 1);
  const auto& Up = Center - UpCenter;
  const auto& Forward = Center - ForwardCenter;

  const auto& Rotation = FRotationMatrix::MakeFromZY(  FVector3d(Up.x, Up.y, Up.z), FVector3d(Forward.x, Forward.y, Forward.z)).ToQuat();

  FTransform ActorTransform = FTransform(VecMath::createMatrix(ActorToUnreal));
  ActorTransform.SetRotation(Rotation);

#if WITH_EDITOR
  // In the Editor, mark the root component modified so Undo works properly.
  AnchorOwnerRoot->Modify();
#endif

  // Set the Actor transform
  AnchorOwnerRoot->SetWorldTransform(
      ActorTransform,
      false,
      nullptr,
      this->TeleportWhenUpdatingTransform
        ? ETeleportType::TeleportPhysics
        : ETeleportType::None);
  return ActorTransform;
}
