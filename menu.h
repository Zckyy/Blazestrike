#pragma once
#include <imgui.h>
#include "settings.h"
#include "types.h"
#include "utils.h"
#include "crosshair.h"
#include "grenades.h"
#include "overlay.h"
#include "weapon_icons.h"
#include "gif_loader.h"

class Menu {
public:
    void toggle() { g_settings.menu_open = !g_settings.menu_open; }

    void render() {
        if (!g_settings.menu_open) return;

        if (g_settings.menu_tab < 0 || g_settings.menu_tab >= NAV_COUNT)
            g_settings.menu_tab = 0;

        if (g_settings.menu_x >= 0 && g_settings.menu_y >= 0)
            ImGui::SetNextWindowPos({g_settings.menu_x, g_settings.menu_y}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({760, 560}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({620, 420}, {FLT_MAX, FLT_MAX});

        ImGui::PushFont(g_overlay.menu_font);

        ImGui::Begin("##mainwindow", &g_settings.menu_open,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImVec2 pos = ImGui::GetWindowPos();
        g_settings.menu_x = pos.x;
        g_settings.menu_y = pos.y;

        render_header();
        ImGui::Dummy({0, 6});

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("##sidebar", {SIDEBAR_WIDTH, avail.y}, false, ImGuiWindowFlags_NoScrollbar);
        render_sidebar();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 14.0f);

        ImVec2 content_size = ImGui::GetContentRegionAvail();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28, 22));
        ImGui::BeginChild("##content", content_size, ImGuiChildFlags_AlwaysUseWindowPadding);
        render_content_panel(content_size);
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::End();

        // Render the spinning cat GIF outside the main window, centered over the sidebar
        auto srv = g_menu_cat_gif.get_current_frame((float)ImGui::GetTime());
        if (srv) {
            float gif_w = g_menu_cat_gif.width > 0 ? (float)g_menu_cat_gif.width : 200.0f;
            float gif_h = g_menu_cat_gif.height > 0 ? (float)g_menu_cat_gif.height : 200.0f;
            
            // Align horizontally with the center of the sidebar.
            // Sidebar starts at pos.x + 14 (WindowPadding.x).
            // Sidebar width is SIDEBAR_WIDTH (172.0f).
            float sidebar_center_x = pos.x + 14.0f + (SIDEBAR_WIDTH * 0.5f);
            float gif_x = sidebar_center_x - (gif_w * 0.5f);
            float gap = 20.0f; // Gap between gif and window
            float gif_y = pos.y - gif_h - gap;

            ImGui::SetNextWindowPos({gif_x, gif_y});
            ImGui::SetNextWindowSize({gif_w, gif_h});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            
            ImGui::Begin("##catgifwindow", nullptr, 
                         ImGuiWindowFlags_NoDecoration | 
                         ImGuiWindowFlags_NoBackground | 
                         ImGuiWindowFlags_NoInputs | 
                         ImGuiWindowFlags_NoMove | 
                         ImGuiWindowFlags_NoResize | 
                         ImGuiWindowFlags_NoSavedSettings | 
                         ImGuiWindowFlags_NoScrollWithMouse);
            
            ImGui::Image((ImTextureID)srv, {gif_w, gif_h});
            
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        ImGui::PopFont();
    }

private:
    enum NavIcon { ICON_HOME, ICON_EYE, ICON_RADAR, ICON_TARGET, ICON_WAVE, ICON_GRID, ICON_ORB, ICON_SLIDERS };
    struct NavItem { const char* label; const char* subtitle; int icon; };

    static constexpr int   NAV_COUNT     = 8;
    static constexpr float SIDEBAR_WIDTH = 172.0f;

    static constexpr NavItem nav_items[NAV_COUNT] = {
        {"Main",  "Status, hotkeys & performance", ICON_HOME},
        {"ESP",   "Player rendering & overlays",   ICON_EYE},
        {"Radar", "2D minimap overlay",            ICON_RADAR},
        {"Aim",   "Aimbot & triggerbot",           ICON_TARGET},
        {"RCS",   "Recoil compensation",           ICON_WAVE},
        {"Misc",  "Crosshair, spectators & HUD",   ICON_GRID},
        {"Nades", "Grenade lineup helper",         ICON_ORB},
        {"Menu",  "Appearance & theming",          ICON_SLIDERS},
    };

    float nav_anim[NAV_COUNT] = {};

    bool bind_waiting_menu = false;
    bool bind_waiting_master = false;
    bool bind_waiting_exit = false;
    bool reset_popup_open = false;
    bool bind_waiting_nade_toggle = false;
    bool bind_waiting_nade_add    = false;
    bool bind_waiting_nade_delete = false;
    bool bind_waiting_aimbot = false;
    bool bind_waiting_trigger = false;

    ImVec4 get_accent() const {
        return { g_settings.menu_accent_color[0], g_settings.menu_accent_color[1],
                 g_settings.menu_accent_color[2], g_settings.menu_accent_color[3] };
    }

    static ImVec4 lerp4(const ImVec4& a, const ImVec4& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
    }

    // ---- Soft glow primitives (faked blur via stacked translucent shapes) ----
    static void draw_glow_circle(ImDrawList* dl, ImVec2 center, float radius, ImVec4 color,
                                 float intensity, int layers = 5) {
        for (int i = layers; i >= 1; i--) {
            float t = (float)i / layers;
            float r = radius + t * radius * 1.6f;
            float a = intensity * (1.0f - t) * (1.0f - t);
            dl->AddCircleFilled(center, r, ImGui::ColorConvertFloat4ToU32({color.x, color.y, color.z, a}), 28);
        }
    }

    static void draw_glow_rect(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImVec4 color, float rounding,
                               float intensity, int layers = 4, float spread = 9.0f) {
        for (int i = layers; i >= 1; i--) {
            float t = (float)i / layers;
            float e = spread * t;
            float a = intensity * (1.0f - t) * (1.0f - t);
            dl->AddRectFilled({mn.x - e, mn.y - e}, {mx.x + e, mx.y + e},
                              ImGui::ColorConvertFloat4ToU32({color.x, color.y, color.z, a}), rounding + e);
        }
    }

    static void draw_nav_icon(ImDrawList* dl, ImVec2 c, int type, float r, ImU32 col) {
        switch (type) {
        case ICON_HOME:
            dl->AddRect({c.x - r, c.y - r}, {c.x + r, c.y + r}, col, 3.0f, 0, 1.4f);
            dl->AddCircleFilled(c, r * 0.32f, col, 12);
            break;
        case ICON_EYE:
            dl->AddBezierQuadratic({c.x - r, c.y}, {c.x, c.y - r * 0.9f}, {c.x + r, c.y}, col, 1.4f, 16);
            dl->AddBezierQuadratic({c.x - r, c.y}, {c.x, c.y + r * 0.9f}, {c.x + r, c.y}, col, 1.4f, 16);
            dl->AddCircleFilled(c, r * 0.30f, col, 12);
            break;
        case ICON_RADAR:
            dl->AddCircle(c, r, col, 20, 1.3f);
            dl->AddCircle(c, r * 0.55f, col, 16, 1.1f);
            dl->AddLine(c, {c.x + r * 0.95f, c.y - r * 0.55f}, col, 1.4f);
            break;
        case ICON_TARGET:
            dl->AddCircle(c, r, col, 20, 1.4f);
            dl->AddCircleFilled(c, 1.6f, col, 8);
            dl->AddLine({c.x - r - 3, c.y}, {c.x - r + 2, c.y}, col, 1.3f);
            dl->AddLine({c.x + r - 2, c.y}, {c.x + r + 3, c.y}, col, 1.3f);
            dl->AddLine({c.x, c.y - r - 3}, {c.x, c.y - r + 2}, col, 1.3f);
            dl->AddLine({c.x, c.y + r - 2}, {c.x, c.y + r + 3}, col, 1.3f);
            break;
        case ICON_WAVE:
            dl->AddLine({c.x - r, c.y - r * 0.5f}, {c.x - r * 0.2f, c.y + r * 0.2f}, col, 1.5f);
            dl->AddLine({c.x - r * 0.2f, c.y + r * 0.2f}, {c.x + r * 0.5f, c.y - r * 0.35f}, col, 1.5f);
            dl->AddLine({c.x + r * 0.5f, c.y - r * 0.35f}, {c.x + r, c.y + r * 0.35f}, col, 1.5f);
            dl->AddTriangleFilled({c.x + r * 0.65f, c.y + r * 0.10f},
                                  {c.x + r * 1.15f, c.y + r * 0.10f},
                                  {c.x + r * 0.90f, c.y + r * 0.65f}, col);
            break;
        case ICON_GRID: {
            float o = r * 0.5f;
            dl->AddCircleFilled({c.x - o, c.y - o}, 1.7f, col, 8);
            dl->AddCircleFilled({c.x + o, c.y - o}, 1.7f, col, 8);
            dl->AddCircleFilled({c.x - o, c.y + o}, 1.7f, col, 8);
            dl->AddCircleFilled({c.x + o, c.y + o}, 1.7f, col, 8);
            break;
        }
        case ICON_ORB:
            dl->AddCircle(c, r, col, 20, 1.3f);
            dl->AddCircleFilled({c.x - r * 0.32f, c.y - r * 0.32f}, r * 0.34f, col, 12);
            break;
        case ICON_SLIDERS:
            for (int i = 0; i < 3; i++) {
                float y  = c.y - r + i * r;
                float kx = c.x + (i == 0 ? -r * 0.4f : (i == 1 ? r * 0.35f : -r * 0.1f));
                dl->AddLine({c.x - r, y}, {c.x + r, y}, col, 1.3f);
                dl->AddCircleFilled({kx, y}, 2.1f, col, 10);
            }
            break;
        }
    }

    void render_header() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float  w = ImGui::GetContentRegionAvail().x;
        ImVec4 accent     = get_accent();
        ImU32  accent_u32 = ImGui::ColorConvertFloat4ToU32(accent);
        float  header_h   = 44.0f;

        // Glowing diamond logo glyph
        ImVec2 logo_c = {p.x + 17, p.y + header_h * 0.5f};
        draw_glow_circle(dl, logo_c, 9.0f, accent, 0.45f);
        ImVec2 dpts[4] = { {logo_c.x, logo_c.y - 9}, {logo_c.x + 9, logo_c.y},
                           {logo_c.x, logo_c.y + 9}, {logo_c.x - 9, logo_c.y} };
        dl->AddConvexPolyFilled(dpts, 4, accent_u32);
        ImVec2 ipts[4] = { {logo_c.x, logo_c.y - 4}, {logo_c.x + 4, logo_c.y},
                           {logo_c.x, logo_c.y + 4}, {logo_c.x - 4, logo_c.y} };
        dl->AddConvexPolyFilled(ipts, 4, ImGui::ColorConvertFloat4ToU32({0.05f, 0.05f, 0.07f, 0.9f}));

        float title_h = g_overlay.menu_title_font ? g_overlay.menu_title_font->FontSize : ImGui::GetFontSize();
        ImGui::PushFont(g_overlay.menu_title_font);
        ImGui::SetCursorScreenPos({p.x + 38, p.y + header_h * 0.5f - title_h - 1.0f});
        ImGui::TextColored(accent, "BlazeStrike");
        ImGui::PopFont();

        ImGui::SetCursorScreenPos({p.x + 38, p.y + header_h * 0.5f + 2.0f});
        ImGui::TextColored({0.45f, 0.50f, 0.50f, 1.0f}, "Performance Suite");

        // Status pill, glowing, right-aligned
        bool active = g_settings.master_switch;
        ImVec4 status_col = active ? ImVec4{0.35f, 0.95f, 0.55f, 1.0f} : ImVec4{0.95f, 0.40f, 0.40f, 1.0f};
        ImU32  status_u32 = ImGui::ColorConvertFloat4ToU32(status_col);
        const char* status_txt = active ? "ACTIVE" : "DISABLED";
        ImVec2 ts = ImGui::CalcTextSize(status_txt);
        float pill_w = ts.x + 32.0f, pill_h = 24.0f;
        ImVec2 pill_min = {p.x + w - pill_w, p.y + (header_h - pill_h) * 0.5f};
        ImVec2 pill_max = {pill_min.x + pill_w, pill_min.y + pill_h};
        draw_glow_rect(dl, pill_min, pill_max, status_col, pill_h * 0.5f, 0.30f, 3, 7.0f);
        dl->AddRectFilled(pill_min, pill_max,
                          ImGui::ColorConvertFloat4ToU32({status_col.x, status_col.y, status_col.z, 0.14f}), pill_h * 0.5f);
        dl->AddRect(pill_min, pill_max,
                    ImGui::ColorConvertFloat4ToU32({status_col.x, status_col.y, status_col.z, 0.55f}), pill_h * 0.5f, 0, 1.2f);
        ImVec2 dot_c = {pill_min.x + 15, (pill_min.y + pill_max.y) * 0.5f};
        dl->AddCircleFilled(dot_c, 3.0f, status_u32, 12);
        dl->AddText({dot_c.x + 9, (pill_min.y + pill_max.y) * 0.5f - ts.y * 0.5f}, status_u32, status_txt);

        // Gradient separator: fades in from the edges toward the center
        ImGui::SetCursorScreenPos({p.x, p.y + header_h + 6.0f});
        ImVec2 sp = ImGui::GetCursorScreenPos();
        ImU32 fade  = ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.0f});
        ImU32 solid = ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.5f});
        dl->AddRectFilledMultiColor(sp, {sp.x + w * 0.5f, sp.y + 1.4f}, fade, solid, solid, fade);
        dl->AddRectFilledMultiColor({sp.x + w * 0.5f, sp.y}, {sp.x + w, sp.y + 1.4f}, solid, fade, fade, solid);

        ImGui::SetCursorScreenPos({p.x, sp.y + 10.0f});
    }

    void render_sidebar() {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 accent = get_accent();
        float  dt = ImGui::GetIO().DeltaTime;
        float  w  = ImGui::GetContentRegionAvail().x;
        const float item_h = 38.0f;
        const float gap    = 4.0f;



        for (int i = 0; i < NAV_COUNT; i++) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(i);
            ImGui::InvisibleButton("##navitem", {w, item_h});
            bool hovered = ImGui::IsItemHovered();
            bool active  = (g_settings.menu_tab == i);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                g_settings.menu_tab = i;
            ImGui::PopID();

            // Smoothly animate the highlight strength toward its target each frame
            float target = active ? 1.0f : (hovered ? 0.45f : 0.0f);
            float speed  = std::min(dt * 14.0f, 1.0f);
            nav_anim[i] += (target - nav_anim[i]) * speed;
            float a = nav_anim[i];

            ImVec2 pmax = {p.x + w, p.y + item_h};
            if (a > 0.01f)
                dl->AddRectFilled(p, pmax,
                    ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.12f * a}), 8.0f);

            if (a > 0.02f) {
                ImVec2 bar_min = {p.x + 1, p.y + item_h * 0.24f};
                ImVec2 bar_max = {p.x + 4, p.y + item_h * 0.76f};
                ImVec4 bar_col = {accent.x, accent.y, accent.z, a};
                draw_glow_rect(dl, bar_min, bar_max, bar_col, 2.0f, 0.4f * a, 3, 6.0f);
                dl->AddRectFilled(bar_min, bar_max, ImGui::ColorConvertFloat4ToU32(bar_col), 2.0f);
            }

            ImVec2 icon_c = {p.x + 26.0f, p.y + item_h * 0.5f};
            ImVec4 icon_col = lerp4({0.50f, 0.55f, 0.55f, 1.0f}, accent, a);
            draw_nav_icon(dl, icon_c, nav_items[i].icon, 7.0f, ImGui::ColorConvertFloat4ToU32(icon_col));

            ImVec4 text_col = lerp4({0.62f, 0.66f, 0.66f, 1.0f}, {0.95f, 0.97f, 0.96f, 1.0f}, a);
            ImVec2 ts = ImGui::CalcTextSize(nav_items[i].label);
            dl->AddText({p.x + 46.0f, p.y + (item_h - ts.y) * 0.5f},
                        ImGui::ColorConvertFloat4ToU32(text_col), nav_items[i].label);

            ImGui::SetCursorScreenPos({p.x, pmax.y + gap});
        }
    }

    void render_content_panel(ImVec2 size) {
        ImDrawList* dl   = ImGui::GetWindowDrawList();
        ImVec2 cmin = ImGui::GetWindowPos();
        ImVec2 cmax = {cmin.x + size.x, cmin.y + size.y};
        ImVec4 accent = get_accent();

        dl->AddRectFilled(cmin, cmax, ImGui::ColorConvertFloat4ToU32({1.0f, 1.0f, 1.0f, 0.025f}), 10.0f);
        dl->AddRect(cmin, cmax, ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.16f}), 10.0f, 0, 1.0f);

        const NavItem& active = nav_items[g_settings.menu_tab];

        ImGui::Spacing();
        ImGui::PushFont(g_overlay.menu_title_font);
        ImGui::TextColored(accent, "%s", active.label);
        ImGui::PopFont();
        ImGui::TextColored({0.50f, 0.55f, 0.55f, 1.0f}, "%s", active.subtitle);

        ImVec2 sp = ImGui::GetCursorScreenPos();
        float  sw = ImGui::GetContentRegionAvail().x;
        ImU32 fade  = ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.0f});
        ImU32 solid = ImGui::ColorConvertFloat4ToU32({accent.x, accent.y, accent.z, 0.45f});
        dl->AddRectFilledMultiColor(sp, {sp.x + sw, sp.y + 1.2f}, solid, fade, fade, solid);
        ImGui::Dummy({0, 10});

        switch (g_settings.menu_tab) {
            case 0: render_tab_main();       break;
            case 1: render_tab_esp();        break;
            case 2: render_tab_radar();      break;
            case 3: render_tab_aim();        break;
            case 4: render_tab_rcs();        break;
            case 5: render_tab_misc();       break;
            case 6: render_tab_nades();      break;
            case 7: render_tab_menu_style(); break;
        }

        ImGui::Dummy({0, 6});
    }

    void add_tooltip(const char* desc) {
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void render_tab_main() {
        ImGui::Spacing();
        ImGui::Text("Status:");
        ImGui::SameLine();
        if (g_settings.master_switch)
            ImGui::TextColored({0.3f, 1.0f, 0.3f, 1}, "Active (%s)", vk_name(g_settings.key_master));
        else
            ImGui::TextColored({1.0f, 0.3f, 0.3f, 1}, "Disabled (%s)", vk_name(g_settings.key_master));

        ImGui::Separator();
        ImGui::Text("Key Binds");
        ImGui::Spacing();
        render_key_bind("Menu Toggle", g_settings.key_menu, bind_waiting_menu);
        add_tooltip("Hotkey to open and close this settings menu window.");
        render_key_bind("Master Toggle", g_settings.key_master, bind_waiting_master);
        add_tooltip("Global hotkey to quickly enable or disable all features at once.");
        render_key_bind("Exit", g_settings.key_exit, bind_waiting_exit);
        add_tooltip("Hotkey to safely exit the application.");

        ImGui::Separator();
        ImGui::Text("Performance");
        ImGui::Checkbox("Vsync", &g_settings.use_vsync);
        add_tooltip("Enable Vertical Sync to synchronize refresh rate with your monitor, reducing tearing and GPU usage.");

        ImGui::BeginDisabled(g_settings.use_vsync);
        ImGui::SliderFloat("Target FPS", &g_settings.target_fps, 30, 1000, "%.0f");
        add_tooltip("Overlay refresh rate. Higher values make ESP drawing smoother at the cost of slightly higher CPU usage.");
        ImGui::EndDisabled();
        ImGui::TextColored({0.5f, 0.5f, 0.5f, 1}, "Higher = smoother ESP");

        ImGui::Separator();
        if (ImGui::Button("Reset All Settings")) reset_popup_open = true;
        add_tooltip("Revert all configurations, colors, and hotkeys to their default values.");
        if (reset_popup_open) {
            ImGui::OpenPopup("Reset?##confirm");
            reset_popup_open = false;
        }
        if (ImGui::BeginPopupModal("Reset?##confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Reset ALL settings to defaults?");
            ImGui::Separator();
            if (ImGui::Button("Yes", {100, 0})) {
                g_settings.reset();
                g_overlay.font_rebuild_needed = true;
                g_overlay.apply_menu_style();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {100, 0})) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

// ---- paste this method in place of render_tab_esp() inside class Menu ----
    void render_tab_esp() {
        ImGui::Spacing();
        ImGui::Checkbox("ESP Enabled", &g_settings.esp_enabled);
        ImGui::SameLine();
        ImGui::Checkbox("Show Teammates", &g_settings.draw_teammates);
        ImGui::Separator();

        if (ImGui::CollapsingHeader("General Config")) {
            ImGui::Indent();
            
            // ESP Font
            ImGui::Text("ESP Font:");
            render_esp_font_selector();
            ImGui::Spacing();

            // Distance Opacity
            ImGui::Checkbox("Distance Opacity Drop", &g_settings.esp_opacity_drop);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Fade out ESP elements with distance.\nDoes not affect radar or spectator list.");
            if (g_settings.esp_opacity_drop) {
                ImGui::Indent();
                ImGui::SliderFloat("Fade Start##opd", &g_settings.esp_opacity_drop_start,
                                   100.0f, 3000.0f, "%.0f units");
                ImGui::SliderFloat("Fade End##opd",   &g_settings.esp_opacity_drop_end,
                                   200.0f, 5000.0f, "%.0f units");
                ImGui::SliderFloat("Min Opacity##opd", &g_settings.esp_opacity_drop_min,
                                   0.0f, 1.0f, "%.2f");
                // Clamp: start must be less than end
                if (g_settings.esp_opacity_drop_start >= g_settings.esp_opacity_drop_end)
                    g_settings.esp_opacity_drop_start = g_settings.esp_opacity_drop_end - 100.0f;
                ImGui::Unindent();
            }
            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Overlay Information")) {
            ImGui::Indent();

            // Name
            ImGui::Checkbox("Draw Name", &g_settings.draw_name);
            if (g_settings.draw_name) {
                ImGui::Indent();
                ImGui::RadioButton("Top##np",   &g_settings.name_position, 0); ImGui::SameLine();
                ImGui::RadioButton("Bot##np",   &g_settings.name_position, 1);
                if (ImGui::SliderFloat("Name Font##nf", &g_settings.name_font_size, 8, 24, "%.0f"))
                    g_overlay.font_rebuild_needed = true;
                ImGui::DragFloat("Offset X##no", &g_settings.name_offset_x, 0.5f, -50, 50, "%.1f");
                ImGui::DragFloat("Offset Y##no", &g_settings.name_offset_y, 0.5f, -50, 50, "%.1f");
                if (!g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("Name Color##nc", g_settings.name_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                ImGui::Checkbox("Shadow##ns", &g_settings.name_shadow);
                if (g_settings.name_shadow)
                    ImGui::ColorEdit4("Shadow##nsc", g_settings.name_shadow_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Unindent();
            }
            ImGui::Separator();

            // Weapon
            ImGui::Checkbox("Draw Weapon", &g_settings.draw_weapon);
            if (g_settings.draw_weapon) {
                ImGui::Indent();
                ImGui::Checkbox("Show Icon##wi", &g_settings.weapon_show_icon);
                if (g_settings.weapon_show_icon && !g_weapon_icons.has_any_icons()) {
                    ImGui::SameLine();
                    ImGui::TextColored({1, 0.6f, 0.2f, 1}, "(no icons in icons/)");
                }
                ImGui::Checkbox("Show Text##wt", &g_settings.weapon_show_text);
                if (ImGui::SliderFloat("Weapon Font##wf", &g_settings.weapon_font_size, 8, 20, "%.0f"))
                    g_overlay.font_rebuild_needed = true;
                ImGui::SliderFloat("Dist. Dropoff##wdd", &g_settings.weapon_distance_dropoff, 0, 1, "%.2f");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("0 = constant size\n1 = scales with distance\n0.3 = default");
                if (!g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("Text Color##wc", g_settings.weapon_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                    ImGui::ColorEdit4("Icon Tint##wic", g_settings.weapon_icon_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                ImGui::Checkbox("Weapon Shadow##ws", &g_settings.weapon_shadow);
                if (g_settings.weapon_shadow)
                    ImGui::ColorEdit4("Shadow##wsc", g_settings.weapon_shadow_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Unindent();
            }
            ImGui::Separator();

            // Health
            ImGui::Checkbox("Draw Health Bar", &g_settings.draw_healthbar);
            if (g_settings.draw_healthbar) {
                ImGui::Indent();
                if (g_settings.esp_use_theme) {
                    ImGui::BeginDisabled();
                    bool forced = true;
                    ImGui::Checkbox("Solid Color (theme override)##hbsc", &forced);
                    ImGui::EndDisabled();
                } else {
                    ImGui::Checkbox("Solid Color##hbsc", &g_settings.healthbar_solid_color);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(
                            "ON: flat single color (set below)\n"
                            "OFF: classic green / yellow / red gradient");
                }
                if (g_settings.healthbar_solid_color && !g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("Bar Color##hbcol", g_settings.healthbar_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                ImGui::Unindent();
            }

            ImGui::Checkbox("Draw Health Text", &g_settings.draw_health_text);
            if (g_settings.draw_health_text) {
                ImGui::Indent();
                if (ImGui::SliderFloat("HP Font##hpf", &g_settings.hp_font_size, 8, 24, "%.0f"))
                    g_overlay.font_rebuild_needed = true;
                if (!g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("HP Color##hpc", g_settings.hp_text_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                ImGui::Checkbox("HP Shadow", &g_settings.hp_text_shadow);
                if (g_settings.hp_text_shadow)
                    ImGui::ColorEdit4("Shadow##hps", g_settings.hp_text_shadow_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                ImGui::Unindent();
            }
            ImGui::Separator();

            // Flags & Bomb ESP
            ImGui::Text("ESP Flags:");
            ImGui::Checkbox("Money", &g_settings.draw_money); ImGui::SameLine();
            ImGui::Checkbox("Scoped", &g_settings.draw_scoped); ImGui::SameLine();
            ImGui::Checkbox("Flashed", &g_settings.draw_flashed);
            
            ImGui::Spacing();
            ImGui::Checkbox("Planted Bomb ESP in World", &g_settings.draw_bomb_esp);

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Models, Box & Chams")) {
            ImGui::Indent();

            // Box
            ImGui::Checkbox("Draw Box", &g_settings.draw_box);
            if (g_settings.draw_box) {
                ImGui::Indent();
                ImGui::RadioButton("Corners##bs", &g_settings.box_style, 0); ImGui::SameLine();
                ImGui::RadioButton("Full##bs",    &g_settings.box_style, 1); ImGui::SameLine();
                ImGui::RadioButton("Dashed##bs",  &g_settings.box_style, 2);
                ImGui::SliderFloat("Thickness##bt", &g_settings.box_thickness,  0.5f, 4.0f, "%.1f");
                ImGui::SliderFloat("Padding X",     &g_settings.box_padding_x,  0.0f, 20.0f, "%.1f");
                ImGui::SliderFloat("Padding Y",     &g_settings.box_padding_y,  0.0f, 20.0f, "%.1f");
                if (g_settings.box_style == 0)
                    ImGui::SliderFloat("Corner %", &g_settings.box_corner_pct, 0.1f, 0.5f, "%.2f");
                ImGui::Unindent();
            }
            ImGui::Separator();

            // Chams Style
            ImGui::Text("Chams Style:");
            ImGui::RadioButton("Filled",   &g_settings.chams_style, 0); ImGui::SameLine();
            ImGui::RadioButton("Wire",     &g_settings.chams_style, 1); ImGui::SameLine();
            ImGui::RadioButton("Glow",     &g_settings.chams_style, 2); ImGui::SameLine();
            ImGui::RadioButton("Skeleton", &g_settings.chams_style, 3);
            ImGui::Checkbox("Draw Head", &g_settings.draw_head);
            ImGui::Separator();

            // Snaplines / Tracers
            ImGui::Checkbox("Snaplines / Tracers", &g_settings.draw_snaplines);
            if (g_settings.draw_snaplines) {
                ImGui::Indent();
                ImGui::Text("Origin:");
                ImGui::RadioButton("Bottom##so", &g_settings.snapline_origin, 0); ImGui::SameLine();
                ImGui::RadioButton("Center##so", &g_settings.snapline_origin, 1); ImGui::SameLine();
                ImGui::RadioButton("Top##so",    &g_settings.snapline_origin, 2);
                if (!g_settings.esp_use_theme) {
                    ImGui::ColorEdit4("Color##socol", g_settings.snapline_color,
                                      ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
                }
                ImGui::Unindent();
            }
            ImGui::Separator();

            // Body Tuning
            ImGui::Text("Body Tuning");
            ImGui::SliderFloat("Body Width",  &g_settings.body_width_scale, 0.3f, 3.0f,  "%.2f");
            ImGui::SliderFloat("Head Size",   &g_settings.head_radius,      1,    10,     "%.1f");
            ImGui::SliderFloat("Depth Scale", &g_settings.depth_scale,      100,  1500,   "%.0f");
            ImGui::SliderFloat("Glow Outer",  &g_settings.glow_expand_outer, 0,   15,     "%.1f");
            ImGui::SliderFloat("Glow Inner",  &g_settings.glow_expand_inner, 0,   10,     "%.1f");
            if (ImGui::Button("Reset Body")) {
                g_settings.body_width_scale  = 1.0f;
                g_settings.head_radius       = 6.0f;
                g_settings.depth_scale       = 500.0f;
                g_settings.glow_expand_outer = 6.0f;
                g_settings.glow_expand_inner = 3.0f;
            }

            ImGui::Unindent();
        }

        if (ImGui::CollapsingHeader("Theme & Colors")) {
            ImGui::Indent();

            // Theme Color
            ImVec4 theme_accent = {g_settings.esp_theme_color[0], g_settings.esp_theme_color[1],
                                   g_settings.esp_theme_color[2], g_settings.esp_theme_color[3]};
            ImGui::TextColored(theme_accent, "ESP Theme Color");
            ImGui::Checkbox("Use Theme Color##esptheme", &g_settings.esp_use_theme);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(
                    "When ON: all ESP element colors (box, name, weapon,\n"
                    "healthbar) are automatically derived from the theme\n"
                    "color. Enemy gets the raw hue; teammates get a\n"
                    "blue-shifted tint. Individual color pickers below\n"
                    "are ignored while theme is active.");

            if (g_settings.esp_use_theme) {
                ImGui::Indent();
                ImGui::ColorEdit4("Theme Color##tc", g_settings.esp_theme_color,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

                // Quick presets
                ImGui::Text("Presets:");
                struct Preset { const char* name; float r, g, b; };
                static constexpr Preset presets[] = {
                    {"Cyan",       0.00f, 0.85f, 1.00f},
                    {"Lime",       0.20f, 1.00f, 0.30f},
                    {"Blue",       0.235f,0.68f, 0.93f},
                    {"Magenta",    0.90f, 0.10f, 0.80f},
                    {"White",      0.95f, 0.95f, 0.95f},
                    {"Red",        1.00f, 0.15f, 0.15f},
                };
                for (const auto& pr : presets) {
                    if (ImGui::Button(pr.name, {65, 0})) {
                        g_settings.esp_theme_color[0] = pr.r;
                        g_settings.esp_theme_color[1] = pr.g;
                        g_settings.esp_theme_color[2] = pr.b;
                        g_settings.esp_theme_color[3] = 1.0f;
                    }
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::Unindent();

                ImGui::TextColored({0.5f, 0.5f, 0.5f, 1}, "(Individual colors below are overridden by theme)");
            }

            // Custom Colors (only shown when theme is OFF)
            if (!g_settings.esp_use_theme) {
                ImGui::Text("Custom Colors");
                ImGui::Columns(2, nullptr, false);
                ImGui::Text("Enemy");
                ImGui::ColorEdit4("Fill##ef",    g_settings.enemy_fill,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Outline##eo", g_settings.enemy_outline, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Glow##eg",    g_settings.enemy_glow,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::NextColumn();
                ImGui::Text("Team");
                ImGui::ColorEdit4("Fill##tf",    g_settings.team_fill,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Outline##to", g_settings.team_outline, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4("Glow##tg",    g_settings.team_glow,    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
                ImGui::Columns(1);
            }

            ImGui::Unindent();
        }
    }

    void render_tab_radar() {
        ImGui::Spacing();
        ImGui::Checkbox("Show Radar", &g_settings.draw_radar);
        add_tooltip("Render a 2D overlay minimap showing player locations.");
        ImGui::Separator();
        ImGui::Checkbox("Circle Shape", &g_settings.radar_circle);
        add_tooltip("Render the radar as a circular window instead of a square.");
        ImGui::Checkbox("Rotate with View", &g_settings.radar_rotate);
        add_tooltip("Rotate the radar map dynamically with your player's camera view angle.");
        ImGui::Checkbox("Range Rings", &g_settings.radar_rings);
        add_tooltip("Draw distance helper concentric rings on the radar.");
        ImGui::Checkbox("Player Names", &g_settings.radar_names);
        add_tooltip("Show player name tags next to their dots on the radar.");
        if (g_settings.radar_names) {
            ImGui::Indent();
            if (ImGui::SliderFloat("Names Font##rnf", &g_settings.radar_names_font_size, 8.0f, 20.0f, "%.0f")) {
                g_overlay.font_rebuild_needed = true;
            }
            add_tooltip("Font size for the radar player names.");
            ImGui::Unindent();
        }
        ImGui::Separator();
        ImGui::SliderFloat("Size", &g_settings.radar_size, 100, 1000, "%.0f");
        add_tooltip("Width and height size of the radar window.");
        ImGui::SliderFloat("Zoom", &g_settings.radar_zoom, 0.1f, 1.0f, "%.2fx");
        add_tooltip("Map scale zoom factor.");
        ImGui::SliderFloat("Opacity", &g_settings.radar_bg_alpha, 0.0f, 1.0f, "%.2f");
        add_tooltip("Opacity level of the radar window background.");
        ImGui::DragFloat("Position X", &g_settings.radar_x, 1, 0, 3000);
        add_tooltip("Horizontal pixel position coordinate of the radar on your screen.");
        ImGui::DragFloat("Position Y", &g_settings.radar_y, 1, 0, 2000);
        add_tooltip("Vertical pixel position coordinate of the radar on your screen.");

        ImGui::Separator();
        ImGui::Text("Radar Colors");
        ImGui::ColorEdit4("Enemy##re", g_settings.radar_enemy_color,
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        add_tooltip("Enemy dot color on the radar.");
        ImGui::ColorEdit4("Team##rt", g_settings.radar_team_color,
                          ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs);
        add_tooltip("Teammate dot color on the radar.");
    }

    void render_tab_aim() {
        ImGui::Checkbox("Enable aimbot", &g_settings.aimbot_enabled);
        add_tooltip("Automatically snap or smooth your crosshair toward target players when holding the Aimbot Key.");

        if (g_settings.aimbot_enabled) {
            render_key_combo("Key",  g_settings.key_aimbot);
            add_tooltip("The hotkey you must hold down to activate aimbot lock-on.");
            ImGui::SliderInt("Fov", &g_settings.aimbot_fov, 1, 360);
            add_tooltip("Field of View angle (in degrees) defining the detection cone around your crosshair.");
            ImGui::SliderFloat("Smoothing", &g_settings.aimbot_smooth, 1.0f, 20.0f, "%.1f");
            add_tooltip("Movement dampening scale. Higher values make the aimbot lock-on look smoother and more legitimate.");
            ImGui::Text("Target Bones:");
            ImGui::Checkbox("Head##aim", &g_settings.aimbot_aim_head);
            add_tooltip("Aimbot target: Head.");
            ImGui::SameLine();
            ImGui::Checkbox("Neck##aim", &g_settings.aimbot_aim_neck);
            add_tooltip("Aimbot target: Neck.");
            ImGui::SameLine();
            ImGui::Checkbox("Chest##aim", &g_settings.aimbot_aim_chest);
            add_tooltip("Aimbot target: Chest.");
            ImGui::SameLine();
            ImGui::Checkbox("Pelvis##aim", &g_settings.aimbot_aim_pelvis);
            add_tooltip("Aimbot target: Pelvis.");
            ImGui::Checkbox("Humanized Aim", &g_settings.aimbot_humanized);
            add_tooltip("Enable organic aim curves, jitter, and ease transitions to simulate natural mouse input.");
            if (g_settings.aimbot_humanized) {
                ImGui::Indent();
                ImGui::SliderFloat("Curve Strength", &g_settings.aimbot_curve_strength, 0.0f, 15.0f, "%.1f");
                add_tooltip("Maximum curvature deviation from a straight line path to the target.");
                ImGui::SliderFloat("Jitter", &g_settings.aimbot_jitter, 0.0f, 10.0f, "%.1f");
                add_tooltip("Amount of micro-movements/random offset added to target tracking.");
                ImGui::SliderFloat("Ease-In", &g_settings.aimbot_ease_in, 0.0f, 1.0f, "%.2f");
                add_tooltip("Dampening coefficient applied when starting mouse movement.");
                ImGui::SliderFloat("Ease-Out", &g_settings.aimbot_ease_out, 0.0f, 1.0f, "%.2f");
                add_tooltip("Dampening coefficient applied when approaching the target bone.");
                ImGui::Unindent();
            }
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Checkbox("Enable triggerbot", &g_settings.triggerbot_enabled);
        add_tooltip("Automatically fire weapons when an enemy player model crosses directly underneath your crosshair.");

        if (g_settings.triggerbot_enabled)
        {
            render_key_combo("Trigger key", g_settings.key_triggerbot);
            add_tooltip("The hotkey you must hold down to engage triggerbot automatic firing.");
            ImGui::SliderInt("Delay ms", &g_settings.triggerbot_delay, 0, 300);
            add_tooltip("Delay interval in milliseconds before firing after an enemy crosses your crosshair.");
            ImGui::Checkbox("Head only##trigger", &g_settings.triggerbot_head_only);
            add_tooltip("Only trigger shot if the crosshair is placed over the enemy's head bone.");
        }

        ImGui::Separator();
    }

    void render_tab_rcs() {
        ImGui::Spacing();
        ImGui::Checkbox("Enable Recoil Control", &g_settings.rcs_enabled);
        add_tooltip("Automatically pull down/compensate weapon recoil pattern when firing.");

        if (g_settings.rcs_enabled) {
            ImGui::Indent();
            ImGui::Checkbox("Only whilst aiming", &g_settings.rcs_only_while_aiming);
            add_tooltip("Only apply recoil control compensation when the Aimbot is actively locking onto a target.");
            ImGui::SliderInt("Start Bullet", &g_settings.rcs_bullet, 1, 30);
            add_tooltip("The bullet count at which RCS compensation begins (1 = immediately on first shot).");
            ImGui::SliderFloat("RCS Scale X", &g_settings.rcs_scale_x, 0.0f, 2.0f, "%.2f");
            add_tooltip("Horizontal compensation strength multiplier.");
            ImGui::SliderFloat("RCS Scale Y", &g_settings.rcs_scale_y, 0.0f, 2.0f, "%.2f");
            add_tooltip("Vertical compensation strength multiplier.");
            ImGui::Unindent();
        }

        ImGui::Separator();
    }

    void render_tab_misc() {
        ImGui::Spacing();

        ImGui::Text("Overlay Boxes");
        ImGui::Checkbox("Show Bomb Timer", &g_settings.bomb_timer_enabled);
        add_tooltip("Show active C4 detonation/defusal time status window on screen.");
        ImGui::Checkbox("Show Vote Teller", &g_settings.vote_teller_enabled);
        add_tooltip("Show active in-game vote details and counts overlay window.");
        ImGui::Checkbox("Enemy Player Info Box", &g_settings.enemy_info_box_enabled);
        add_tooltip("Show a panel containing weapon details and HP stats of the target enemy player.");

        ImGui::Separator();

        ImGui::Text("Spectator List");
        ImGui::Checkbox("Show Spectators", &g_settings.draw_spectators);
        add_tooltip("Display names of players currently spectating your camera view.");
        if (g_settings.draw_spectators) {
            ImGui::Indent();
            ImGui::DragFloat("Spec X##sx", &g_settings.spec_x, 1, -1, 3000, "%.0f");
            add_tooltip("Horizontal pixel coordinate for the spectator list window (-1 for auto positioning).");
            ImGui::SameLine();
            if (ImGui::Button("Auto##specauto")) g_settings.spec_x = -1.0f;
            add_tooltip("Reset spectator window position back to default automatic placement.");
            ImGui::DragFloat("Spec Y##sy", &g_settings.spec_y, 1, 0, 2000, "%.0f");
            add_tooltip("Vertical pixel coordinate for the spectator list window.");
            ImGui::Unindent();
        }

        ImGui::Separator();

        ImGui::Text("Crosshair");
        ImGui::Checkbox("Enabled##xhair", &g_settings.crosshair_enabled);
        add_tooltip("Render a custom screen-centered crosshair overlay.");
        if (g_settings.crosshair_enabled) {
            ImGui::Indent();
            ImGui::Text("Shape:");
            ImGui::RadioButton("+##xs", &g_settings.crosshair_shape, 0);
            add_tooltip("Classic cross shape.");
            ImGui::SameLine();
            ImGui::RadioButton("T##xs", &g_settings.crosshair_shape, 1);
            add_tooltip("T-shaped crosshair.");
            ImGui::SameLine();
            ImGui::RadioButton("O##xs", &g_settings.crosshair_shape, 2);
            add_tooltip("Circular crosshair shape.");
            ImGui::SameLine();
            ImGui::RadioButton("Dot##xs", &g_settings.crosshair_shape, 3);
            add_tooltip("Single center point dot crosshair.");
            ImGui::SameLine();
            ImGui::RadioButton("+O##xs", &g_settings.crosshair_shape, 4);
            add_tooltip("Cross shape inside a circle.");

            ImGui::ColorEdit4("Color##xcol", g_settings.crosshair_color, ImGuiColorEditFlags_NoInputs);
            add_tooltip("Set primary crosshair color.");
            ImGui::SliderFloat("Size##xsz", &g_settings.crosshair_size, 0.5f, 20, "%.1f");
            add_tooltip("Adjust crosshair line length/size.");
            ImGui::SliderFloat("Thick##xth", &g_settings.crosshair_thickness, 0.5f, 5, "%.1f");
            add_tooltip("Adjust crosshair line thickness.");

            bool has_gap = g_settings.crosshair_shape <= 1 ||
                           g_settings.crosshair_shape == 4 ||
                           g_settings.crosshair_shape == 6;
            if (has_gap) {
                ImGui::SliderFloat("Gap##xgap", &g_settings.crosshair_gap, -10, 10, "%.1f");
                add_tooltip("Adjust center gap distance.");
            }

            ImGui::Checkbox("Outline##xol", &g_settings.crosshair_outline);
            add_tooltip("Draw a dark border outline around crosshair lines for better contrast.");
            if (g_settings.crosshair_outline) {
                ImGui::ColorEdit4("OL Color##xolc", g_settings.crosshair_outline_color, ImGuiColorEditFlags_NoInputs);
                add_tooltip("Color of the crosshair outline border.");
                ImGui::SliderFloat("OL Width##xolt", &g_settings.crosshair_outline_thickness, 1, 3, "%.0f");
                add_tooltip("Thickness of the crosshair outline border.");
            }
            if (g_settings.crosshair_shape != 3) {
                ImGui::Checkbox("Center Dot##xdot", &g_settings.crosshair_dot);
                add_tooltip("Draw a center dot in the middle of the crosshair lines.");
                if (g_settings.crosshair_dot) {
                    ImGui::SliderFloat("Dot Size##xds", &g_settings.crosshair_dot_size, 1, 4, "%.0f");
                    add_tooltip("Adjust size of the center dot.");
                }
            }

            ImGui::Separator();
            ImVec2 pp = ImGui::GetCursorScreenPos();
            float psz = 60;
            ImGui::InvisibleButton("##xprev", {psz, psz});
            add_tooltip("Live interactive preview of your custom crosshair design.");
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(pp, {pp.x + psz, pp.y + psz}, IM_COL32(20, 20, 20, 255));
            dl->AddRect(pp, {pp.x + psz, pp.y + psz}, IM_COL32(50, 50, 50, 255));
            dl->PushClipRect(pp, {pp.x + psz, pp.y + psz}, true);
            ImDrawListFlags old = dl->Flags;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedLines;
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            Crosshair::Config prev_cfg = {
                true, g_settings.crosshair_shape, g_settings.crosshair_size,
                g_settings.crosshair_gap, g_settings.crosshair_thickness,
                float4_to_col(g_settings.crosshair_color),
                g_settings.crosshair_outline, g_settings.crosshair_outline_thickness,
                float4_to_col(g_settings.crosshair_outline_color),
                g_settings.crosshair_dot, g_settings.crosshair_dot_size,
            };
            float pcx = floorf(pp.x + psz * 0.5f) + 0.5f;
            float pcy = floorf(pp.y + psz * 0.5f) + 0.5f;
            g_crosshair.draw_preview(dl, pcx, pcy, prev_cfg);
            dl->Flags = old;
            dl->PopClipRect();
            ImGui::Unindent();
        }
    }

    void render_tab_nades() {
        ImGui::Spacing();
        ImGui::Checkbox("Grenade Helper", &g_settings.grenade_helper_enabled);
        add_tooltip("Enable grenade lineup assistant to view positions, throw types, and target angles.");

        if (!g_settings.grenade_helper_enabled) {
            ImGui::TextColored({0.5f,0.5f,0.5f,1},
                "Enable to configure and use grenade lineups.");
            return;
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 130);
        if (g_settings.grenade_helper_visible)
            ImGui::TextColored({0.3f,1.0f,0.3f,1}, "[VISIBLE]");
        else
            ImGui::TextColored({0.6f,0.6f,0.6f,1}, "[HIDDEN]");

        ImGui::Separator();

        // Key binds
        ImGui::Text("Key Binds");
        render_key_bind("Toggle Visible",  g_settings.key_grenade_toggle, bind_waiting_nade_toggle);
        add_tooltip("Hotkey to toggle visibility of grenade spots overlays.");
        render_key_bind("Add Spot",        g_settings.key_grenade_add,    bind_waiting_nade_add);
        add_tooltip("Hotkey to save current camera/player position as a new custom grenade spot lineup.");
        render_key_bind("Delete Spot",     g_settings.key_grenade_delete, bind_waiting_nade_delete);
        add_tooltip("Hotkey to delete the nearest custom grenade lineup spot.");
        ImGui::Separator();

        // Filters
        ImGui::Text("Filters");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f,0.70f,0.70f,1));
        ImGui::Checkbox("Smoke##nf",   &g_settings.grenade_filter_smoke);
        add_tooltip("Filter: Show Smokes.");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f,0.47f,0.12f,1));
        ImGui::Checkbox("Molotov##nf", &g_settings.grenade_filter_molotov);
        add_tooltip("Filter: Show Molotovs.");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f,0.86f,0.31f,1));
        ImGui::Checkbox("Frag##nf",    &g_settings.grenade_filter_frag);
        add_tooltip("Filter: Show Frag Grenades.");
        ImGui::SameLine();
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f,1.00f,0.39f,1));
        ImGui::Checkbox("Flash##nf",   &g_settings.grenade_filter_flash);
        add_tooltip("Filter: Show Flashbangs.");
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Appearance
        ImGui::Text("Appearance");
        ImGui::SliderFloat("Circle Radius##nc",    &g_settings.grenade_circle_radius,    10.0f, 150.0f, "%.0f");
        add_tooltip("Radius size of grenade spot indicator markers.");
        ImGui::SliderFloat("Circle Thickness##nc", &g_settings.grenade_circle_thickness,  0.5f,   4.0f, "%.1f");
        add_tooltip("Line outline thickness of grenade spot indicator markers.");
        if (ImGui::SliderFloat("Text Size##nc",        &g_settings.grenade_text_font_size,    8.0f,  20.0f, "%.0f")) {
            g_overlay.font_rebuild_needed = true;
        }
        add_tooltip("Font size of grenade helper text overlays.");
        ImGui::Spacing();
        ImGui::ColorEdit4("Circle##ncc",        g_settings.grenade_circle_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Color of inactive/out-of-range grenade lineup circles.");
        ImGui::ColorEdit4("Active Circle##nac", g_settings.grenade_circle_active_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Color of the active grenade lineup circle when standing directly on it.");
        ImGui::ColorEdit4("Aim Line##nal",      g_settings.grenade_aim_line_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Color of alignment crosshair indicator guide.");
        ImGui::ColorEdit4("Text##ntc",          g_settings.grenade_text_color,
                          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Color of text info labels.");
        ImGui::Separator();

        // Spot list for current map
        ImGui::Text("Spots");
        ImGui::BeginChild("##nadespots", {0, 200}, true);
        g_grenades.render_spot_list();
        ImGui::EndChild();
    }

    void render_tab_menu_style() {
        ImGui::Spacing();
        ImGui::Text("Menu Appearance");
        ImGui::Separator();

        bool style_changed = false;
        style_changed |= ImGui::ColorEdit4("Accent Color", g_settings.menu_accent_color,
                                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Set primary accent color for window title and highlighted controls.");
        style_changed |= ImGui::ColorEdit4("Border Color", g_settings.menu_border_color,
                                            ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
        add_tooltip("Set border color of the settings panel.");
        style_changed |= ImGui::SliderFloat("BG Opacity", &g_settings.menu_bg_alpha, 0.3f, 1.0f, "%.2f");
        add_tooltip("Set transparency value of the settings window background.");

        ImGui::Separator();
        ImGui::Text("Presets:");
        if (ImGui::Button("Cyber Green", {120, 0})) {
            float cc[] = {0, 1, 0.65f, 1}; float b[] = {0, 1, 0.65f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        add_tooltip("Apply Cyber Green style preset.");
        ImGui::SameLine();
        if (ImGui::Button("Blood Red", {120, 0})) {
            float cc[] = {1, 0.2f, 0.15f, 1}; float b[] = {1, 0.2f, 0.15f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        add_tooltip("Apply Blood Red style preset.");
        ImGui::SameLine();
        if (ImGui::Button("Electric Blue", {120, 0})) {
            float cc[] = {0.2f, 0.5f, 1, 1}; float b[] = {0.2f, 0.5f, 1, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        add_tooltip("Apply Electric Blue style preset.");
        if (ImGui::Button("Purple Haze", {120, 0})) {
            float cc[] = {0.7f, 0.3f, 1, 1}; float b[] = {0.7f, 0.3f, 1, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.90f; style_changed = true;
        }
        add_tooltip("Apply Purple Haze style preset.");
        ImGui::SameLine();
        if (ImGui::Button("Gold", {120, 0})) {
            float cc[] = {1, 0.8f, 0.2f, 1}; float b[] = {1, 0.8f, 0.2f, 0.3f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.92f; style_changed = true;
        }
        add_tooltip("Apply Gold style preset.");
        ImGui::SameLine();
        if (ImGui::Button("Minimal", {120, 0})) {
            float cc[] = {0.7f, 0.7f, 0.7f, 1}; float b[] = {0.4f, 0.4f, 0.4f, 0.2f};
            memcpy(g_settings.menu_accent_color, cc, 16);
            memcpy(g_settings.menu_border_color, b, 16);
            g_settings.menu_bg_alpha = 0.88f; style_changed = true;
        }
        add_tooltip("Apply Minimal style preset.");

        if (style_changed)
            g_overlay.apply_menu_style();

        ImGui::Separator();

        ImGui::Text("Menu Font");
        render_menu_font_selector();
        add_tooltip("Select font used for the menu text.");

        if (ImGui::SliderFloat("Menu Font Size", &g_settings.menu_font_size, 10.0f, 22.0f, "%.0f"))
            g_overlay.font_rebuild_needed = true;
        add_tooltip("Adjust the menu font scale.");
    }

    struct KeyBindOption {
        const char* name;
        int vk_code;
    };

    void render_key_combo(const char* label, int& current_key) {
        static constexpr KeyBindOption keybind_options[] = {
            { "Mouse 1", VK_LBUTTON },
            { "Mouse 2", VK_RBUTTON },
            { "Middle Mouse", VK_MBUTTON },
            { "Mouse 4", VK_XBUTTON1 },
            { "Mouse 5", VK_XBUTTON2 },
            { "Alt", VK_MENU },
            { "Shift", VK_SHIFT },
            { "Caps", VK_CAPITAL }
        };

        const char* preview = "None";
        for (const auto& opt : keybind_options) {
            if (opt.vk_code == current_key) {
                preview = opt.name;
                break;
            }
        }

        if (ImGui::BeginCombo(label, preview)) {
            for (const auto& opt : keybind_options) {
                bool is_selected = (opt.vk_code == current_key);
                if (ImGui::Selectable(opt.name, is_selected)) {
                    current_key = opt.vk_code;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void render_key_bind(const char* label, int& key, bool& waiting, bool allow_mouse1 = false) {
        ImGui::Text("%s:", label);
        ImGui::SameLine(160);

        char btn[64];
        if (waiting)
            snprintf(btn, sizeof(btn), "[...]##%s", label);
        else
            snprintf(btn, sizeof(btn), "%s##%s", vk_name(key), label);

        if (ImGui::Button(btn, {100, 0})) {
            waiting = true;
        }

        if (waiting) {
            int pressed = scan_any_key(allow_mouse1);
            if (pressed > 0)   { key = pressed; waiting = false; }
            else if (pressed == -1) { waiting = false; } // Escape = cancel
        }
    }

    void render_esp_font_selector() {
        auto& fonts = g_overlay.available_fonts;
        if (fonts.empty()) { ImGui::Text("No fonts"); return; }
        const char* preview = (g_settings.esp_font_index >= 0 &&
                               g_settings.esp_font_index < (int)fonts.size())
                                  ? fonts[g_settings.esp_font_index].display_name.c_str() : "?";
        if (ImGui::BeginCombo("##espfont", preview)) {
            for (int i = 0; i < (int)fonts.size(); i++) {
                bool sel = (g_settings.esp_font_index == i);
                if (ImGui::Selectable(fonts[i].display_name.c_str(), sel)) {
                    if (g_settings.esp_font_index != i) {
                        g_settings.esp_font_index = i;
                        g_overlay.font_rebuild_needed = true;
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (g_overlay.esp_font) {
            ImGui::PushFont(g_overlay.esp_font);
            float sz = g_settings.name_font_size;
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const char* sample = "Player_Name 123";
            ImVec2 ts = g_overlay.esp_font->CalcTextSizeA(sz, FLT_MAX, 0, sample);
            dl->AddRectFilled(p, {p.x + ts.x + 8, p.y + ts.y + 4}, IM_COL32(15, 15, 15, 200), 3);
            dl->AddText(g_overlay.esp_font, sz, {p.x + 4, p.y + 2},
                        float4_to_col(g_settings.name_color), sample);
            ImGui::Dummy({ts.x + 8, ts.y + 6});
            ImGui::PopFont();
        }
    }

    void render_menu_font_selector() {
        auto& fonts = g_overlay.menu_fonts;
        if (fonts.empty()) { ImGui::Text("No fonts"); return; }
        const char* preview = (g_settings.menu_font_index >= 0 &&
                               g_settings.menu_font_index < (int)fonts.size())
                                  ? fonts[g_settings.menu_font_index].display_name.c_str() : "?";
        if (ImGui::BeginCombo("##menufont", preview)) {
            for (int i = 0; i < (int)fonts.size(); i++) {
                bool sel = (g_settings.menu_font_index == i);
                if (ImGui::Selectable(fonts[i].display_name.c_str(), sel)) {
                    if (g_settings.menu_font_index != i) {
                        g_settings.menu_font_index = i;
                        g_overlay.font_rebuild_needed = true;
                    }
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
};

inline Menu g_menu;