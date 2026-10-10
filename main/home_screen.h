#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H

#include <lvgl.h>
#include <functional>
#include <string>

/**
 * Home Screen - Function selection menu (pixel style)
 * 
 * Based on FoloToy/ai-passport demo pattern:
 * - Grid of cards, UP/DOWN to navigate, OK to enter
 * - Long-press OK from any sub-page returns to this menu
 * - Only the selected function is active
 */

// Function entry definition (mirrors demo_entry_t from FoloToy demo)
struct HomeFunction {
    const char* name;        // Display name on the card
    const char* subtitle;    // Small text below name
    std::function<void()> enter;   // Called when user selects this function
    std::function<void()> exit;    // Called when returning to menu
};

class HomeScreen {
public:
    static HomeScreen& GetInstance() {
        static HomeScreen instance;
        return instance;
    }

    /**
     * Register available functions (call before Show)
     */
    void SetFunctions(HomeFunction* funcs, int count);

    /**
     * Show the home screen (creates LVGL screen if needed, loads it)
     */
    void Show();

    /**
     * Hide the home screen (unloads the screen)
     */
    void Hide();

    bool IsVisible() const { return visible_; }
    int GetSelectedIndex() const { return selected_; }

    /**
     * Handle key events (call from button callback)
     * Returns true if the event was consumed by the home screen
     */
    bool HandleKey(int button, int event);
    // button: 0=UP, 1=DOWN, 2=OK
    // event:  0=CLICK, 1=LONG

private:
    HomeScreen() = default;

    lv_obj_t* screen_ = nullptr;
    lv_obj_t* cards_[4] = {nullptr};
    lv_obj_t* labels_[4] = {nullptr};
    lv_obj_t* subtitles_[4] = {nullptr};
    lv_obj_t* hint_label_ = nullptr;

    HomeFunction* funcs_ = nullptr;
    int func_count_ = 0;
    int selected_ = 0;
    bool visible_ = false;

    void BuildScreen();
    void RefreshSelection();
};

#endif // HOME_SCREEN_H
