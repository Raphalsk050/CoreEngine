#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/render/frame_buffer.h"

namespace CoreEngine {
    enum class ShaderStage : std::uint8_t {
        Vertex = 1u << 0u,
        Pixel = 1u << 1u,
        VertexPixel = (1u << 0u) | (1u << 1u),
    };

    enum class ShaderBindingScope : std::uint8_t {
        Global,
        Pass,
        Material,
        Object,
    };

    enum class ShaderBindingType : std::uint8_t {
        UniformBuffer,
        Texture,
    };

    [[nodiscard]] constexpr bool HasShaderStage(ShaderStage value, ShaderStage stage) {
        return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(stage)) != 0u;
    }

    struct ShaderBindingDesc {
        std::string name;
        ShaderBindingType type = ShaderBindingType::UniformBuffer;
        ShaderBindingScope scope = ShaderBindingScope::Pass;
        ShaderStage stages = ShaderStage::Pixel;
        std::uint32_t byte_size = 0;
        std::string sampler_name;

        [[nodiscard]] static ShaderBindingDesc UniformBuffer(std::string name, std::uint32_t byte_size,
                                                             ShaderBindingScope scope = ShaderBindingScope::Pass,
                                                             ShaderStage stages = ShaderStage::Pixel) {
            return {
                    .name = std::move(name),
                    .type = ShaderBindingType::UniformBuffer,
                    .scope = scope,
                    .stages = stages,
                    .byte_size = byte_size,
                    .sampler_name = {},
            };
        }

        [[nodiscard]] static ShaderBindingDesc Texture(std::string name,
                                                       ShaderBindingScope scope = ShaderBindingScope::Pass,
                                                       ShaderStage stages = ShaderStage::Pixel,
                                                       std::string sampler_name = {}) {
            if (sampler_name.empty()) {
                sampler_name = name + "_sampler";
            }

            return {
                    .name = std::move(name),
                    .type = ShaderBindingType::Texture,
                    .scope = scope,
                    .stages = stages,
                    .sampler_name = std::move(sampler_name),
            };
        }

        [[nodiscard]] bool IsValid() const {
            if (name.empty()) {
                return false;
            }

            if (type == ShaderBindingType::UniformBuffer) {
                return byte_size > 0;
            }

            return true;
        }
    };

    struct ShaderUniformData {
        std::string name;
        ShaderStage stages = ShaderStage::Pixel;
        std::vector<std::uint8_t> data;

        [[nodiscard]] bool IsValid() const { return !name.empty() && !data.empty(); }
    };

    struct ShaderProgramDesc {
        std::string debug_name;
        std::string vertex_shader_source;
        std::string pixel_shader_source;
        std::vector<ShaderBindingDesc> bindings;
        bool has_color_target = true;
        FrameBufferFormat color_format = FrameBufferFormat::SwapChainColor;
        FrameBufferFormat depth_format = FrameBufferFormat::SwapChainDepth;
        bool depth_test = false;

        [[nodiscard]] bool IsValid() const {
            if (vertex_shader_source.empty() && pixel_shader_source.empty()) {
                return false;
            }

            if (has_color_target && pixel_shader_source.empty()) {
                return false;
            }

            for (const ShaderBindingDesc &binding: bindings) {
                if (!binding.IsValid()) {
                    return false;
                }
            }

            for (std::size_t i = 0; i < bindings.size(); ++i) {
                for (std::size_t j = i + 1; j < bindings.size(); ++j) {
                    if (bindings[i].name == bindings[j].name ||
                        (!bindings[i].sampler_name.empty() && bindings[i].sampler_name == bindings[j].name) ||
                        (!bindings[j].sampler_name.empty() && bindings[j].sampler_name == bindings[i].name) ||
                        (!bindings[i].sampler_name.empty() && !bindings[j].sampler_name.empty() &&
                         bindings[i].sampler_name == bindings[j].sampler_name)) {
                        return false;
                    }
                }
            }

            return true;
        }
    };

    [[nodiscard]] inline ShaderUniformData MakeShaderUniformData(std::string name, ShaderStage stages,
                                                                 std::span<const std::uint8_t> data) {
        ShaderUniformData uniform;
        uniform.name = std::move(name);
        uniform.stages = stages;
        uniform.data.assign(data.begin(), data.end());
        return uniform;
    }
} // namespace CoreEngine
