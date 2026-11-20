/*
* Copyright (c) Galib Arrieta (aka lumbermixalot@github, aka galibzon@github).
*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/
#pragma once

#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/Memory/SystemAllocator.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/Math/Color.h>

#include <Atom/RPI.Reflect/Image/Image.h>

#include <Renderer/CloudMaterialProperties.h>
#include <Renderer/CloudLayers.h>

namespace VolumetricClouds
{
    // Consolidates all the data for the shader constants needed
    // by the cloudscape shader.
    // See declaration of CloudscapeComponentConfig for details on each parameter.
    struct CloudscapeShaderConstantData
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO_WITH_NAME_DECL(CloudscapeShaderConstantData);
        AZ_RTTI_NO_TYPE_INFO_DECL();

        virtual ~CloudscapeShaderConstantData() = default;

        static void Reflect(AZ::ReflectContext* reflection);

        // Only compares the reflected parameters.
        bool operator==(const CloudscapeShaderConstantData& rhs) const;
        bool operator!=(const CloudscapeShaderConstantData& rhs) const;

        // Used to scale world position XYZ when sampling
        // the Noise Textures during ray marching.
        float m_uvwScale = 0.25;
        // Maximum number of mip levels to consider when sampling
        // the low frequency texture. It will be clamped at runtime to the number
        // of mips available in the low frequency texture.
        // Also the clamping will make sure this value is never less than 1.
        uint32_t m_maxMipLevels = 10;
        // Clamped version of @m_maxMipLevels. Calculated at runtime.
        // A value between 1 and mip counts in low freq noise texture.
        uint32_t m_clampedMipLevels = 10; // DO NOT REFLECT

        // Ray Marching Steps, for performance/quality tradeoff.
        // These are the steps uses to ray march the clouds slab and calculate
        // the transmittance for each pixel.
        uint8_t m_minRayMarchingSteps = 32;
        uint8_t m_maxRayMarchingSteps = 64;

        float m_planetRadiusKm = 6371.0f; // TODO: Get this value from Sky Atmosphere Component.
        // Distance, above sea level, where the cloud slab begins.
        float m_cloudSlabDistanceAboveSeaLevelKm = 1.5f;
        // Clouds will be present only within the thickness
        // of the cloud slab.
        float m_cloudSlabThicknessKm = 3.5f;

        AZ::Vector3 m_sunColor = { 1.0f, 1.0f, 1.0f }; // DO NOT REFLECT (comes from an entity)
        float m_sunLightIntensity = 1.0f;
        AZ::Color m_ambientLightColor = { 0.55f, 0.7f, 1.0f, 1.0f };
        float m_ambientLightIntensity = 1.0f;
        // The shader expects a normalized vector.
        AZ::Vector3 m_directionTowardsTheSun = {0.0f, 0.0f, 1.0f}; // DO NOT REFLECT (comes from an entity)

        CloudMaterialProperties m_cloudMaterialProperties;

        //////////////////////////////////////////////////////////////
        // ******************* Weather Data Start
        // The weather map is a texture representing a squared area.
        // The length of each side of the square is measured in kilometers.
        float m_weatherMapSizeKm = 60.0;
        float m_globalCloudCoverage = 0.75; // Не используют: покрытие задают слои.
        float m_globalCloudDensity = 1.0; // Не используют: плотность задают слои.
        float m_windSpeedKmPerSec = 0.0;
        AZ::Vector3 m_windDirection = { 1.0f, 1.0f, 0.0f };
        // Pushes the top of the clouds along the wind direction by this
        // distance. Useful for dramatic/artistic effects.
        float m_cloudTopOffsetKm = 0.0;
        // Сужает переход плотности на краю облака: при 1 край полупрозрачный
        // шириной в сотни метров, при 4 край кучевого облака чёткий.
        float m_cloudEdgeSharpness = 4.0;
        // Пасмурный режим: балл в октах, с которого просветы затягивает
        // дымкой, и степень кривой нарастания. 1 - линейная рампа, выше 1 -
        // в начале медленно, в конце быстро, ниже 1 - наоборот.
        float m_overcastStartOktas = 7.5;
        float m_overcastCurve = 1.0;

        // Слои облаков по родам. У каждого слоя своё покрытие и свои высоты.
        // Слой облаков (m_cloudSlab*) охватывает все слои с ненулевым баллом.
        StratusLayer m_stratus;
        StratocumulusLayer m_stratocumulus;
        CumulusLayer m_cumulus;

        // Значения слоев для шейдера. Слоистые облака держатся ниже
        // слоисто-кучевых: при пересечении слоистые опускаются.
        void GetLayerParams(CloudLayerParams& stratus, CloudLayerParams& stratocumulus, CloudLayerParams& cumulus) const;
        // Нижняя граница и толщина слоя облаков, км. Без активных слоев
        // возвращает m_cloudSlabDistanceAboveSeaLevelKm и m_cloudSlabThicknessKm.
        void GetEffectiveSlab(float& baseKm, float& thicknessKm) const;
        // Необязательная RGBA-карта погоды. R: покрытие (0 - ясно, 0.5 - балл
        // слоя, 1 - вдвое больше), B: доля толщины слоя, A: плотность
        // (0.5 - плотность слоя).
        AZ::Data::Instance<AZ::RPI::Image> m_weatherMap; // DO NOT REFLECT
        // ******************* Weather Data End
        //////////////////////////////////////////////////////////////


        // May come from a realtime texture generator (AttachmentImage)
        // or from a file (StreamingImage)
        AZ::Data::Instance<AZ::RPI::Image> m_lowFrequencyNoiseTexture; // DO NOT REFLECT (comes from an entity)

        // May come from a realtime texture generator (AttachmentImage)
        // or from a file (StreamingImage)

        AZ::Data::Instance<AZ::RPI::Image> m_highFrequencyNoiseTexture; // DO NOT REFLECT (comes from an entity)
    };

} // namespace VolumetricClouds
