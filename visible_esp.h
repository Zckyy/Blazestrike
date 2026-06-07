#pragma once
#include <imgui.h>
#include "types.h"
#include "settings.h"
#include "chams_renderer.h"
#include "box_renderer.h"
#include "overlay.h"
#include "utils.h"

class VisibleESP {
public:
    BoxRenderer box_renderer;

    void draw_player(ImDrawList* draw, const PlayerVisuals& p, int local_team,
                         int sw, int sh, int idx, bool is_scoped) {
        if (!p.valid || !g_settings.esp_enabled || !g_settings.master_switch) return;
        bool enemy = (p.team != local_team);
        if (!enemy && !g_settings.draw_teammates) return;

        // Compute average depth from center bones for opacity
        float avg_depth = 0.0f;
        int   depth_cnt = 0;
        static constexpr int CENTER_BONES[] = {
            BONE_HEAD, BONE_NECK, BONE_SPINE1, BONE_SPINE2, BONE_PELVIS
        };
        for (int b : CENTER_BONES) {
            if (p.visible[b]) { avg_depth += p.depths[b]; depth_cnt++; }
        }
        float opacity = 1.0f;
        if (depth_cnt > 0) opacity = esp_depth_opacity(avg_depth / depth_cnt);

        if (g_settings.esp_use_theme) {
            EspTheme::apply(enemy);
            g_settings.healthbar_solid_color = true;
        }

        ColorSet c = enemy ? get_enemy_colors(opacity) : get_team_colors(opacity);
        ChamsStyle style = static_cast<ChamsStyle>(g_settings.chams_style);

        float depth_scale = g_settings.depth_scale * (is_scoped ? 2.0f : 1.0f);

        ChamsRenderer::draw_chams(draw, p, c, style, depth_scale);

        if (g_settings.draw_snaplines) {
            ImVec2 origin;
            origin.x = sw * 0.5f;
            switch (g_settings.snapline_origin) {
                case 0:  origin.y = (float)sh; break;       // Bottom
                case 1:  origin.y = sh * 0.5f; break;        // Center
                case 2:  origin.y = 0.0f; break;             // Top
                default: origin.y = (float)sh; break;
            }

            ImVec2 target = p.screens[BONE_PELVIS];
            if (p.visible[BONE_LFOOT] && p.visible[BONE_RFOOT]) {
                target.x = (p.screens[BONE_LFOOT].x + p.screens[BONE_RFOOT].x) * 0.5f;
                target.y = (p.screens[BONE_LFOOT].y + p.screens[BONE_RFOOT].y) * 0.5f;
            }

            if (p.visible[BONE_PELVIS]) {
                ImU32 col = apply_opacity(float4_to_col(g_settings.snapline_color), opacity);
                draw->AddLine(origin, target, col, 1.2f);
            }
        }

        if (g_settings.draw_box || g_settings.draw_healthbar ||
            g_settings.draw_health_text || g_settings.draw_name ||
            g_settings.draw_weapon) {
            box_renderer.draw_box_hp_name(draw, p, c, idx, is_scoped,
                                          g_overlay.esp_font,
                                          g_settings.esp_font_atlas_size,
                                          opacity);
            }
    }
};

inline VisibleESP g_esp;