#pragma once

#include <map>
#include <string>
#include <GLFW/glfw3.h>
#include <flecs.h>
#include <vector>
#include <variant>
#include <set>
#include <unordered_map>
#include <array>
#include <imgui.h>
#include <unordered_set>
#include "EngineHandle.hpp"
#include "WindowSystem.hpp"


struct InputActionState {
    bool changed_this_frame {false};
    bool active {false};
};

struct InputAxisState {
    double value {0.};
};


enum class InputAxis {
    MOUSE_X,
    MOUSE_Y,
    GAMEPAD_LEFT_X,
    GAMEPAD_LEFT_Y,
    GAMEPAD_RIGHT_X,
    GAMEPAD_RIGHT_Y,
    GAMEPAD_LEFT_TRIGGER,
    GAMEPAD_RIGHT_TRIGGER,
    MAX
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
    size_t operator()(const KeyStroke& ks) const;

    static size_t hash_combine( size_t lhs, size_t rhs );
};


class KeyStrokeHandler {
    std::unordered_map<KeyStroke, bool, KeyStrokeHasher> key_strokes_;

public:
    void Update(
        const std::vector<InputActionState>& keyboard_keymap,
        const std::vector<InputActionState>& mouse_keymap
    );

    void SetKeyStrokes(const std::vector<KeyStroke>& keystrokes);

    bool IsActive(const KeyStroke& keystroke);

    bool IsAnyActive(const std::vector<KeyStroke>& keystrokes);
};


class Action {
    std::string name_;
    flecs::entity base_;

    flecs::entity on_press_;
    flecs::entity pressed_;
    flecs::entity on_release_;
    flecs::entity state_;

public:
    std::vector<KeyStroke> key_strokes;

    Action(const flecs::entity& base, std::string name);

    void setValue(bool active);
};


class Axis {
    std::string name_;
    flecs::entity base_;

    flecs::entity state_;

public:
    std::unordered_map<KeyStroke, double, KeyStrokeHasher> key_strokes;
    std::array<double, (size_t)InputAxis::MAX> input_axes {};

    Axis(const flecs::entity& base, std::string name);

    void setValue(float value);
};


class InputHandler {
    static const int MAX_KEYBOARD_KEY_ID = GLFW_KEY_LAST;
    static const int MAX_MOUSE_KEY_ID = GLFW_MOUSE_BUTTON_LAST;

    std::vector<InputActionState> keymap_{MAX_KEYBOARD_KEY_ID + 1};
    std::vector<InputActionState> mouse_keymap_{MAX_MOUSE_KEY_ID + 1};

    std::pair<double, double> mouse_position_;
    std::array<double, static_cast<size_t>(InputAxis::MAX)> input_axes_map_ {};

    std::unordered_map<std::string, Action> actions_;
    std::unordered_map<std::string, Axis> axes_;
    KeyStrokeHandler key_stroke_handler_;

    flecs::entity listens_tag_;


public:
    static const std::unordered_map<std::string, int> name_to_keyboard_key;
    static const std::unordered_map<std::string, int> name_to_mouse_key;
    static const std::unordered_map<std::string, InputAxis> name_to_input_axis;


    explicit InputHandler(flecs::entity listens_tag);

    void Update();

    void UpdateAxes();

    void UpdateKeymaps();

    void AddAction(const std::string& name, KeyStroke keystroke);

    void AddAxis(const std::string& name, KeyStroke keystroke, double value);

    void AddAxis(const std::string& name, InputAxis input_axis, double value);

    void RebuildKeyStrokesHandler();

    void LoadFromConfig(const std::string& path) {

    }

    static std::unique_ptr<InputHandler> register_input_systems(flecs::world& world);
};


struct CGlobalInputHandlerRef {
    InputHandler* ref;
};


