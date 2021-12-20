#pragma once

#include <map>
#include <string>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <vector>
#include <variant>
#include <set>
#include <unordered_map>
#include <imgui.h>
#include <unordered_set>
#include "EngineHandle.hpp"
#include "WindowSystem.hpp"


struct InputActionState {
    bool changed_this_frame {false};
    bool active {false};
};


struct KeyStroke {
    enum class KeyType {
        KEYBOARD,
        MOUSE,
        JOYSTICK
    };

    int key {0};
    KeyType key_type {KeyType::KEYBOARD};

    bool shift {false};
    bool ctrl {false};
    bool alt {false};

    auto operator<=>(const KeyStroke&) const = default;
};

struct KeyStrokeHasher {
    size_t operator()(const KeyStroke& ks) const {
        using std::hash;
        size_t res = hash<int>()(ks.key);
        res = hash_combine(res, hash<bool>()(ks.shift));
        res = hash_combine(res, hash<bool>()(ks.ctrl));
        res = hash_combine(res, hash<bool>()(ks.alt));
        res = hash_combine(res, hash<int>()((int)ks.key_type));
        return res;
    }

    static size_t hash_combine( size_t lhs, size_t rhs ) {
        lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
        return lhs;
    }
};


class KeyStrokeHandler {
    std::unordered_map<KeyStroke, bool, KeyStrokeHasher> key_strokes_;

public:
    void Update(
        const std::vector<InputActionState>& keyboard_keymap,
        const std::vector<InputActionState>& mouse_keymap
    ) {
        for (auto& pair : key_strokes_) {
            auto& ks = pair.first;
            auto& enabled = pair.second;
            bool shift = keyboard_keymap[GLFW_KEY_LEFT_SHIFT].active || keyboard_keymap[GLFW_KEY_RIGHT_SHIFT].active;
            bool ctrl = keyboard_keymap[GLFW_KEY_LEFT_CONTROL].active || keyboard_keymap[GLFW_KEY_RIGHT_CONTROL].active;
            bool alt = keyboard_keymap[GLFW_KEY_LEFT_ALT].active || keyboard_keymap[GLFW_KEY_RIGHT_ALT].active;
            bool satisfies_modifiers = (!ks.shift || shift) && (!ks.ctrl || ctrl) && (!ks.alt || alt);

            InputActionState state;
            if (ks.key_type == KeyStroke::KeyType::KEYBOARD) {
                state = keyboard_keymap[ks.key];
            } else if (ks.key_type == KeyStroke::KeyType::MOUSE) {
                state = mouse_keymap[ks.key];
            }
            if (state.changed_this_frame) {
                enabled = satisfies_modifiers && state.active;
            }
        }
    }

    void SetKeyStrokes(const std::vector<KeyStroke>& keystrokes) {
        std::unordered_map<KeyStroke, bool, KeyStrokeHasher> new_strokes;
        for (auto& ks : keystrokes) {
            new_strokes[ks] = key_strokes_[ks];
        }
        key_strokes_ = std::move(new_strokes);
    }

    bool IsActive(const KeyStroke& keystroke) {
        return key_strokes_[keystroke];
    }

    bool IsAnyActive(const std::vector<KeyStroke>& keystrokes) {
        for (auto& ks : keystrokes) {
            if (key_strokes_[ks]) {
                return true;
            }
        }
        return false;
    }
};


class Action {
    std::string name_;
    flecs::entity base_;

    flecs::entity on_press_;
    flecs::entity pressed_;
    flecs::entity on_release_;
    flecs::entity state_;

    bool prev_active_ = false;

public:
    std::vector<KeyStroke> key_strokes;

    Action(const flecs::entity& base, std::string name) : name_(std::move(name)), base_(base) {
        flecs::world world = base.world();

        std::string event_name_base = "InputAction_" + name_;
        on_press_ = world.entity((event_name_base + "_OnPress").c_str()).add(flecs::Tag);
        pressed_ = world.entity((event_name_base + "_Pressed").c_str()).add(flecs::Tag);
        on_release_ = world.entity((event_name_base + "_OnRelease").c_str()).add(flecs::Tag);
        //state_ = world.component<InputActionState>((event_name_base + "_State").c_str());
    }

    void setValue(bool active) {
        //auto state = static_cast<const InputActionState*>(base_.get(state_));
        bool changed_this_frame = prev_active_ != active;//state != nullptr && state->active != active;
        prev_active_ = active;
        //InputActionState new_state = {changed_this_frame, active};
        //base_.set_ptr(state_, &new_state);

        if (active && changed_this_frame)
            base_.add(on_press_);
        else
            base_.remove(on_press_);

        if (!active && changed_this_frame)
            base_.add(on_release_);
        else
            base_.remove(on_release_);

        if (active)
            base_.add(pressed_);
        else
            base_.remove(pressed_);
    }
};


template <class Key, class Value>
std::vector<Value> get_map_unique_values(std::unordered_map<Key, Value> map) {
    std::set<Value> ret;
    for (auto& pair : map) {
        ret.insert(pair.second);
    }
    return {ret.begin(), ret.end()};
}

class InputHandler {
    static const int MAX_KEYBOARD_KEY_ID = GLFW_KEY_LAST;
    static const int MAX_MOUSE_KEY_ID = GLFW_MOUSE_BUTTON_LAST;

    std::vector<InputActionState> keymap_{MAX_KEYBOARD_KEY_ID + 1};
    std::vector<InputActionState> mouse_keymap_{MAX_MOUSE_KEY_ID + 1};

    std::unordered_map<std::string, Action> actions_;
    KeyStrokeHandler key_stroke_handler_;

    flecs::entity listens_tag_;


public:
    static const std::unordered_map<std::string, int> name_to_keyboard_key;
    static const std::unordered_map<std::string, int> name_to_mouse_key;


    explicit InputHandler(flecs::entity listens_tag) : listens_tag_(std::move(listens_tag)) {
        AddAction("Pidorakshen", {.key = GLFW_KEY_3});
        AddAction("MoveForward", {.key = GLFW_KEY_W});
        AddAction("MoveLeft", {.key = GLFW_KEY_A});
        AddAction("MoveBackwards", {.key = GLFW_KEY_S});
        AddAction("MoveRight", {.key = GLFW_KEY_D});
    }

    void Update() {
        UpdateKeymaps();
        key_stroke_handler_.Update(keymap_, mouse_keymap_);
        for (auto& pair : actions_) {
            Action& action = pair.second;
            action.setValue(key_stroke_handler_.IsAnyActive(action.key_strokes));
        }
    }

    void UpdateKeymaps() {
        static const std::vector<int> keyboard_ids = get_map_unique_values(name_to_keyboard_key);
        static const std::vector<int> mouse_ids = get_map_unique_values(name_to_mouse_key);

        auto window_c = g_engine.world().entity("MainWindow").get<CWindow>();
        if (!window_c || !window_c->glfw_window) {
            spdlog::error("OOPSIE FUCKY WUCKY!! INPUT HANDLER FOUND NO WINDOW PLEASE FIXIE uWu");
            return;
        }
        auto window = window_c->glfw_window.get();

        bool ignore_kb = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
        bool ignore_mouse = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse;

        for (auto key_id : keyboard_ids) {
            auto active = !ignore_kb && glfwGetKey(window, key_id) == GLFW_PRESS;
            auto& key_state = keymap_[key_id];
            key_state.changed_this_frame = active != key_state.active;
            key_state.active = active;
        }

        for (auto key_id : mouse_ids) {
            auto active = !ignore_mouse && glfwGetMouseButton(window, key_id) == GLFW_PRESS;
            auto& key_state = mouse_keymap_[key_id];
            key_state.changed_this_frame = active != key_state.active;
            key_state.active = active;
        }
    }

    void AddAction(const std::string& name, KeyStroke keystroke) {
        if (!actions_.contains(name)) {
            actions_.emplace(name, Action{listens_tag_, name});
        }
        actions_.at(name).key_strokes.push_back(keystroke);
        UpdateKeyStrokes();
    }

    void UpdateKeyStrokes() {
        std::unordered_set<KeyStroke, KeyStrokeHasher> all;
        for (auto& pair : actions_) {
            for (auto& ks : pair.second.key_strokes) {
                all.insert(ks);
            }
        }

        key_stroke_handler_.SetKeyStrokes({all.begin(), all.end()});
    }

    void LoadFromConfig(const std::string& path) {

    }

    static std::unique_ptr<InputHandler> register_input_systems(flecs::world& world);
};


struct CGlobalInputHandlerRef {
    InputHandler* ref;
};


