#pragma once
#include <imgui.h>
#include <string>
#include <cstring>
#include "memory/imemory.h"
#include "offsets.h"
#include "settings.h"
#include "overlay.h"
#include "entity_utils.h"

class VoteTeller {
private:
    bool is_voting = false;
    int voting_team = 0;
    int yes_votes = 0;
    int no_votes = 0;
    int active_issue = 0;

public:
    void update(uintptr_t entity_list) {
        if (!g_memory || !g_memory->get_client_base() || !entity_list) {
            reset_vote();
            return;
        }

        uintptr_t vote_controller = find_vote_controller(entity_list);
        if (!vote_controller) {
            reset_vote();
            return;
        }

        active_issue = g_memory->read<int>(vote_controller + 1552);
        voting_team = g_memory->read<int>(vote_controller + 1556);
        yes_votes = g_memory->read<int>(vote_controller + 1560);
        no_votes = g_memory->read<int>(vote_controller + 1564);
        is_voting = active_issue > 0;
    }

    void draw(int screen_w, int screen_h) {
        if (!g_settings.vote_teller_enabled || !g_settings.master_switch || !is_voting) return;

        float window_w = 280.0f;
        float sx = 10.0f;
        // Place it just under the bomb timer / mid-screen left area
        float sy = (float)screen_h / 2.0f + 50.0f;

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
        const char* team_name = "ALL";
        if (voting_team == 2) {
            team_name = "TERRORISTS";
            text_color = {1.0f, 0.24f, 0.24f, 1.0f}; // OrangeRed/Red theme
        } else if (voting_team == 3) {
            team_name = "COUNTER-TERRORISTS";
            text_color = {0.24f, 0.5f, 1.0f, 1.0f}; // DeepSkyBlue/Blue theme
        } else {
            text_color = {1.0f, 1.0f, 1.0f, 1.0f}; // White
        }

        ImFont* font = g_overlay.menu_font;
        if (font) ImGui::PushFont(font);

        ImGui::Begin("##vote_teller_window", nullptr, flags);

        ImGui::TextColored(text_color, "Vote: %s", team_name);
        ImGui::Text("Issue ID: %d", active_issue);
        ImGui::Text("Yes: %d | No: %d", yes_votes, no_votes);

        ImGui::End();

        if (font) ImGui::PopFont();
    }

private:
    uintptr_t find_vote_controller(uintptr_t entity_list) {
        for (int i = 64; i < 8192; i++) {
            uintptr_t page = g_memory->read<uintptr_t>(
                entity_list + 8 * (i >> 9) + 16);
            if (!page) continue;

            uintptr_t entity = g_memory->read<uintptr_t>(
                page + 112 * (i & 0x1FF));
            if (!entity) continue;

            uintptr_t entity_identity = g_memory->read<uintptr_t>(entity + 0x10);
            if (!entity_identity) continue;

            uintptr_t designer_name_ptr = g_memory->read<uintptr_t>(entity_identity + 0x20);
            if (!designer_name_ptr) continue;

            char designer_name[64] = {0};
            if (g_memory->read_raw(designer_name_ptr, designer_name, sizeof(designer_name))) {
                if (strcmp(designer_name, "vote_controller") == 0) {
                    return entity;
                }
            }
        }
        return 0;
    }

    void reset_vote() {
        is_voting = false;
        voting_team = 0;
        yes_votes = 0;
        no_votes = 0;
        active_issue = 0;
    }
};

inline VoteTeller g_vote_teller;
