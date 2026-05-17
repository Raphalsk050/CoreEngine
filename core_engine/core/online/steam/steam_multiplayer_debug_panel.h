#pragma once

#include <array>
#include <cstddef>
#include <memory>

#include "core/network/network_stats.h"

struct ImTextureData;

namespace CoreEngine {
    class IOnlineSystem;
    struct OnlineStatus;

    /**
     * @brief Renders developer controls for Steam multiplayer sessions.
     *
     * Responsibility: keep Steam profile, lobby, transport, and auth diagnostics
     * out of gameplay app code while reusing the engine online module.
     */
    class SteamMultiplayerDebugPanel {
    public:
        SteamMultiplayerDebugPanel() = default;

        ~SteamMultiplayerDebugPanel();

        SteamMultiplayerDebugPanel(const SteamMultiplayerDebugPanel &) = delete;

        SteamMultiplayerDebugPanel &operator=(const SteamMultiplayerDebugPanel &) = delete;

        void Render(IOnlineSystem &online_system);

    private:
        struct NetworkGraphSample {
            float transport_ping_ms = -1.0f;
            float protocol_ping_ms = -1.0f;
            float jitter_ms = 0.0f;
            float protocol_jitter_ms = 0.0f;
            float transport_queue_time_ms = 0.0f;
            float transport_pending_bytes = 0.0f;
            float packet_loss_percent = 0.0f;
            float bytes_in_per_second = 0.0f;
            float bytes_out_per_second = 0.0f;
            float packets_in_per_second = 0.0f;
            float packets_out_per_second = 0.0f;
            float packets_dropped_per_second = 0.0f;
            float snapshots_sent_per_second = 0.0f;
            float snapshots_received_per_second = 0.0f;
            float snapshots_dropped_per_second = 0.0f;
            float input_received_per_second = 0.0f;
            float input_dropped_per_second = 0.0f;
            float input_duplicated_per_second = 0.0f;
        };

        void RenderNetworkTelemetry(const OnlineStatus &status);

        void UpdateNetworkGraph(const NetworkStats &stats);

        void RenderNetworkPlot(const char *label,
                               float NetworkGraphSample::*field,
                               float scale_min,
                               float scale_max);

        [[nodiscard]] const NetworkGraphSample &SampleAt(std::size_t index) const noexcept;

        void RenderSteamProfile(IOnlineSystem &online_system);

        void UpdateSteamAvatarTexture(IOnlineSystem &online_system);

        static constexpr std::size_t kNetworkGraphSampleCount = 240;
        char lobby_id_buffer_[32]{};
        std::array<NetworkGraphSample, kNetworkGraphSampleCount> network_graph_samples_{};
        std::array<float, kNetworkGraphSampleCount> network_plot_scratch_{};
        NetworkStats previous_network_stats_{};
        double last_network_graph_sample_time_ = -1.0;
        std::size_t network_graph_sample_cursor_ = 0;
        std::size_t network_graph_sample_count_ = 0;
        std::unique_ptr<ImTextureData> steam_avatar_texture_;
    };
} // namespace CoreEngine
