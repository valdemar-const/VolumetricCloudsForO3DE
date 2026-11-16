/*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/
#pragma once

#include <AzCore/RTTI/TypeInfoSimple.h>
#include <AzCore/Memory/SystemAllocator.h>

namespace AZ
{
    class ReflectContext;
}

namespace VolumetricClouds
{
    // Поправки к значениям, которые слой выводит из рода облаков, балла и
    // стадии. Множитель 1 оставляет выведенное значение.
    struct CloudLayerAdvanced
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO_WITH_NAME_DECL(CloudLayerAdvanced);

        static void Reflect(AZ::ReflectContext* context);

        bool operator==(const CloudLayerAdvanced& rhs) const;
        bool operator!=(const CloudLayerAdvanced& rhs) const { return !(*this == rhs); }

        float m_densityScale = 1.0f;
        float m_thicknessScale = 1.0f;
        float m_topVariationScale = 1.0f;
        float m_baseReliefScale = 1.0f;
    };

    // Значения слоя для шейдера. Высоты в км над уровнем моря.
    struct CloudLayerParams
    {
        bool IsActive() const { return m_cover > 0.0f && m_thicknessKm > 0.0f; }

        float m_baseKm = 0.0f;
        float m_thicknessKm = 0.0f;
        // Доля неба под слоем, 0..1: балл в октах, делённый на 8.
        float m_cover = 0.0f;
        // Множитель коэффициента рассеяния материала облака.
        float m_density = 0.0f;
        float m_topVariation = 0.0f;
        float m_baseRelief = 0.0f;
    };

    // Слоистые облака (Stratus, St): ровный серый слой с размытым верхом.
    struct StratusLayer
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO_WITH_NAME_DECL(StratusLayer);

        static void Reflect(AZ::ReflectContext* context);

        bool operator==(const StratusLayer& rhs) const;
        bool operator!=(const StratusLayer& rhs) const { return !(*this == rhs); }

        CloudLayerParams GetParams() const;

        // Балл в октах, 0..8.
        float m_coverOktas = 0.0f;
        float m_baseAltitudeKm = 0.4f;
        // 0 - translucidus (солнце видно сквозь слой), 1 - opacus (слой
        // закрывает солнце).
        float m_opacity = 0.5f;
        CloudLayerAdvanced m_advanced;
    };

    // Слоисто-кучевые облака (Stratocumulus, Sc): слой плоских клубов.
    struct StratocumulusLayer
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO_WITH_NAME_DECL(StratocumulusLayer);

        static void Reflect(AZ::ReflectContext* context);

        bool operator==(const StratocumulusLayer& rhs) const;
        bool operator!=(const StratocumulusLayer& rhs) const { return !(*this == rhs); }

        CloudLayerParams GetParams() const;

        float m_coverOktas = 2.0f;
        float m_baseAltitudeKm = 1.0f;
        float m_opacity = 0.5f;
        CloudLayerAdvanced m_advanced;
    };

    // Кучевые облака (Cumulus, Cu): отдельные облака с куполом.
    struct CumulusLayer
    {
        AZ_CLASS_ALLOCATOR_DECL;
        AZ_TYPE_INFO_WITH_NAME_DECL(CumulusLayer);

        static void Reflect(AZ::ReflectContext* context);

        bool operator==(const CumulusLayer& rhs) const;
        bool operator!=(const CumulusLayer& rhs) const { return !(*this == rhs); }

        CloudLayerParams GetParams() const;

        // Кучевые облака не дают сплошного слоя: при 7-8 октах наблюдатель
        // называет слой слоисто-кучевым.
        static constexpr float MaxCoverOktas = 6.0f;

        float m_coverOktas = 4.0f;
        float m_baseAltitudeKm = 1.5f;
        // Стадия развития: 1 - humilis, 2 - mediocris, 3 - congestus.
        float m_development = 2.0f;
        CloudLayerAdvanced m_advanced;
    };
} // namespace VolumetricClouds
