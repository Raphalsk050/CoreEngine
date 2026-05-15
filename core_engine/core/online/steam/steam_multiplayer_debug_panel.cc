#include "core/online/steam/steam_multiplayer_debug_panel.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#include "imgui.h"
#include "imgui_internal.h"

#include "core/online/i_online_system.h"

namespace CoreEngine {
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
            ImGui::Text("P2P/Auth state: %s", ToString(status.session_state));
            ImGui::Text("Disconnect reason: %s", ToString(status.last_disconnect_reason));
            ImGui::Text("Ping: %d ms", stats.ping_ms);
            ImGui::Text("Jitter: %d ms", stats.jitter_ms);
            ImGui::Text("Packet loss: %.2f", stats.packet_loss);
            ImGui::Text("Bytes in/out: %llu / %llu",
                        static_cast<unsigned long long>(stats.bytes_in),
                        static_cast<unsigned long long>(stats.bytes_out));
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
