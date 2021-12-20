// Copyright 2020-2021 CesiumGS, Inc. and Contributors

#include "CesiumMetadataPrimitive.h"
#include "CesiumGltf/ExtensionMeshPrimitiveExtFeatureMetadata.h"
#include "CesiumGltf/ExtensionModelExtFeatureMetadata.h"
#include "CesiumGltf/Model.h"

FCesiumMetadataPrimitive::FCesiumMetadataPrimitive(
    const CesiumGltf::Model& model,
    const CesiumGltf::MeshPrimitive& primitive,
    const CesiumGltf::ExtensionModelExtFeatureMetadata& metadata,
    const CesiumGltf::ExtensionMeshPrimitiveExtFeatureMetadata&
        primitiveMetadata) {

  const CesiumGltf::Accessor& indicesAccessor =
      model.getSafe(model.accessors, primitive.indices);
  switch (indicesAccessor.componentType) {
  case CesiumGltf::Accessor::ComponentType::UNSIGNED_BYTE:
    _vertexIDAccessor =
        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint8_t>>(
            model,
            indicesAccessor);
    break;
  case CesiumGltf::Accessor::ComponentType::UNSIGNED_SHORT:
    _vertexIDAccessor =
        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint16_t>>(
            model,
            indicesAccessor);
    break;
  case CesiumGltf::Accessor::ComponentType::UNSIGNED_INT:
    _vertexIDAccessor =
        CesiumGltf::AccessorView<CesiumGltf::AccessorTypes::SCALAR<uint32_t>>(
            model,
            indicesAccessor);
    break;
  default:
    break;
  }

  for (const CesiumGltf::FeatureIDAttribute& attribute :
       primitiveMetadata.featureIdAttributes) {
    if (attribute.featureIds.attribute) {
      auto featureID =
          primitive.attributes.find(attribute.featureIds.attribute.value());
      if (featureID == primitive.attributes.end()) {
        continue;
      }

      const CesiumGltf::Accessor* accessor =
          model.getSafe<CesiumGltf::Accessor>(
              &model.accessors,
              featureID->second);
      if (!accessor) {
        continue;
      }

      if (accessor->type != CesiumGltf::Accessor::Type::SCALAR) {
        continue;
      }

      auto featureTable = metadata.featureTables.find(attribute.featureTable);
      if (featureTable == metadata.featureTables.end()) {
        continue;
      }

      this->_vertexFeatures.Add(
          FCesiumVertexMetadata(model, *accessor, featureTable->second));
    }
  }

  for (const CesiumGltf::FeatureIDTexture& featureIDTexture :
       primitiveMetadata.featureIdTextures) {
    this->_featureIDTextures.Add(
        FCesiumFeatureIDTexture(model, featureIDTexture));
  }

  for (const std::string& featureTextureId :
       primitiveMetadata.featureTextures) {
    const auto& featureTextureIt =
        metadata.featureTextures.find(featureTextureId);
    this->_featureTextures.Reserve(metadata.featureTextures.size());
    if (featureTextureIt != metadata.featureTextures.end()) {
      this->_featureTextures.Emplace(model, featureTextureIt->second);
    }
  }
}

const TArray<FCesiumVertexMetadata>&
UCesiumMetadataPrimitiveBlueprintLibrary::GetVertexFeatures(
    UPARAM(ref) const FCesiumMetadataPrimitive& MetadataPrimitive) {
  return MetadataPrimitive._vertexFeatures;
}

const TArray<FCesiumFeatureIDTexture>&
UCesiumMetadataPrimitiveBlueprintLibrary::GetFeatureIDTextures(
    UPARAM(ref) const FCesiumMetadataPrimitive& MetadataPrimitive) {
  return MetadataPrimitive._featureIDTextures;
}

const TArray<FCesiumFeatureTexture>&
UCesiumMetadataPrimitiveBlueprintLibrary::GetFeatureTextures(
    UPARAM(ref) const FCesiumMetadataPrimitive& MetadataPrimitive) {
  return MetadataPrimitive._featureTextures;
}

int64 UCesiumMetadataPrimitiveBlueprintLibrary::GetFirstVertexIDFromFaceID(
    const FCesiumMetadataPrimitive& MetadataPrimitive,
    int64 faceID) {
  return std::visit(
      [faceID](const auto& accessor) {
        int64 index = faceID * 3;

        if (accessor.status() != CesiumGltf::AccessorViewStatus::Valid) {
          // No indices, so each successive face is just the next three
          // vertices.
          return index;
        } else if (index >= accessor.size()) {
          // Invalid face index.
          return -1LL;
        }

        return int64(accessor[index].value[0]);
      },
      MetadataPrimitive._vertexIDAccessor);
}
