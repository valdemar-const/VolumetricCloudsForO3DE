/*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include <AzCore/std/algorithm.h>

#include <VolumetricClouds/VolumetricCloudsTypeIds.h>
#include "CloudLayers.h"

namespace VolumetricClouds
{
    AZ_CLASS_ALLOCATOR_IMPL(CloudLayerAdvanced, AZ::SystemAllocator);
    AZ_TYPE_INFO_WITH_NAME_IMPL(CloudLayerAdvanced, "VolumetricClouds::CloudLayerAdvanced", CloudLayerAdvancedTypeId);
    AZ_CLASS_ALLOCATOR_IMPL(StratusLayer, AZ::SystemAllocator);
    AZ_TYPE_INFO_WITH_NAME_IMPL(StratusLayer, "VolumetricClouds::StratusLayer", StratusLayerTypeId);
    AZ_CLASS_ALLOCATOR_IMPL(StratocumulusLayer, AZ::SystemAllocator);
    AZ_TYPE_INFO_WITH_NAME_IMPL(StratocumulusLayer, "VolumetricClouds::StratocumulusLayer", StratocumulusLayerTypeId);
    AZ_CLASS_ALLOCATOR_IMPL(CumulusLayer, AZ::SystemAllocator);
    AZ_TYPE_INFO_WITH_NAME_IMPL(CumulusLayer, "VolumetricClouds::CumulusLayer", CumulusLayerTypeId);

    namespace
    {
        constexpr const char* CoverTooltip =
            "Cloud amount in oktas, eighths of the sky: 0 SKC clear, 1-2 FEW, 3-4 SCT scattered, 5-7 BKN broken, 8 OVC overcast.";
        constexpr const char* OpacityTooltip =
            "0 translucidus: the sun shows through the layer. 1 opacus: the layer hides the sun. Thickness and density follow.";

        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * AZStd::clamp(t, 0.0f, 1.0f);
        }

        // Поправки ограничены, чтобы слой не терял род облаков: толщина
        // меняется не более чем вдвое, разброс и рельеф остаются в 0..1.
        void ApplyAdvanced(const CloudLayerAdvanced& advanced, CloudLayerParams& params)
        {
            params.m_density *= AZStd::clamp(advanced.m_densityScale, 0.0f, 4.0f);
            params.m_thicknessKm *= AZStd::clamp(advanced.m_thicknessScale, 0.5f, 2.0f);
            params.m_topVariation = AZStd::clamp(params.m_topVariation * advanced.m_topVariationScale, 0.0f, 1.0f);
            params.m_baseRelief = AZStd::clamp(params.m_baseRelief * advanced.m_baseReliefScale, 0.0f, 1.0f);
        }

        template<typename Layer>
        void ReflectCover(AZ::EditContext::ClassBuilder& builder, float maxOktas)
        {
            builder->DataElement(AZ::Edit::UIHandlers::Slider, &Layer::m_coverOktas, "Cover", CoverTooltip)
                ->Attribute(AZ::Edit::Attributes::Suffix, " oktas")
                ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                ->Attribute(AZ::Edit::Attributes::Max, maxOktas)
                ->Attribute(AZ::Edit::Attributes::Step, 0.01f)
                ->Attribute(AZ::Edit::Attributes::Decimals, 2);
        }

        template<typename Layer>
        void ReflectBase(AZ::EditContext::ClassBuilder& builder, float minKm, float maxKm)
        {
            builder->DataElement(AZ::Edit::UIHandlers::Slider, &Layer::m_baseAltitudeKm, "Base Altitude",
                    "Altitude of the cloud base above sea level. The range is typical for this cloud genus.")
                ->Attribute(AZ::Edit::Attributes::Suffix, " Km")
                ->Attribute(AZ::Edit::Attributes::Min, minKm)
                ->Attribute(AZ::Edit::Attributes::Max, maxKm)
                ->Attribute(AZ::Edit::Attributes::Step, 0.05f);
        }

        template<typename Layer>
        void ReflectAdvanced(AZ::EditContext::ClassBuilder& builder)
        {
            builder->DataElement(AZ::Edit::UIHandlers::Default, &Layer::m_advanced, "Advanced",
                    "Corrections to the values that the layer derives from genus, cover and stage.")
                ->Attribute(AZ::Edit::Attributes::AutoExpand, false);
        }
    } // namespace

    void CloudLayerAdvanced::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CloudLayerAdvanced>()
                ->Version(1)
                ->Field("DensityScale", &CloudLayerAdvanced::m_densityScale)
                ->Field("ThicknessScale", &CloudLayerAdvanced::m_thicknessScale)
                ->Field("TopVariationScale", &CloudLayerAdvanced::m_topVariationScale)
                ->Field("BaseReliefScale", &CloudLayerAdvanced::m_baseReliefScale)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                editContext->Class<CloudLayerAdvanced>("Advanced", "Corrections to the derived layer values. 1 keeps the derived value.")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudLayerAdvanced::m_densityScale, "Density Scale", "Multiplies the optical density of the layer.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 4.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudLayerAdvanced::m_thicknessScale, "Thickness Scale", "Multiplies the layer thickness.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.5f)
                        ->Attribute(AZ::Edit::Attributes::Max, 2.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudLayerAdvanced::m_topVariationScale, "Top Variation Scale", "Multiplies the spread of cloud top heights. 0 gives one top height for the whole layer.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 3.0f)
                    ->DataElement(AZ::Edit::UIHandlers::Slider, &CloudLayerAdvanced::m_baseReliefScale, "Base Relief Scale", "Multiplies the relief of the cloud base. 0 gives a flat base.")
                        ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                        ->Attribute(AZ::Edit::Attributes::Max, 3.0f)
                    ;
            }
        }
    }

    bool CloudLayerAdvanced::operator==(const CloudLayerAdvanced& rhs) const
    {
        return AZ::IsClose(m_densityScale, rhs.m_densityScale) &&
               AZ::IsClose(m_thicknessScale, rhs.m_thicknessScale) &&
               AZ::IsClose(m_topVariationScale, rhs.m_topVariationScale) &&
               AZ::IsClose(m_baseReliefScale, rhs.m_baseReliefScale);
    }

    void StratusLayer::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StratusLayer>()
                ->Version(1)
                ->Field("CoverOktas", &StratusLayer::m_coverOktas)
                ->Field("BaseAltitudeKm", &StratusLayer::m_baseAltitudeKm)
                ->Field("Opacity", &StratusLayer::m_opacity)
                ->Field("Advanced", &StratusLayer::m_advanced)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                auto builder = editContext->Class<StratusLayer>("Stratus", "Low uniform grey sheet.");
                builder->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
                ReflectCover<StratusLayer>(builder, 8.0f);
                ReflectBase<StratusLayer>(builder, 0.05f, 1.5f);
                builder->DataElement(AZ::Edit::UIHandlers::Slider, &StratusLayer::m_opacity, "Opacity", OpacityTooltip)
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f);
                ReflectAdvanced<StratusLayer>(builder);
            }
        }
    }

    bool StratusLayer::operator==(const StratusLayer& rhs) const
    {
        return AZ::IsClose(m_coverOktas, rhs.m_coverOktas) &&
               AZ::IsClose(m_baseAltitudeKm, rhs.m_baseAltitudeKm) &&
               AZ::IsClose(m_opacity, rhs.m_opacity) &&
               m_advanced == rhs.m_advanced;
    }

    // Толщина слоистых облаков 150-600 м. Плотность подобрана так, что
    // оптическая толщина translucidus около 2 (диск солнца виден), а
    // opacus около 20 (солнце скрыто).
    CloudLayerParams StratusLayer::GetParams() const
    {
        CloudLayerParams params;
        params.m_baseKm = AZStd::clamp(m_baseAltitudeKm, 0.05f, 1.5f);
        params.m_thicknessKm = Lerp(0.15f, 0.6f, m_opacity);
        params.m_cover = AZStd::clamp(m_coverOktas, 0.0f, 8.0f) / 8.0f;
        params.m_density = Lerp(0.25f, 0.8f, m_opacity);
        params.m_topVariation = 0.0f;
        params.m_baseRelief = 0.15f;
        ApplyAdvanced(m_advanced, params);
        return params;
    }

    void StratocumulusLayer::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<StratocumulusLayer>()
                ->Version(1)
                ->Field("CoverOktas", &StratocumulusLayer::m_coverOktas)
                ->Field("BaseAltitudeKm", &StratocumulusLayer::m_baseAltitudeKm)
                ->Field("Opacity", &StratocumulusLayer::m_opacity)
                ->Field("Advanced", &StratocumulusLayer::m_advanced)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                auto builder = editContext->Class<StratocumulusLayer>("Stratocumulus", "Low layer of flat lumpy cloud patches.");
                builder->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
                ReflectCover<StratocumulusLayer>(builder, 8.0f);
                ReflectBase<StratocumulusLayer>(builder, 0.3f, 2.5f);
                builder->DataElement(AZ::Edit::UIHandlers::Slider, &StratocumulusLayer::m_opacity, "Opacity", OpacityTooltip)
                    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 1.0f);
                ReflectAdvanced<StratocumulusLayer>(builder);
            }
        }
    }

    bool StratocumulusLayer::operator==(const StratocumulusLayer& rhs) const
    {
        return AZ::IsClose(m_coverOktas, rhs.m_coverOktas) &&
               AZ::IsClose(m_baseAltitudeKm, rhs.m_baseAltitudeKm) &&
               AZ::IsClose(m_opacity, rhs.m_opacity) &&
               m_advanced == rhs.m_advanced;
    }

    // Толщина слоисто-кучевых облаков 250-900 м.
    CloudLayerParams StratocumulusLayer::GetParams() const
    {
        CloudLayerParams params;
        params.m_baseKm = AZStd::clamp(m_baseAltitudeKm, 0.3f, 2.5f);
        params.m_thicknessKm = Lerp(0.25f, 0.9f, m_opacity);
        params.m_cover = AZStd::clamp(m_coverOktas, 0.0f, 8.0f) / 8.0f;
        params.m_density = Lerp(0.25f, 0.6f, m_opacity);
        params.m_topVariation = 0.3f;
        params.m_baseRelief = 0.4f;
        ApplyAdvanced(m_advanced, params);
        return params;
    }

    void CumulusLayer::Reflect(AZ::ReflectContext* context)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<CumulusLayer>()
                ->Version(1)
                ->Field("CoverOktas", &CumulusLayer::m_coverOktas)
                ->Field("BaseAltitudeKm", &CumulusLayer::m_baseAltitudeKm)
                ->Field("Development", &CumulusLayer::m_development)
                ->Field("Advanced", &CumulusLayer::m_advanced)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                auto builder = editContext->Class<CumulusLayer>("Cumulus", "Separate clouds with dome tops.");
                builder->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                    ->Attribute(AZ::Edit::Attributes::AutoExpand, true);
                ReflectCover<CumulusLayer>(builder, MaxCoverOktas);
                ReflectBase<CumulusLayer>(builder, 0.3f, 3.0f);
                builder->DataElement(AZ::Edit::UIHandlers::Slider, &CumulusLayer::m_development, "Development",
                        "Growth stage: 1 humilis, flat fair-weather clouds about 0.5 km tall. 2 mediocris, about 1.5 km. 3 congestus, towers about 4 km tall.")
                    ->Attribute(AZ::Edit::Attributes::Min, 1.0f)
                    ->Attribute(AZ::Edit::Attributes::Max, 3.0f);
                ReflectAdvanced<CumulusLayer>(builder);
            }
        }
    }

    bool CumulusLayer::operator==(const CumulusLayer& rhs) const
    {
        return AZ::IsClose(m_coverOktas, rhs.m_coverOktas) &&
               AZ::IsClose(m_baseAltitudeKm, rhs.m_baseAltitudeKm) &&
               AZ::IsClose(m_development, rhs.m_development) &&
               m_advanced == rhs.m_advanced;
    }

    // Высота кучевых облаков по стадиям: humilis около 0.5 км, mediocris
    // около 1.5 км, congestus около 4 км. С ростом облака растёт и разброс
    // высоты вершин соседних облаков.
    CloudLayerParams CumulusLayer::GetParams() const
    {
        const float development = AZStd::clamp(m_development, 1.0f, 3.0f);
        CloudLayerParams params;
        params.m_baseKm = AZStd::clamp(m_baseAltitudeKm, 0.3f, 3.0f);
        params.m_thicknessKm = development <= 2.0f
            ? Lerp(0.5f, 1.5f, development - 1.0f)
            : Lerp(1.5f, 4.0f, development - 2.0f);
        params.m_cover = AZStd::clamp(m_coverOktas, 0.0f, MaxCoverOktas) / 8.0f;
        params.m_density = 0.3f;
        params.m_topVariation = Lerp(0.3f, 0.7f, (development - 1.0f) * 0.5f);
        params.m_baseRelief = 0.6f;
        ApplyAdvanced(m_advanced, params);
        return params;
    }
} // namespace VolumetricClouds
