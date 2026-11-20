/*
* Copyright (c) Galib Arrieta (aka lumbermixalot@github, aka galibzon@github).
*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/

#include <AzCore/Asset/AssetSerializer.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/algorithm.h>
#include <AzCore/std/limits.h>
#include <initializer_list>

#include <VolumetricClouds/VolumetricCloudsTypeIds.h>
#include "CloudscapeShaderConstantData.h"

namespace VolumetricClouds
{
    AZ_CLASS_ALLOCATOR_IMPL(CloudscapeShaderConstantData, AZ::SystemAllocator);
    AZ_TYPE_INFO_WITH_NAME_IMPL(CloudscapeShaderConstantData, "VolumetricClouds::CloudscapeShaderConstantData", CloudscapeShaderConstantDataTypeId);
    AZ_RTTI_NO_TYPE_INFO_IMPL(CloudscapeShaderConstantData);

    void CloudscapeShaderConstantData::GetLayerParams(CloudLayerParams& stratus, CloudLayerParams& stratocumulus, CloudLayerParams& cumulus) const
    {
        stratus = m_stratus.GetParams();
        stratocumulus = m_stratocumulus.GetParams();
        cumulus = m_cumulus.GetParams();

        // Слоистые облака под слоисто-кучевыми: вершину слоистых держат на
        // 50 м ниже основания слоисто-кучевых, сначала опуская основание,
        // затем уменьшая толщину.
        static constexpr float LayerGapKm = 0.05f;
        static constexpr float MinThicknessKm = 0.05f;
        if (stratus.IsActive() && stratocumulus.IsActive())
        {
            const float maxTopKm = stratocumulus.m_baseKm - LayerGapKm;
            if (stratus.m_baseKm + stratus.m_thicknessKm > maxTopKm)
            {
                stratus.m_baseKm = AZStd::max(0.05f, maxTopKm - stratus.m_thicknessKm);
                stratus.m_thicknessKm = AZStd::max(MinThicknessKm, maxTopKm - stratus.m_baseKm);
            }
        }
    }

    void CloudscapeShaderConstantData::GetEffectiveSlab(float& baseKm, float& thicknessKm) const
    {
        CloudLayerParams layers[3];
        GetLayerParams(layers[0], layers[1], layers[2]);
        float minKm = AZStd::numeric_limits<float>::max();
        float maxKm = AZStd::numeric_limits<float>::lowest();
        for (const CloudLayerParams& layer : layers)
        {
            if (layer.IsActive())
            {
                minKm = AZStd::min(minKm, layer.m_baseKm);
                maxKm = AZStd::max(maxKm, layer.m_baseKm + layer.m_thicknessKm);
            }
        }
        if (minKm > maxKm)
        {
            baseKm = m_cloudSlabDistanceAboveSeaLevelKm;
            thicknessKm = m_cloudSlabThicknessKm;
            return;
        }
        baseKm = minKm;
        thicknessKm = maxKm - minKm;
    }

    void CloudscapeShaderConstantData::Reflect(AZ::ReflectContext* context)
    {
        CloudMaterialProperties::Reflect(context);
        CloudLayerAdvanced::Reflect(context);
        StratusLayer::Reflect(context);
        StratocumulusLayer::Reflect(context);
        CumulusLayer::Reflect(context);

        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CloudscapeShaderConstantData>()
                ->Version(1)
                ->Field("UVWScale", &CloudscapeShaderConstantData::m_uvwScale)
                ->Field("MaxMipLevels", &CloudscapeShaderConstantData::m_maxMipLevels)
                ->Field("MinRayMarchingSteps", &CloudscapeShaderConstantData::m_minRayMarchingSteps)
                ->Field("MaxRayMarchingSteps", &CloudscapeShaderConstantData::m_maxRayMarchingSteps)
                ->Field("PlanetRadiusKm", &CloudscapeShaderConstantData::m_planetRadiusKm)
                ->Field("CloudSlabDistanceAboveSeaLevelKm", &CloudscapeShaderConstantData::m_cloudSlabDistanceAboveSeaLevelKm)
                ->Field("CloudSlabThicknessKm", &CloudscapeShaderConstantData::m_cloudSlabThicknessKm)
                ->Field("SunLightIntensity", &CloudscapeShaderConstantData::m_sunLightIntensity)
                ->Field("AmbientLightColor", &CloudscapeShaderConstantData::m_ambientLightColor)
                ->Field("AmbientLightIntensity", &CloudscapeShaderConstantData::m_ambientLightIntensity)
                ->Field("WeatherMapSizeKm", &CloudscapeShaderConstantData::m_weatherMapSizeKm)
                ->Field("GlobalCloudCoverage", &CloudscapeShaderConstantData::m_globalCloudCoverage)
                ->Field("GlobalCloudDensity", &CloudscapeShaderConstantData::m_globalCloudDensity)
                ->Field("WindSpeedKmPerSec", &CloudscapeShaderConstantData::m_windSpeedKmPerSec)
                ->Field("WindDirection", &CloudscapeShaderConstantData::m_windDirection)
                ->Field("CloudTopOffsetKm", &CloudscapeShaderConstantData::m_cloudTopOffsetKm)
                ->Field("CloudEdgeSharpness", &CloudscapeShaderConstantData::m_cloudEdgeSharpness)
                ->Field("OvercastStartOktas", &CloudscapeShaderConstantData::m_overcastStartOktas)
                ->Field("OvercastCurve", &CloudscapeShaderConstantData::m_overcastCurve)
                ->Field("Stratus", &CloudscapeShaderConstantData::m_stratus)
                ->Field("Stratocumulus", &CloudscapeShaderConstantData::m_stratocumulus)
                ->Field("Cumulus", &CloudscapeShaderConstantData::m_cumulus)
                ->Field("CloudMaterialProperties", &CloudscapeShaderConstantData::m_cloudMaterialProperties)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CloudscapeShaderConstantData>(
                    "Shader Constants", "Data for the Cloudscape shader.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                    ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::Show)
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Cloud Layers")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_stratus, "Stratus", "Low uniform grey sheet. It always stays below the stratocumulus layer.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_stratocumulus, "Stratocumulus", "Low layer of flat lumpy cloud patches.")
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_cumulus, "Cumulus", "Separate clouds with dome tops.")
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_weatherMapSizeKm, "Weather Map Size", "Length of the weather map side in the world. Used only with a weather map.")
                            ->Attribute(AZ::Edit::Attributes::Suffix, " Km")
                            ->Attribute(AZ::Edit::Attributes::Min, 1.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 100.0)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_overcastStartOktas, "Overcast Start", "Cover value in oktas where the haze fill of gaps begins. The ramp ends at full cover.")
                            ->Attribute(AZ::Edit::Attributes::Suffix, " oktas")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                            ->Attribute(AZ::Edit::Attributes::Max, 8.0f)
                            ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                            ->Attribute(AZ::Edit::Attributes::Decimals, 2)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_overcastCurve, "Overcast Curve", "Shape of the fill ramp. 1 is linear. Above 1 the fill starts slow and finishes fast, below 1 the opposite.")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.1f)
                            ->Attribute(AZ::Edit::Attributes::Max, 4.0f)
                            ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                            ->Attribute(AZ::Edit::Attributes::Decimals, 2)
                    ->EndGroup()
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Structure")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_cloudEdgeSharpness, "Cloud Edge Sharpness", "Narrows the density ramp at cloud edges. 1 gives wide semi-transparent fringes, 4 gives crisp cumulus edges.")
                            ->Attribute(AZ::Edit::Attributes::Min, 1.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 16.0)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_uvwScale, "UVW Scale", "Noise texture tiles per kilometre. Larger values give smaller cloud puffs.")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.05)
                            ->Attribute(AZ::Edit::Attributes::Max, 1.0)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_windSpeedKmPerSec, "Wind Speed", "Clouds will appear to move at the speed of the wind.")
                            ->Attribute(AZ::Edit::Attributes::Suffix, " Km/s")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 0.1)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_windDirection, "Wind Direction", "")
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_cloudTopOffsetKm, "Cloud Top Shift Offset", "Proportionally pushes the top of the clouds along the wind direction by this distance.")
                            ->Attribute(AZ::Edit::Attributes::Suffix, " Km")
                            ->Attribute(AZ::Edit::Attributes::Min, -5.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 5.0)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_maxMipLevels, "Max Mip Levels", "Maximum number of mip levels used during noise texture sampling.")
                            ->Attribute(AZ::Edit::Attributes::Min, 1)
                            ->Attribute(AZ::Edit::Attributes::Max, 10)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_minRayMarchingSteps, "Min Ray Marching Steps", "The shorter the ray marching distance, the ray marching steps will be closer to this minimum count.")
                            ->Attribute(AZ::Edit::Attributes::Min, 1)
                            ->Attribute(AZ::Edit::Attributes::Max, 128)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_maxRayMarchingSteps, "Max Ray Marching Steps", "The longest the ray marching distance, the ray marching steps will be closer to this maximum count.")
                            ->Attribute(AZ::Edit::Attributes::Min, 1)
                            ->Attribute(AZ::Edit::Attributes::Max, 128)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_planetRadiusKm, "Planet Radius", "Defaults to Earth Radius = 6371[Km]")
                            ->Attribute(AZ::Edit::Attributes::Suffix, " Km")
                    ->EndGroup()
                    ->ClassElement(AZ::Edit::ClassElements::Group, "Lighting")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_sunLightIntensity, "Sun Light Intensity", "A scaling factor to the color of the sun light.")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 100.0)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_ambientLightColor, "Ambient Light Color", "Sky light color for the shaded side of the clouds. The sun color does not tint it.")
                        ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudscapeShaderConstantData::m_ambientLightIntensity, "Ambient Light Intensity", "A scaling factor to the color of the ambient light. It will be modulated by the Sun Light Intensity.")
                            ->Attribute(AZ::Edit::Attributes::Min, 0.0)
                            ->Attribute(AZ::Edit::Attributes::Max, 10.0)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &CloudscapeShaderConstantData::m_cloudMaterialProperties, "Cloud Material Properties", "")
                    ->EndGroup()
                    ;
            }
        }

    }

    bool CloudscapeShaderConstantData::operator==(const CloudscapeShaderConstantData& rhs) const
    {
        return AZ::IsClose(m_uvwScale, rhs.m_uvwScale, 0.00001f) &&
               (m_maxMipLevels == rhs.m_maxMipLevels) &&
               (m_minRayMarchingSteps == rhs.m_minRayMarchingSteps) &&
               (m_maxRayMarchingSteps == rhs.m_maxRayMarchingSteps) &&
               (m_planetRadiusKm ==  rhs.m_planetRadiusKm) &&
               AZ::IsClose(m_cloudSlabDistanceAboveSeaLevelKm, rhs.m_cloudSlabDistanceAboveSeaLevelKm) &&
               AZ::IsClose(m_cloudSlabThicknessKm, rhs.m_cloudSlabThicknessKm) &&
               AZ::IsClose(m_weatherMapSizeKm, rhs.m_weatherMapSizeKm) &&
               AZ::IsClose(m_sunLightIntensity, rhs.m_sunLightIntensity) &&
               m_ambientLightColor.IsClose(rhs.m_ambientLightColor) &&
               AZ::IsClose(m_ambientLightIntensity, rhs.m_ambientLightIntensity) &&
               AZ::IsClose(m_globalCloudCoverage, rhs.m_globalCloudCoverage) &&
               AZ::IsClose(m_globalCloudDensity, rhs.m_globalCloudDensity) &&
               AZ::IsClose(m_windSpeedKmPerSec, rhs.m_windSpeedKmPerSec) &&
               m_windDirection.IsClose(rhs.m_windDirection) &&
               AZ::IsClose(m_cloudTopOffsetKm, rhs.m_cloudTopOffsetKm) &&
               AZ::IsClose(m_cloudEdgeSharpness, rhs.m_cloudEdgeSharpness) &&
               AZ::IsClose(m_overcastStartOktas, rhs.m_overcastStartOktas) &&
               AZ::IsClose(m_overcastCurve, rhs.m_overcastCurve) &&
               m_stratus == rhs.m_stratus &&
               m_stratocumulus == rhs.m_stratocumulus &&
               m_cumulus == rhs.m_cumulus &&
               (m_cloudMaterialProperties == rhs.m_cloudMaterialProperties)
               ;
    }

    bool CloudscapeShaderConstantData::operator!=(const CloudscapeShaderConstantData& rhs) const
    {
        return !(*this == rhs);
    }

} // namespace VolumetricClouds
