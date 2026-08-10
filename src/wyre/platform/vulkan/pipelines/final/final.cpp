/**
 * @file pipelines/final.cpp
 * @brief Vulkan final pass pipeline.
 */
#include "final.h"

#include "vulkan/shader/module.h" /* shader::from_file */
#include "vulkan/hardware/image.h"
#include "vulkan/hardware/pipeline-builder.h"
#include "vulkan/device.h"

#include "wyre/core/system/window.h" /* wyre::Window */
#include "wyre/core/system/log.h"

namespace wyre {

/* Final shaders */
const char* FINAL_SHADER = "assets/shaders/final.slang.spv";

FinalPipeline::FinalPipeline(Logger& logger, const Window& window, const Device& device) {
    /* Load the uber fragment & vertex shader modules */
    final_shader = shader::from_file(device.device, FINAL_SHADER).expect("failed to load final shader.");

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "loaded final shader modules.");

    PipelineBuilder builder{};
    /* Shader stages */
    builder.add_shader_stage(ShaderStage::eVertex, final_shader, "entry_vertex");
    builder.add_shader_stage(ShaderStage::eFragment, final_shader, "entry_pixel");
    /* Dynamic state */
    builder.add_dynamic_state(vk::DynamicState::eViewport);
    builder.add_dynamic_state(vk::DynamicState::eScissor);
    /* Vertex input */
    builder.set_primitive_topology(vk::PrimitiveTopology::eTriangleList);
    /* Color blend mode */
    builder.add_basic_colorblend_attachment(false);
    builder.add_color_attachment(device.swapchain_fmt);
    /* Rasterizer */
    builder.add_viewport(0, 0, window.width, window.height, 0.0f, 1.0f);
    builder.add_scissor(0, 0, window.width, window.height);
    builder.set_polygon_mode(vk::PolygonMode::eFill);
    /* Descriptor sets */
    builder.add_descriptor_set(device.get_frame().attach_render_desc.layout);

    { /* Build the pipeline layout */
        const vk::ResultValue result = builder.build_layout(device.device);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create final pipeline layout.");
            return;
        }
        layout = result.value;
    }

    { /* Build the graphics pipeline */
        const vk::ResultValue result = builder.build_pipeline(device.device, layout);
        if (result.result != vk::Result::eSuccess) {
            logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to create final graphics pipeline.");
            return;
        }
        pipeline = result.value;
    }

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized final pipeline.");
}

/**
 * @brief Push final pipeline commands into the graphics command buffer.
 */
void FinalPipeline::enqueue(const Window& window, const Device& device) {
    /* Fetch the command buffer & render target */
    const vk::CommandBuffer& cmd = device.get_frame().gcb;
    const img::RenderAttachment& albedo = device.get_frame().albedo;
    const img::RenderAttachment& normal_depth = device.get_frame().normal_depth;
    const wyre::DescriptorSet& desc_set = device.get_frame().attach_render_desc;
    const RenderTarget& rt = device.get_rt();

    /* Transition render target into color attachment format */
    img::barrier(cmd, rt.img,
        /* Src */ img::PStage::eBottomOfPipe, img::Access::eNone, img::Layout::eUndefined,
        /* Dst */ img::PStage::eFragmentShader, img::Access::eShaderRead, img::Layout::eColorAttachmentOptimal);

    /* Place a memory barrier on the albedo output, we have to wait for the primary pass to finish */
    img::barrier(cmd, albedo.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eFragmentShader, img::Access::eShaderRead, img::Layout::eShaderReadOnlyOptimal);
    img::barrier(cmd, normal_depth.image,
        /* Src */ img::PStage::eComputeShader, img::Access::eShaderWrite, img::Layout::eGeneral,
        /* Dst */ img::PStage::eFragmentShader, img::Access::eShaderRead, img::Layout::eShaderReadOnlyOptimal);

    /* Define the render target attachment */
    vk::RenderingAttachmentInfoKHR attachment_info{};
    attachment_info.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachment_info.storeOp = vk::AttachmentStoreOp::eStore;
    attachment_info.loadOp = vk::AttachmentLoadOp::eClear;
    attachment_info.clearValue.color = vk::ClearColorValue({0.05f, 0.05f, 0.05f, 1.0f});
    attachment_info.imageView = rt.view;

    /* Viewport & scissor */
    const vk::Viewport viewport(0, 0, window.width, window.height, 0.0f, 1.0f);
    const vk::Rect2D scissor({0, 0}, {window.width, window.height});

    /* Rendering info */
    vk::RenderingInfoKHR rendering_info{};
    rendering_info.renderArea.extent = vk::Extent2D{window.width, window.height};
    rendering_info.renderArea.offset = vk::Offset2D{0, 0};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachments = &attachment_info;
    rendering_info.layerCount = 1;

    /* Setup for rendering */
    cmd.beginRenderingKHR(&rendering_info, device.dldi); /* Start rendering */
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0u, desc_set.set, {});

    /* Draw full-screen quad */
    cmd.setViewport(0u, {viewport});
    cmd.setScissor(0u, {scissor});
    cmd.draw(3u, 1u, 0u, 0u, device.dldi);

    cmd.endRenderingKHR(device.dldi); /* End rendering */
}

void FinalPipeline::destroy(const Device& device) {
    /* Destroy the final shader module */
    device.device.destroyShaderModule(final_shader);

    /* Destroy the pipeline & the layout */
    device.device.destroyPipelineLayout(layout);
    device.device.destroyPipeline(pipeline);
}

}  // namespace wyre
