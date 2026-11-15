/*
* Copyright (c) Galib Arrieta (aka lumbermixalot@github, aka galibzon@github).
*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/

#include <AzCore/Name/NameDictionary.h>

#include <Atom/RHI/DrawPacketBuilder.h>
#include <Atom/RHI.Reflect/InputStreamLayoutBuilder.h>

#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/RenderPipeline.h>

#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Reflect/Asset/AssetUtils.h> // FIXME: Try removing

#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/RasterPass.h>
#include <Atom/RPI.Public/Shader/Shader.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/ViewportContext.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>

#include <Renderer/Passes/CloudscapeComputePass.h>
#include <Renderer/Passes/CloudscapeRasterPass.h>
// #include <Renderer/Passes/DepthBufferCopyPass.h>
#include "CloudscapeFeatureProcessor.h"

namespace VolumetricClouds
{
    void CloudscapeFeatureProcessor::Reflect(AZ::ReflectContext* context)
    {
        if (auto* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext
                ->Class<CloudscapeFeatureProcessor, AZ::RPI::FeatureProcessor>()
                ->Version(1);
        }
    }

    /////////////////////////////////////////////////////////////////////////////
    //! AZ::RPI::FeatureProcessor overrides START ...
    void CloudscapeFeatureProcessor::Activate()
    {
        ActivateInternal();
    }

    void CloudscapeFeatureProcessor::Deactivate()
    {
        if (m_cloudscapeComputePass)
        {
            // This is necessary to avoid pesky error messages of invalid attachments when
            // the feature processor is being destroyed.
            m_cloudscapeComputePass->QueueForRemoval();
            m_cloudscapeReprojectionPass->QueueForRemoval();
            m_cloudscapeRenderPass->QueueForRemoval();
            //m_depthBufferCopyPass->QueueForRemoval();
        }

        DisableSceneNotification();
        m_viewportSize = { 0,0 };
    }

    void CloudscapeFeatureProcessor::Simulate(const SimulatePacket&)
    {
        ResizeOutputAttachmentsIfNeeded();

        if (m_cloudscapeComputePass)
        {
            m_cloudscapeComputePass->UpdateFrameCounter(m_frameCounter);

            const auto& passSrg = m_cloudscapeReprojectionPass->GetShaderResourceGroup();
            const uint32_t pixelIndex4x4 = m_frameCounter % 16;
            passSrg->SetConstant(m_pixelIndex4x4Index, pixelIndex4x4);

            m_cloudscapeRenderPass->UpdateFrameCounter(m_frameCounter);
            
            m_frameCounter++;
        }
    }

    void CloudscapeFeatureProcessor::AddRenderPasses([[maybe_unused]] AZ::RPI::RenderPipeline* renderPipeline)
    {
        // Get the pass requests to create passes from the asset
        AddPassRequestToRenderPipeline(renderPipeline, "Passes/CloudscapeComputePassRequest.azasset", "DepthPrePass", false /*before*/);
        // Hold a reference to the compute pass
        {
            const auto passName = AZ::Name("CloudscapeComputePass");
            AZ::RPI::PassFilter passFilter = AZ::RPI::PassFilter::CreateWithPassName(passName, renderPipeline);
            AZ::RPI::Pass* existingPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter);
            m_cloudscapeComputePass = azrtti_cast<CloudscapeComputePass*>(existingPass);
            if (!m_cloudscapeComputePass)
            {
                AZ_Error(LogName, false, "%s Failed to find as RenderPass: %s", __FUNCTION__, passName.GetCStr());
                return;
            }

            if (m_shaderConstantData)
            {
                m_cloudscapeComputePass->UpdateShaderConstantData(*m_shaderConstantData);
            }
        }

        AddPassRequestToRenderPipeline(renderPipeline, "Passes/CloudscapeReprojectionComputePassRequest.azasset", "MotionVectorPass", false /*before*/);
        // Hold a reference to the compute pass
        {
            const auto passName = AZ::Name("CloudscapeReprojectionComputePass");
            AZ::RPI::PassFilter passFilter = AZ::RPI::PassFilter::CreateWithPassName(passName, renderPipeline);
            AZ::RPI::Pass* existingPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter);
            m_cloudscapeReprojectionPass = azrtti_cast<AZ::RPI::ComputePass*>(existingPass);
            if (!m_cloudscapeReprojectionPass)
            {
                AZ_Error(LogName, false, "%s Failed to find as RenderPass: %s", __FUNCTION__, passName.GetCStr());
                return;
            }
            m_cloudscapeReprojectionPass->SetTargetThreadCounts(m_viewportSize.m_width, m_viewportSize.m_height, 1);
        }


        AddPassRequestToRenderPipeline(renderPipeline, "Passes/CloudscapeRasterPassRequest.azasset", "TransparentPass", true /*before*/);
        // Hold a reference to the render pass
        {
            const auto passName = AZ::Name("CloudscapeRasterPass");
            AZ::RPI::PassFilter passFilter = AZ::RPI::PassFilter::CreateWithPassName(passName, renderPipeline);
            AZ::RPI::Pass* existingPass = AZ::RPI::PassSystemInterface::Get()->FindFirstPass(passFilter);
            m_cloudscapeRenderPass = azrtti_cast<CloudscapeRasterPass*>(existingPass);
            if (!m_cloudscapeRenderPass)
            {
                AZ_Error(LogName, false, "%s Failed to find as RenderPass: %s", __FUNCTION__, passName.GetCStr());
                return;
            }
        }

    }

    //! AZ::RPI::FeatureProcessor overrides END ...
    /////////////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////////////
    //! Functions called by CloudscapeComponentController START
    void CloudscapeFeatureProcessor::UpdateShaderConstantData(const CloudscapeShaderConstantData& shaderData)
    {
        m_shaderConstantData = &shaderData;
        if (m_cloudscapeComputePass)
        {
            m_cloudscapeComputePass->UpdateShaderConstantData(shaderData);
        }
    }

    //! Functions called by CloudscapeComponentController END
    /////////////////////////////////////////////////////////////////////


    void CloudscapeFeatureProcessor::ActivateInternal()
    {
        auto viewportContextInterface = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        auto viewportContext = viewportContextInterface->GetViewportContextByScene(GetParentScene());
        m_viewportSize = viewportContext->GetViewportSize();

        CreateOutputAttachments();

        DisableSceneNotification();
        EnableSceneNotification();
    }

    void CloudscapeFeatureProcessor::CreateOutputAttachments()
    {
        const AZStd::string name0 = AZStd::string::format("CloudscapeOutput0_%u", m_attachmentGeneration);
        const AZStd::string name1 = AZStd::string::format("CloudscapeOutput1_%u", m_attachmentGeneration);
        m_cloudOutput0 = CreateCloudscapeOutputAttachment(AZ::Name(name0), m_viewportSize, AZ::RHI::Format::R16G16B16A16_FLOAT);
        AZ_Assert(!!m_cloudOutput0, "Failed to create %s", name0.c_str());
        m_cloudOutput1 = CreateCloudscapeOutputAttachment(AZ::Name(name1), m_viewportSize, AZ::RHI::Format::R16G16B16A16_FLOAT);
        AZ_Assert(!!m_cloudOutput1, "Failed to create %s", name1.c_str());

        // Цвет и глубины упакованы в half: CloudscapeCommon.azsli PackCloudTrace.
        const AZStd::string traceName = AZStd::string::format("CloudscapeTrace_%u", m_attachmentGeneration);
        const AzFramework::WindowSize traceSize{ (m_viewportSize.m_width + 3) / 4, (m_viewportSize.m_height + 3) / 4 };
        m_cloudTrace = CreateCloudscapeOutputAttachment(AZ::Name(traceName), traceSize, AZ::RHI::Format::R32G32B32A32_UINT);
        AZ_Assert(!!m_cloudTrace, "Failed to create %s", traceName.c_str());
    }

    void CloudscapeFeatureProcessor::ResizeOutputAttachmentsIfNeeded()
    {
        auto viewportContextInterface = AZ::Interface<AZ::RPI::ViewportContextRequestsInterface>::Get();
        auto viewportContext = viewportContextInterface ? viewportContextInterface->GetViewportContextByScene(GetParentScene()) : nullptr;
        if (!viewportContext)
        {
            return;
        }

        const AzFramework::WindowSize size = viewportContext->GetViewportSize();
        if (size.m_width == 0 || size.m_height == 0 ||
            (size.m_width == m_viewportSize.m_width && size.m_height == m_viewportSize.m_height))
        {
            return;
        }

        m_viewportSize = size;
        ++m_attachmentGeneration;
        CreateOutputAttachments();

        // Привязку текстур к слотам выполняет BuildInternal вычислительного
        // прохода; остальные проходы получают текстуры через соединения слотов.
        if (m_cloudscapeComputePass)
        {
            m_cloudscapeComputePass->QueueForBuildAndInitialization();
        }
        if (m_cloudscapeReprojectionPass)
        {
            m_cloudscapeReprojectionPass->SetTargetThreadCounts(m_viewportSize.m_width, m_viewportSize.m_height, 1);
            m_cloudscapeReprojectionPass->QueueForBuildAndInitialization();
        }
        if (m_cloudscapeRenderPass)
        {
            m_cloudscapeRenderPass->QueueForBuildAndInitialization();
        }
    }


    AZ::Data::Instance<AZ::RPI::AttachmentImage> CloudscapeFeatureProcessor::CreateCloudscapeOutputAttachment(const AZ::Name& attachmentName
        , const AzFramework::WindowSize attachmentSize, AZ::RHI::Format format) const
    {
        // Яркость освещённого солнцем облака превышает 1.0; формат UNORM
        // обрезает её и даёт полосы на градиентах.
        AZ::RHI::ImageDescriptor imageDesc = AZ::RHI::ImageDescriptor::Create2D(
            AZ::RHI::ImageBindFlags::ShaderReadWrite, attachmentSize.m_width, attachmentSize.m_height, format);
        AZ::RHI::ClearValue clearValue = AZ::RHI::ClearValue::CreateVector4Float(0, 0, 0, 0);
        AZ::Data::Instance<AZ::RPI::AttachmentImagePool> pool = AZ::RPI::ImageSystemInterface::Get()->GetSystemAttachmentPool();
        return AZ::RPI::AttachmentImage::Create(*pool.get(), imageDesc, attachmentName, &clearValue, nullptr);
    }

} // namespace VolumetricClouds
