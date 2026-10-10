#include "home_screen.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "HomeScreen"

// Pixel-style colors (from FoloToy/ai-passport demo)
#define HS_SKY        0x1689E8
#define HS_SKY_DARK   0x0872C9
#define HS_INK        0x17202A
#define HS_PAPER      0xF4F4EA
#define HS_GRASS      0x82BE2D
#define HS_YELLOW     0xFFD928
#define HS_MUTED      0xD9E7EC
#define HS_BG         0x1A1A2E
#define HS_CARD_BG    0x16213E
#define HS_CARD_SEL   0x0F3460
#define HS_BORDER     0x333355
#define HS_BORDER_SEL 0x4FC3F7
#define HS_TEXT       0xE0E0E0
#define HS_TITLE      0x4FC3F7
#define HS_HINT       0x888888

void HomeScreen::SetFunctions(HomeFunction* funcs, int count) {
    funcs_ = funcs;
    func_count_ = count;
    selected_ = 0;
}

void HomeScreen::BuildScreen() {
    if (screen_ != nullptr) {
        lv_obj_delete(screen_);
        screen_ = nullptr;
    }

    auto display = Board::GetInstance().GetDisplay();
    if (!display || !display->SupportsGuiOperations()) return;

    DisplayLockGuard lock(display);

    // Create a new screen (like FoloToy demo's ui_pixel_screen_create)
    screen_ = lv_obj_create(nullptr);
    lv_obj_set_size(screen_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(HS_BG), 0);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_style_radius(screen_, 0, 0);
    lv_obj_clear_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    // Title
    lv_obj_t* title = lv_label_create(screen_);
    lv_label_set_text(title, "Select Function");
    lv_obj_set_style_text_color(title, lv_color_hex(HS_TITLE), 0);
    lv_obj_set_style_text_font(title, lv_font_monospace_28, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    // Function cards (vertical list, max 4)
    int card_w = 200;
    int card_h = 90;
    int start_y = 70;
    int gap = 16;
    int max_cards = func_count_ < 4 ? func_count_ : 4;

    for (int i = 0; i < max_cards; i++) {
        int y = start_y + i * (card_h + gap);

        // Card background panel
        cards_[i] = lv_obj_create(screen_);
        lv_obj_set_size(cards_[i], card_w, card_h);
        lv_obj_set_pos(cards_[i], (LV_HOR_RES - card_w) / 2, y);
        lv_obj_set_style_bg_color(cards_[i], lv_color_hex(HS_CARD_BG), 0);
        lv_obj_set_style_bg_opa(cards_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(cards_[i], 10, 0);
        lv_obj_set_style_border_width(cards_[i], 2, 0);
        lv_obj_set_style_border_color(cards_[i], lv_color_hex(HS_BORDER), 0);
        lv_obj_clear_flag(cards_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(cards_[i], LV_SCROLLBAR_MODE_OFF);

        // Function name label
        labels_[i] = lv_label_create(cards_[i]);
        lv_label_set_text(labels_[i], funcs_[i].name);
        lv_obj_set_style_text_color(labels_[i], lv_color_hex(HS_TEXT), 0);
        lv_obj_set_style_text_font(labels_[i], lv_font_monospace_20, 0);
        lv_obj_align(labels_[i], LV_ALIGN_CENTER, 0, -12);

        // Subtitle label
        subtitles_[i] = lv_label_create(cards_[i]);
        lv_label_set_text(subtitles_[i], funcs_[i].subtitle);
        lv_obj_set_style_text_color(subtitles_[i], lv_color_hex(HS_HINT), 0);
        lv_obj_set_style_text_font(subtitles_[i], lv_font_monospace_16, 0);
        lv_obj_align(subtitles_[i], LV_ALIGN_CENTER, 0, 16);
    }

    // Bottom hint
    hint_label_ = lv_label_create(screen_);
    lv_label_set_text(hint_label_, "UP/DOWN: Select  |  OK: Enter");
    lv_obj_set_style_text_color(hint_label_, lv_color_hex(HS_HINT), 0);
    lv_obj_set_style_text_font(hint_label_, lv_font_monospace_16, 0);
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -16);

    RefreshSelection();
}

void HomeScreen::Show() {
    if (func_count_ == 0) {
        ESP_LOGE(TAG, "No functions registered");
        return;
    }
    BuildScreen();
    lv_screen_load(screen_);
    visible_ = true;
    ESP_LOGI(TAG, "Home screen shown (%d functions)", func_count_);
}

void HomeScreen::Hide() {
    if (!visible_) return;
    visible_ = false;
    // Switch back to the default screen
    auto display = Board::GetInstance().GetDisplay();
    if (display && display->SupportsGuiOperations()) {
        DisplayLockGuard lock(display);
        // Load the original screen (the one that was active before we took over)
        // In xiaozhi firmware, the main UI is on the default screen
        lv_obj_t* default_scr = lv_screen_active();
        // We need to find the original screen - it's the one that's not ours
        // For now, just hide our screen elements
        if (screen_ != nullptr) {
            lv_obj_delete(screen_);
            screen_ = nullptr;
        }
        cards_[0] = cards_[1] = cards_[2] = cards_[3] = nullptr;
        labels_[0] = labels_[1] = labels_[2] = labels_[3] = nullptr;
        subtitles_[0] = subtitles_[1] = subtitles_[2] = subtitles_[3] = nullptr;
        hint_label_ = nullptr;
    }
    ESP_LOGI(TAG, "Home screen hidden");
}

void HomeScreen::RefreshSelection() {
    if (!visible_ || screen_ == nullptr) return;
    auto display = Board::GetInstance().GetDisplay();
    if (!display) return;
    DisplayLockGuard lock(display);

    int max_cards = func_count_ < 4 ? func_count_ : 4;
    for (int i = 0; i < max_cards; i++) {
        if (cards_[i] == nullptr) continue;
        if (i == selected_) {
            lv_obj_set_style_bg_color(cards_[i], lv_color_hex(HS_CARD_SEL), 0);
            lv_obj_set_style_border_color(cards_[i], lv_color_hex(HS_BORDER_SEL), 0);
            lv_obj_set_style_border_width(cards_[i], 3, 0);
        } else {
            lv_obj_set_style_bg_color(cards_[i], lv_color_hex(HS_CARD_BG), 0);
            lv_obj_set_style_border_color(cards_[i], lv_color_hex(HS_BORDER), 0);
            lv_obj_set_style_border_width(cards_[i], 2, 0);
        }
    }
}

bool HomeScreen::HandleKey(int button, int event) {
    if (!visible_) return false;

    // Only handle short clicks for navigation (long-press OK is "back" from sub-pages)
    if (event != 0) return false;  // Not a click, ignore

    switch (button) {
        case 0:  // UP
            selected_ = (selected_ - 1 + func_count_) % func_count_;
            RefreshSelection();
            return true;
        case 1:  // DOWN
            selected_ = (selected_ + 1) % func_count_;
            RefreshSelection();
            return true;
        case 2:  // OK - Enter selected function
            ESP_LOGI(TAG, "Function %d selected: %s", selected_, funcs_[selected_].name);
            if (funcs_[selected_].enter) {
                funcs_[selected_].enter();
            }
            return true;
        default:
            return false;
    }
}
