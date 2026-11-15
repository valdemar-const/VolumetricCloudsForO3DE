/*
* Copyright (c) Galib Arrieta (aka lumbermixalot@github, aka galibzon@github).
*
* SPDX-License-Identifier: Apache-2.0 OR MIT
*
*/

#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/View.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/PassSystem.h>
#include <Atom/RPI.Public/Image/AttachmentImagePool.h>
#include <Atom/RPI.Public/Image/ImageSystemInterface.h>

#include <Atom/RHI/FrameScheduler.h>
#include <Atom/RHI/PipelineState.h>

#include <Renderer/CloudscapeFeatureProcessor.h>
#include "CloudscapeRasterPass.h"

namespace VolumetricClouds
{
    
    AZ::RPI::Ptr<CloudscapeRasterPass> CloudscapeRasterPass::Create(const AZ::RPI::PassDescriptor& descriptor)
    {
        AZ::RPI::Ptr<CloudscapeRasterPass> pass = aznew CloudscapeRasterPass(descriptor);
        return AZStd::move(pass);
    }
    
    CloudscapeRasterPass::CloudscapeRasterPass(const AZ::RPI::PassDescriptor& descriptor)
        : AZ::RPI::FullscreenTrianglePass(descriptor)
    {
    }
    
    void CloudscapeRasterPass::InitializeInternal()
    {
        AZ::RPI::FullscreenTrianglePass::InitializeInternal();

        m_cloudscapeTextureIndex = 0;
        m_srgNeedsUpdate = true;

        //InitializeShaderVariant();
    }


    void CloudscapeRasterPass::BuildInternal()
    {
        auto* featureProcessor = m_pipeline ? m_pipeline->GetScene()->GetFeatureProcessor<CloudscapeFeatureProcessor>() : nullptr;
        if (featureProcessor)
        {
            // Без SkyAtmosphere слот получает заглушку, и шейдер не читает объём.
            auto skyVolume = featureProcessor->GetSkyVolumeLUT();
            m_aerialPerspectiveEnabled = skyVolume ? 1 : 0;
            AttachImageToSlot(AZ::Name("SkyVolumeLUT"), skyVolume ? skyVolume : featureProcessor->GetSkyVolumeFallback());
            m_srgNeedsUpdate = true;
        }
        AZ::RPI::FullscreenTrianglePass::BuildInternal();
    }

    void CloudscapeRasterPass::FrameBeginInternal(FramePrepareParams params)
    {
        AZ::RPI::FullscreenTrianglePass::FrameBeginInternal(params);
    }


    void CloudscapeRasterPass::SetupFrameGraphDependencies(AZ::RHI::FrameGraphInterface frameGraph)
    {
        AZ::RPI::FullscreenTrianglePass::SetupFrameGraphDependencies(frameGraph);
    }

    
    void CloudscapeRasterPass::CompileResources(const AZ::RHI::FrameGraphCompileContext& context)
    {
       AZ_Assert(m_shaderResourceGroup != nullptr, "CloudscapeRasterPass %s has a null shader resource group when calling Compile.", GetPathName().GetCStr());

       if (m_srgNeedsUpdate)
       {
           m_shaderResourceGroup->SetConstant(m_cloudscapeTextureIndexIndex, m_cloudscapeTextureIndex);
           m_shaderResourceGroup->SetConstant(m_aerialPerspectiveEnabledIndex, m_aerialPerspectiveEnabled);
           m_srgNeedsUpdate = false;
       }

       AZ::RPI::FullscreenTrianglePass::CompileResources(context);
    }
    
    
    void CloudscapeRasterPass::BuildCommandListInternal(const AZ::RHI::FrameGraphExecuteContext& context)
    {
        //AZ_Assert(m_shaderResourceGroup != nullptr, "CloudscapeRasterPass %s has a null shader resource group when calling Execute.", GetPathName().GetCStr());
        //
        //AZ::RHI::CommandList* commandList = context.GetCommandList();
        //
        //commandList->SetViewport(m_viewportState);
        //commandList->SetScissor(m_scissorState);
        //
        //SetSrgsForDraw(commandList);
        //
        //m_item.m_pipelineState = GetPipelineStateFromShaderVariant();
        //
        //commandList->Submit(m_item);
        AZ::RPI::FullscreenTrianglePass::BuildCommandListInternal(context);
    }


    void CloudscapeRasterPass::UpdateFrameCounter(uint32_t frameCounter)
    {
        // Проход репроекции пишет текстуру с индексом, равным чётности кадра.
        m_cloudscapeTextureIndex = frameCounter & 1;
        m_srgNeedsUpdate = true;
    }

}   // VolumetricClouds AZ
