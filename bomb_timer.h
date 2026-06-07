#pragma once
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cmath>
#include "memory/imemory.h"
#include "offsets.h"
#include "settings.h"
#include "overlay.h"

class BombTimer {
private:
    std::string bomb_planted_text;
    std::string time_left_text;
    std::string defuse_left_text;
    std::string defused_text;
    
    bool is_bomb_planted = false;
    float time_left = 0.0f;
    float defuse_left = 0.0f;
    bool being_defused = false;
    bool bomb_defused = false;
    std::string bomb_site;
    
    float current_time = 0.0f;
    float defuse_countdown = 0.0f;
    float c4_blow = 0.0f;
    
    // Timer to keep "BOMB DEFUSED!" on screen for a few seconds
    std::chrono::steady_clock::time_point defused_show_start;
    bool showing_defused = false;

public:
    void update() {
        if (!g_memory || !g_memory->get_client_base()) {
            reset_bomb_state();
            return;
        }

        uintptr_t client_base = g_memory->get_client_base();
        uintptr_t global_vars = g_memory->read<uintptr_t>(client_base + g_offsets.client.dwGlobalVars);
        if (!global_vars) {
            reset_bomb_state();
            return;
        }

        current_time = g_memory->read<float>(global_vars + 0x30);

        uintptr_t temp_c4 = g_memory->read<uintptr_t>(client_base + g_offsets.client.dwPlantedC4);
        if (!temp_c4) {
            reset_bomb_state();
            return;
        }

        uintptr_t planted_c4 = g_memory->read<uintptr_t>(temp_c4);
        
        // Read the isBombPlanted bool from dwPlantedC4 - 0x8
        is_bomb_planted = g_memory->read<bool>(client_base + g_offsets.client.dwPlantedC4 - 0x8);

        if (!is_bomb_planted || !planted_c4) {
            reset_bomb_state();
            return;
        }

        bomb_defused = g_memory->read<bool>(planted_c4 + g_offsets.C_PlantedC4.m_bBombDefused);
        if (bomb_defused) {
            if (!showing_defused) {
                showing_defused = true;
                defused_show_start = std::chrono::steady_clock::now();
            }
            reset_bomb_state();
            bomb_defused = true;
            defused_text = "BOMB DEFUSED!";
            return;
        }

        defuse_countdown = g_memory->read<float>(planted_c4 + g_offsets.C_PlantedC4.m_flDefuseCountDown);
        c4_blow = g_memory->read<float>(planted_c4 + g_offsets.C_PlantedC4.m_flC4Blow);
        being_defused = g_memory->read<bool>(planted_c4 + g_offsets.C_PlantedC4.m_bBeingDefused);

        time_left = c4_blow - current_time;
        defuse_left = defuse_countdown - current_time;

        time_left = (std::max)(time_left, 0.0f);
        defuse_left = (std::max)(defuse_left, 0.0f);

        if (!being_defused) {
            defuse_left = 0.0f;
        }

        int site_val = g_memory->read<int>(planted_c4 + g_offsets.C_PlantedC4.m_nBombSite);
        bomb_site = (site_val == 1) ? "B" : "A";
        
        char plant_buf[128];
        snprintf(plant_buf, sizeof(plant_buf), "Bomb planted on site: %s", bomb_site.c_str());
        bomb_planted_text = plant_buf;

        char time_buf[128];
        snprintf(time_buf, sizeof(time_buf), "Time left: %.2f seconds", time_left);
        time_left_text = time_buf;

        if (being_defused) {
            char defuse_buf[128];
            snprintf(defuse_buf, sizeof(defuse_buf), "Defuse time: %.2f seconds", defuse_left);
            defuse_left_text = defuse_buf;
        } else {
            defuse_left_text.clear();
        }
        
        defused_text.clear();
        showing_defused = false;
    }

    void draw(int screen_w, int screen_h) {
        if (!g_settings.bomb_timer_enabled || !g_settings.master_switch) return;

        // Handle temporary "BOMB DEFUSED!" message timing
        if (showing_defused) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - defused_show_start).count();
            if (elapsed > 4) {
                showing_defused = false;
                defused_text.clear();
            }
        }

        if (!is_bomb_planted && defused_text.empty()) return;

        float window_w = 280.0f;
        float sx = 10.0f;
        float sy = (float)screen_h / 2.0f - 100.0f;

        ImGui::SetNextWindowPos({sx, sy}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({window_w, 0}, {window_w, 1000});
        ImGui::SetNextWindowBgAlpha(0.6f);

        int flags = ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoTitleBar;

        ImVec4 text_color;
        if (!defused_text.empty()) {
            text_color = {0.0f, 1.0f, 0.0f, 1.0f}; // LimeGreen
        } else if (time_left < 5.0f) {
            text_color = {1.0f, 0.0f, 0.0f, 1.0f}; // Red
        } else if (time_left < 15.0f) {
            text_color = {1.0f, 1.0f, 0.0f, 1.0f}; // Yellow
        } else {
            text_color = {1.0f, 1.0f, 1.0f, 1.0f}; // White
        }

        ImFont* font = g_overlay.menu_font;
        if (font) ImGui::PushFont(font);

        ImGui::Begin("##bomb_timer_window", nullptr, flags);

        if (is_bomb_planted) {
            ImGui::TextColored(text_color, "%s", bomb_planted_text.c_str());
            ImGui::TextColored(text_color, "%s", time_left_text.c_str());

            if (being_defused) {
                // If defuse time is less than time left, defuser wins, show green defuse text, else red
                ImVec4 defuse_color = (defuse_left < time_left) ? ImVec4{0.0f, 1.0f, 0.0f, 1.0f} : ImVec4{1.0f, 0.0f, 0.0f, 1.0f};
                ImGui::TextColored(defuse_color, "%s", defuse_left_text.c_str());
            }
        } else if (!defused_text.empty()) {
            ImGui::TextColored({0.0f, 1.0f, 0.0f, 1.0f}, "%s", defused_text.c_str());
        }

        ImGui::End();

        // Draw large top-center screen warning if defusing
        if (is_bomb_planted && being_defused) {
            float warning_fs = 26.0f; // Large, highly visible font
            char warning_text[256];
            ImVec4 warning_color;

            if (defuse_left < time_left) {
                snprintf(warning_text, sizeof(warning_text), "WARNING: ENEMY IS DEFUSING (THEY WILL DEFUSE IN TIME! - %.2fs)", defuse_left);
                warning_color = {1.0f, 0.1f, 0.1f, 1.0f}; // Red warning
            } else {
                snprintf(warning_text, sizeof(warning_text), "WARNING: ENEMY IS DEFUSING (TOO LATE! - %.2fs)", defuse_left);
                warning_color = {0.1f, 1.0f, 0.1f, 1.0f}; // Green warning
            }

            ImFont* draw_font = font ? font : ImGui::GetFont();
            ImVec2 text_size = draw_font->CalcTextSizeA(warning_fs, FLT_MAX, 0.0f, warning_text);

            float wx = ((float)screen_w - text_size.x) * 0.5f;
            float wy = 60.0f; // Top center

            ImDrawList* d = ImGui::GetBackgroundDrawList();
            ImU32 col = ImGui::ColorConvertFloat4ToU32(warning_color);
            ImU32 shadow_col = IM_COL32(0, 0, 0, 240);

            // Draw shadow/outline for readability against any background
            d->AddText(draw_font, warning_fs, {wx - 2, wy}, shadow_col, warning_text);
            d->AddText(draw_font, warning_fs, {wx + 2, wy}, shadow_col, warning_text);
            d->AddText(draw_font, warning_fs, {wx, wy - 2}, shadow_col, warning_text);
            d->AddText(draw_font, warning_fs, {wx, wy + 2}, shadow_col, warning_text);
            
            d->AddText(draw_font, warning_fs, {wx, wy}, col, warning_text);
        }

        if (font) ImGui::PopFont();
    }

private:
    void reset_bomb_state() {
        is_bomb_planted = false;
        bomb_planted_text.clear();
        time_left_text.clear();
        defuse_left_text.clear();
        time_left = 0.0f;
        defuse_left = 0.0f;
        being_defused = false;
        bomb_site.clear();
    }
};

inline BombTimer g_bomb_timer;
