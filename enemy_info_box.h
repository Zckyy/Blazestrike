#pragma once
#include <imgui.h>
#include <string>
#include <algorithm>
#include "types.h"
#include "settings.h"
#include "overlay.h"
#include "entity_reader.h"

class EnemyInfoBox {
public:
    void draw(const FrameState& state, int screen_w, int screen_h) {
        if (!g_settings.enemy_info_box_enabled || !g_settings.master_switch) return;

        float window_w = 320.0f;
        float sx = 10.0f;
        // Positioned slightly below the bomb timer box (which is at screen_h / 2.0f - 100.0f)
        float sy = (float)screen_h / 2.0f + 60.0f;

        ImGui::SetNextWindowPos({sx, sy}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({window_w, 0}, {window_w, 1000});
        ImGui::SetNextWindowBgAlpha(0.6f);

        int flags = ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar;

        ImFont* font = g_overlay.menu_font;
        if (font) ImGui::PushFont(font);

        ImGui::Begin("##enemy_info_box_window", nullptr, flags);

        ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Enemy Player Info");
        ImGui::Separator();

        bool has_enemies = false;
        int local_team = state.local.team;

        for (int i = 1; i < 64; i++) {
            const auto& p = state.players[i];
            if (!p.valid) continue;
            if (p.team == local_team) continue;

            has_enemies = true;

            std::string name_str = p.name;
            if (name_str.length() > 14) {
                name_str = name_str.substr(0, 12) + "..";
            }

            std::string wep_str = p.weapon;
            if (wep_str.empty()) {
                wep_str = "None";
            } else if (wep_str.length() > 14) {
                wep_str = wep_str.substr(0, 12) + "..";
            }

            ImGui::Text("%s", name_str.c_str());
            ImGui::SameLine(130.0f);
            ImGui::TextColored({0.7f, 0.7f, 0.7f, 1.0f}, "%s", wep_str.c_str());
            ImGui::SameLine(250.0f);
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1.0f}, "$%d", p.money);
        }

        if (!has_enemies) {
            ImGui::TextColored({0.6f, 0.6f, 0.6f, 1.0f}, "No enemy players detected.");
        }

        ImGui::End();

        if (font) ImGui::PopFont();
    }
};

inline EnemyInfoBox g_enemy_info_box;
