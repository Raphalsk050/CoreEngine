#include "core/online/steam/steam_multiplayer_debug_panel.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "imgui.h"
#include "imgui_internal.h"

#include "core/online/i_online_system.h"

namespace CoreEngine {
    namespace {
        constexpr double kNetworkGraphSampleIntervalSeconds = 0.25;

        template<typename T>
        [[nodiscard]] float CounterRate(T current, T previous, double elapsed_seconds) noexcept {
            if (elapsed_seconds <= 0.0 || current < previous) {
                return 0.0f;
            }

            return static_cast<float>(
                static_cast<double>(current - previous) / elapsed_seconds);
        }

        [[nodiscard]] float MetricValue(int value) noexcept {
            return value >= 0 ? static_cast<float>(value) : -1.0f;
        }
    }

    SteamMultiplayerDebugPanel::SteamMultiplayerDebugPanel() = default;

    SteamMultiplayerDebugPanel::~SteamMultiplayerDebugPanel() = default;

    void SteamMultiplayerDebugPanel::Render(IOnlineSystem &online_system) {
        if (ImGui::GetCurrentContext() == nullptr) {
            return;
        }

        if (ImGui::Begin("Steam Multiplayer Debug")) {
            const OnlineStatus &status = online_system.Status();
            const NetworkStats &stats = status.network_stats;

            RenderSteamProfile(online_system);
            ImGui::Separator();

            ImGui::Text("Steam initialized: %s", status.steam_available ? "yes" : "no");
            ImGui::Text("Steam overlay: %s", status.steam_overlay_enabled ? "enabled" : "not ready");
            ImGui::Text("Overlay needs present: %s", status.steam_overlay_needs_present ? "yes" : "no");
            ImGui::Text("Lobby ID: %llu", static_cast<unsigned long long>(status.lobby_id));
            ImGui::Text("Lobby owner: %llu", static_cast<unsigned long long>(status.lobby_owner_user_id));
            ImGui::Text("Role: %s", ToString(status.role));
            ImGui::Text("Session route: %s", ToString(status.session_kind));
            ImGui::Text("P2P/Auth state: %s", ToString(status.session_state));
            ImGui::Text("Disconnect reason: %s", ToString(status.last_disconnect_reason));
            ImGui::Text("Ping: %d ms", stats.ping_ms);
            ImGui::Text("Jitter: %d ms", stats.jitter_ms);
            ImGui::Text("Engine RTT/Jitter: %d ms / %d ms", stats.protocol_ping_ms, stats.protocol_jitter_ms);
            ImGui::Text("Transport queue: %d ms, pending %u / %u bytes",
                        stats.transport_queue_time_ms,
                        stats.transport_pending_unreliable_bytes,
                        stats.transport_pending_reliable_bytes);
            ImGui::Text("Transport send rate: %u B/s", stats.transport_send_rate_bytes_per_second);
            ImGui::Text("Packet loss: %.2f", stats.packet_loss);
            ImGui::Text("Bytes in/out: %llu / %llu",
                        static_cast<unsigned long long>(stats.bytes_in),
                        static_cast<unsigned long long>(stats.bytes_out));
            ImGui::Text("Packets in/out/drop/send fail: %llu / %llu / %llu / %llu",
                        static_cast<unsigned long long>(stats.packets_in),
                        static_cast<unsigned long long>(stats.packets_out),
                        static_cast<unsigned long long>(stats.packets_dropped),
                        static_cast<unsigned long long>(stats.packets_send_failed));
            ImGui::Text("Input recv/drop/dup: %llu / %llu / %llu",
                        static_cast<unsigned long long>(stats.input_commands_received),
                        static_cast<unsigned long long>(stats.input_commands_dropped),
                        static_cast<unsigned long long>(stats.input_commands_duplicated));
            ImGui::Text("Snapshots sent/recv/drop: %llu / %llu / %llu",
                        static_cast<unsigned long long>(stats.snapshots_sent),
                        static_cast<unsigned long long>(stats.snapshots_received),
                        static_cast<unsigned long long>(stats.snapshots_dropped));
            ImGui::Text("Prediction corrections/snaps: %llu / %llu",
                        static_cast<unsigned long long>(stats.prediction_corrections),
                        static_cast<unsigned long long>(stats.prediction_hard_snaps));
            ImGui::Text("Avg snapshot size: %u bytes", stats.avg_snapshot_size_bytes);
            ImGui::Text("Snapshot tick: %u", stats.last_snapshot_tick);
            ImGui::Text("Last input tick: %u", stats.last_input_tick);

            RenderNetworkTelemetry(status);

            if (ImGui::CollapsingHeader("Connection route diagnostics", ImGuiTreeNodeFlags_DefaultOpen)) {
                const std::string diagnostics = online_system.ConnectionDiagnosticsText();
                if (diagnostics.empty()) {
                    ImGui::TextDisabled("No connected Steam P2P route yet.");
                } else {
                    ImGui::BeginChild("ConnectionRouteDiagnostics", ImVec2{0.0f, 180.0f}, true);
                    ImGui::TextUnformatted(diagnostics.c_str());
                    ImGui::EndChild();
                }
            }

            if (ImGui::Button("Create Friends Lobby")) {
                online_system.CreateFriendsLobby(4);
            }

            ImGui::SameLine();
            const bool overlay_enabled = status.steam_overlay_enabled;
            if (!overlay_enabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open Steam Overlay")) {
                online_system.OpenSteamOverlay("Friends");
            }
            if (!overlay_enabled) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (!overlay_enabled) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Open Invite Overlay")) {
                online_system.OpenInviteOverlay();
            }
            if (!overlay_enabled) {
                ImGui::EndDisabled();
            }

            ImGui::InputText("Join Lobby ID", lobby_id_buffer_, sizeof(lobby_id_buffer_));
            ImGui::SameLine();
            if (ImGui::Button("Join")) {
                const auto lobby_id = static_cast<std::uint64_t>(std::strtoull(lobby_id_buffer_, nullptr, 10));
                online_system.JoinLobbyById(lobby_id);
            }

            if (ImGui::CollapsingHeader("Direct LAN", ImGuiTreeNodeFlags_DefaultOpen)) {
                const double now_seconds = ImGui::GetTime();
                if (now_seconds >= next_direct_local_ip_refresh_time_) {
                    direct_local_ip_text_ = online_system.LocalNetworkAddressText();
                    next_direct_local_ip_refresh_time_ = now_seconds + 1.0;
                }
                ImGui::Text("Local IP: %s", direct_local_ip_text_.c_str());

                ImGui::InputInt("Direct Port", &direct_port_);
                direct_port_ = std::clamp(direct_port_, 1, 65535);

                ImGui::InputInt("Direct Max Players", &direct_max_players_);
                direct_max_players_ = std::clamp(direct_max_players_, 1, 32);

                ImGui::InputText("Direct Host", direct_host_buffer_, sizeof(direct_host_buffer_));

                if (ImGui::Button("Create Local Match")) {
                    online_system.CreateDirectHost(static_cast<std::uint16_t>(direct_port_), direct_max_players_);
                }

                ImGui::SameLine();
                if (ImGui::Button("Direct Connect")) {
                    online_system.JoinDirect(direct_host_buffer_, static_cast<std::uint16_t>(direct_port_));
                }
            }

            if (ImGui::Button("Leave Lobby")) {
                online_system.LeaveLobby();
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump Connection Status")) {
                online_system.DumpConnectionStatus();
            }

            if (ImGui::CollapsingHeader("Lobby members")) {
                for (const OnlineLobbyMember &member: online_system.LobbyMembers()) {
                    ImGui::BulletText("%llu %s",
                                      static_cast<unsigned long long>(member.user_id),
                                      member.display_name.c_str());
                }
            }
        }
        ImGui::End();
    }

    void SteamMultiplayerDebugPanel::RenderNetworkTelemetry(const OnlineStatus &status) {
        UpdateNetworkGraph(status.network_stats);

        if (!ImGui::CollapsingHeader("Realtime network graphs", ImGuiTreeNodeFlags_DefaultOpen)) {
            return;
        }

        const NetworkStats &stats = status.network_stats;
        const NetworkGraphSample latest =
            network_graph_sample_count_ > 0 ? SampleAt(network_graph_sample_count_ - 1u) : NetworkGraphSample{};
        const int engine_delay_ms =
            stats.ping_ms >= 0 && stats.protocol_ping_ms >= 0
                ? stats.protocol_ping_ms - stats.ping_ms
                : -1;

        if (status.session_state != NetworkSessionState::Connected) {
            ImGui::TextDisabled("Waiting for a connected peer before collecting useful network samples.");
        } else if (stats.transport_queue_time_ms > 8 ||
                   stats.transport_pending_unreliable_bytes + stats.transport_pending_reliable_bytes > 0u) {
            ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.25f, 1.0f},
                               "Bottleneck candidate: Steam send queue. Queue time is %d ms with %u pending bytes.",
                               stats.transport_queue_time_ms,
                               stats.transport_pending_unreliable_bytes + stats.transport_pending_reliable_bytes);
        } else if (stats.ping_ms > 30) {
            ImGui::TextColored(ImVec4{1.0f, 0.55f, 0.20f, 1.0f},
                               "Bottleneck candidate: transport route or LAN path. Steam ping is %d ms.",
                               stats.ping_ms);
        } else if (engine_delay_ms > 16) {
            ImGui::TextColored(ImVec4{1.0f, 0.70f, 0.20f, 1.0f},
                               "Bottleneck candidate: engine frame/tick path. Engine RTT is %d ms above Steam ping.",
                               engine_delay_ms);
        } else if (latest.packets_dropped_per_second > 0.0f || latest.snapshots_dropped_per_second > 0.0f) {
            ImGui::TextColored(ImVec4{1.0f, 0.35f, 0.25f, 1.0f},
                               "Bottleneck candidate: packet parse/drop path. Drops are visible in the last sample.");
        } else {
            ImGui::TextColored(ImVec4{0.35f, 0.90f, 0.50f, 1.0f},
                               "No obvious network bottleneck in the current sample window.");
        }

        ImGui::Text("Steam ping: %d ms | Engine RTT: %d ms | Delta: %d ms",
                    stats.ping_ms,
                    stats.protocol_ping_ms,
                    engine_delay_ms);
        ImGui::Text("Rates: %.0f B/s in, %.0f B/s out, %.1f packets/s in, %.1f packets/s out",
                    latest.bytes_in_per_second,
                    latest.bytes_out_per_second,
                    latest.packets_in_per_second,
                    latest.packets_out_per_second);
        ImGui::Text("Steam queue: %d ms, pending %u bytes, send rate cap %u B/s",
                    stats.transport_queue_time_ms,
                    stats.transport_pending_unreliable_bytes + stats.transport_pending_reliable_bytes,
                    stats.transport_send_rate_bytes_per_second);

        RenderNetworkPlot("Steam transport ping (ms)", &NetworkGraphSample::transport_ping_ms, 0.0f, 120.0f);
        RenderNetworkPlot("Engine protocol RTT (ms)", &NetworkGraphSample::protocol_ping_ms, 0.0f, 120.0f);
        RenderNetworkPlot("Steam jitter (ms)", &NetworkGraphSample::jitter_ms, 0.0f, 40.0f);
        RenderNetworkPlot("Engine protocol jitter (ms)", &NetworkGraphSample::protocol_jitter_ms, 0.0f, 40.0f);
        RenderNetworkPlot("Steam queue time (ms)", &NetworkGraphSample::transport_queue_time_ms, 0.0f, 40.0f);
        RenderNetworkPlot("Steam pending bytes", &NetworkGraphSample::transport_pending_bytes, 0.0f, 0.0f);
        RenderNetworkPlot("Packet loss (%)", &NetworkGraphSample::packet_loss_percent, 0.0f, 10.0f);
        RenderNetworkPlot("Bytes in per second", &NetworkGraphSample::bytes_in_per_second, 0.0f, 0.0f);
        RenderNetworkPlot("Bytes out per second", &NetworkGraphSample::bytes_out_per_second, 0.0f, 0.0f);
        RenderNetworkPlot("Packets dropped per second", &NetworkGraphSample::packets_dropped_per_second, 0.0f, 10.0f);
        RenderNetworkPlot("Snapshots received per second", &NetworkGraphSample::snapshots_received_per_second, 0.0f, 80.0f);
        RenderNetworkPlot("Snapshots sent per second", &NetworkGraphSample::snapshots_sent_per_second, 0.0f, 80.0f);
        RenderNetworkPlot("Input commands received per second", &NetworkGraphSample::input_received_per_second, 0.0f, 160.0f);
        RenderNetworkPlot("Input commands dropped per second", &NetworkGraphSample::input_dropped_per_second, 0.0f, 20.0f);
        RenderNetworkPlot("Input commands duplicated per second", &NetworkGraphSample::input_duplicated_per_second, 0.0f, 20.0f);
    }

    void SteamMultiplayerDebugPanel::UpdateNetworkGraph(const NetworkStats &stats) {
        const double now = ImGui::GetTime();
        if (last_network_graph_sample_time_ < 0.0) {
            last_network_graph_sample_time_ = now;
            previous_network_stats_ = stats;
            return;
        }

        const double elapsed_seconds = now - last_network_graph_sample_time_;
        if (elapsed_seconds < kNetworkGraphSampleIntervalSeconds) {
            return;
        }

        NetworkGraphSample sample{
            .transport_ping_ms = MetricValue(stats.ping_ms),
            .protocol_ping_ms = MetricValue(stats.protocol_ping_ms),
            .jitter_ms = MetricValue(stats.jitter_ms),
            .protocol_jitter_ms = MetricValue(stats.protocol_jitter_ms),
            .transport_queue_time_ms = static_cast<float>(stats.transport_queue_time_ms),
            .transport_pending_bytes =
                static_cast<float>(stats.transport_pending_unreliable_bytes + stats.transport_pending_reliable_bytes),
            .packet_loss_percent = stats.packet_loss * 100.0f,
            .bytes_in_per_second = CounterRate(stats.bytes_in, previous_network_stats_.bytes_in, elapsed_seconds),
            .bytes_out_per_second = CounterRate(stats.bytes_out, previous_network_stats_.bytes_out, elapsed_seconds),
            .packets_in_per_second = CounterRate(stats.packets_in, previous_network_stats_.packets_in, elapsed_seconds),
            .packets_out_per_second = CounterRate(stats.packets_out, previous_network_stats_.packets_out, elapsed_seconds),
            .packets_dropped_per_second =
                CounterRate(stats.packets_dropped, previous_network_stats_.packets_dropped, elapsed_seconds),
            .snapshots_sent_per_second =
                CounterRate(stats.snapshots_sent, previous_network_stats_.snapshots_sent, elapsed_seconds),
            .snapshots_received_per_second =
                CounterRate(stats.snapshots_received, previous_network_stats_.snapshots_received, elapsed_seconds),
            .snapshots_dropped_per_second =
                CounterRate(stats.snapshots_dropped, previous_network_stats_.snapshots_dropped, elapsed_seconds),
            .input_received_per_second =
                CounterRate(stats.input_commands_received,
                            previous_network_stats_.input_commands_received,
                            elapsed_seconds),
            .input_dropped_per_second =
                CounterRate(stats.input_commands_dropped,
                            previous_network_stats_.input_commands_dropped,
                            elapsed_seconds),
            .input_duplicated_per_second =
                CounterRate(stats.input_commands_duplicated,
                            previous_network_stats_.input_commands_duplicated,
                            elapsed_seconds),
        };

        network_graph_samples_[network_graph_sample_cursor_] = sample;
        network_graph_sample_cursor_ = (network_graph_sample_cursor_ + 1u) % network_graph_samples_.size();
        network_graph_sample_count_ = std::min(network_graph_sample_count_ + 1u, network_graph_samples_.size());
        previous_network_stats_ = stats;
        last_network_graph_sample_time_ = now;
    }

    void SteamMultiplayerDebugPanel::RenderNetworkPlot(const char *label,
                                                       float NetworkGraphSample::*field,
                                                       float scale_min,
                                                       float scale_max) {
        if (network_graph_sample_count_ == 0) {
            ImGui::TextDisabled("%s: waiting for samples", label);
            return;
        }

        float latest = 0.0f;
        float max_value = 0.0f;
        float total = 0.0f;
        std::size_t valid_count = 0;
        for (std::size_t i = 0; i < network_graph_sample_count_; ++i) {
            const float raw_value = SampleAt(i).*field;
            const float value = raw_value >= 0.0f ? raw_value : 0.0f;
            network_plot_scratch_[i] = value;
            latest = raw_value;
            max_value = std::max(max_value, value);
            total += value;
            ++valid_count;
        }

        if (scale_max <= scale_min) {
            scale_max = std::max(1.0f, max_value * 1.25f);
        }

        const float average = valid_count > 0 ? total / static_cast<float>(valid_count) : 0.0f;
        char overlay[96]{};
        std::snprintf(overlay,
                      sizeof(overlay),
                      "now %.1f | avg %.1f | max %.1f",
                      latest,
                      average,
                      max_value);
        ImGui::PlotLines(label,
                         network_plot_scratch_.data(),
                         static_cast<int>(network_graph_sample_count_),
                         0,
                         overlay,
                         scale_min,
                         scale_max,
                         ImVec2{0.0f, 72.0f});
    }

    const SteamMultiplayerDebugPanel::NetworkGraphSample &SteamMultiplayerDebugPanel::SampleAt(
        std::size_t index) const noexcept {
        const std::size_t oldest =
            (network_graph_sample_cursor_ + network_graph_samples_.size() - network_graph_sample_count_) %
            network_graph_samples_.size();
        return network_graph_samples_[(oldest + index) % network_graph_samples_.size()];
    }

    void SteamMultiplayerDebugPanel::RenderSteamProfile(IOnlineSystem &online_system) {
        UpdateSteamAvatarTexture(online_system);
        const OnlineStatus &status = online_system.Status();

        if (steam_avatar_texture_ != nullptr && steam_avatar_texture_->Status != ImTextureStatus_Destroyed) {
            ImGui::Image(steam_avatar_texture_->GetTexRef(), ImVec2{64.0f, 64.0f});
            ImGui::SameLine();
        }

        ImGui::BeginGroup();
        ImGui::Text("Persona name: %s", status.local_display_name.c_str());
        ImGui::Text("Steam ID local: %llu", static_cast<unsigned long long>(status.local_user_id));
        if (status.steam_available && steam_avatar_texture_ == nullptr) {
            ImGui::TextUnformatted("Profile image: loading");
        }
        ImGui::EndGroup();
    }

    void SteamMultiplayerDebugPanel::UpdateSteamAvatarTexture(IOnlineSystem &online_system) {
        if (steam_avatar_texture_ != nullptr || !online_system.Status().steam_available) {
            return;
        }

        OnlineAvatarImage avatar = online_system.LoadLocalAvatarImage();
        constexpr auto max_texture_axis = static_cast<std::uint32_t>(std::numeric_limits<unsigned short>::max());
        if (!avatar.IsValid() || avatar.width > max_texture_axis || avatar.height > max_texture_axis) {
            return;
        }

        auto texture = std::make_unique<ImTextureData>();
        texture->UniqueID = 0x53544541; // "STEA" for the Steam profile avatar user texture.
        texture->RefCount = 1;
        texture->Create(ImTextureFormat_RGBA32, static_cast<int>(avatar.width), static_cast<int>(avatar.height));
        std::memcpy(texture->GetPixels(), avatar.rgba.data(), avatar.rgba.size());
        texture->UseColors = true;
        texture->UsedRect = ImTextureRect{
            0,
            0,
            static_cast<unsigned short>(avatar.width),
            static_cast<unsigned short>(avatar.height),
        };
        texture->SetStatus(ImTextureStatus_WantCreate);

        ImGui::RegisterUserTexture(texture.get());
        steam_avatar_texture_ = std::move(texture);
    }
} // namespace CoreEngine
