// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#include "CalcBoundsWithMatrix.h"
#include "VecMath.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/vec3.hpp>

glm::dmat4 CalcBoundsOperationWithMatrix::getModelToUnrealWorldMatrix() const {
  return localToWorld;
}

glm::dmat4 CalcBoundsOperationWithMatrix::getTilesetToUnrealWorldMatrix() const {
  const glm::dmat4 modelToUnreal = getModelToUnrealWorldMatrix();
  const glm::dmat4 tilesetToModel = glm::affineInverse(highPrecisionTransform);
  return modelToUnreal * tilesetToModel;
}

FBoxSphereBounds CalcBoundsOperationWithMatrix::operator()(
    const CesiumGeometry::BoundingSphere& sphere) const {
  glm::dmat4 matrix = getTilesetToUnrealWorldMatrix();
  glm::dvec3 center = glm::dvec3(matrix * glm::dvec4(sphere.getCenter(), 1.0));
  glm::dmat3 halfAxes = glm::dmat3(matrix) * glm::dmat3(sphere.getRadius());

  // The sphere only needs to reach the sides of the box, not the corners.
  double sphereRadius =
      glm::max(glm::length(halfAxes[0]), glm::length(halfAxes[1]));
  sphereRadius = glm::max(sphereRadius, glm::length(halfAxes[2]));

  FBoxSphereBounds result;
  result.Origin = VecMath::createVector(center);
  result.SphereRadius = sphereRadius;
  result.BoxExtent = FVector(sphereRadius, sphereRadius, sphereRadius);
  return result;
}

FBoxSphereBounds CalcBoundsOperationWithMatrix::operator()(
    const CesiumGeometry::OrientedBoundingBox& box) const {
  glm::dmat4 matrix = getTilesetToUnrealWorldMatrix();
  glm::dvec3 center = glm::dvec3(matrix * glm::dvec4(box.getCenter(), 1.0));
  glm::dmat3 halfAxes = glm::dmat3(matrix) * box.getHalfAxes();

  glm::dvec3 corner1 = halfAxes[0] + halfAxes[1];
  glm::dvec3 corner2 = halfAxes[0] + halfAxes[2];
  glm::dvec3 corner3 = halfAxes[1] + halfAxes[2];

  double sphereRadius = glm::max(glm::length(corner1), glm::length(corner2));
  sphereRadius = glm::max(sphereRadius, glm::length(corner3));

  double maxX = glm::abs(halfAxes[0].x) + glm::abs(halfAxes[1].x) +
                glm::abs(halfAxes[2].x);
  double maxY = glm::abs(halfAxes[0].y) + glm::abs(halfAxes[1].y) +
                glm::abs(halfAxes[2].y);
  double maxZ = glm::abs(halfAxes[0].z) + glm::abs(halfAxes[1].z) +
                glm::abs(halfAxes[2].z);

  FBoxSphereBounds result;
  result.Origin = VecMath::createVector(center);
  result.SphereRadius = sphereRadius;
  result.BoxExtent = FVector(maxX, maxY, maxZ);
  return result;
}

FBoxSphereBounds CalcBoundsOperationWithMatrix::operator()(
    const CesiumGeospatial::BoundingRegion& region) const {
  return (*this)(region.getBoundingBox());
}

FBoxSphereBounds CalcBoundsOperationWithMatrix::operator()(
    const CesiumGeospatial::BoundingRegionWithLooseFittingHeights& region)
    const {
  return (*this)(region.getBoundingRegion());
}

FBoxSphereBounds CalcBoundsOperationWithMatrix::operator()(
    const CesiumGeospatial::S2CellBoundingVolume& s2) const {
  return (*this)(s2.computeBoundingRegion());
}
