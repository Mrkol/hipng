#include "core/InputHandler.hpp"
#include "core/GameplaySystem.hpp"
#include "core/EngineHandle.hpp"
#include "core/WindowSystem.hpp"
#include <fstream>
#include <unordered_set>
#include <imgui.h>
#include <yaml-cpp/yaml.h>


std::unique_ptr<InputHandler> InputHandler::register_input_systems(flecs::world &world) {
    auto listens_tag_ = world.prefab("ListensToInputEvents");
    auto handler = std::make_unique<InputHandler>(listens_tag_);
    world.set<CGlobalInputHandlerRef>({handler.get()});

    // world.entity().is_a(world.entity("ListensToInputEvents"));
    world.system<CPosition, InputAxisState>()
        .arg(2).obj(world.entity("InputAxis_MoveForward_State"))
        .each([](flecs::entity e, CPosition& pos, InputAxisState& axis) {
            pos.position += pos.rotation * glm::vec3(0, 0, -1) * e.delta_time() * (float)axis.value;
        });
    world.system<CPosition>()
        .term(world.entity("InputAction_MoveLeft_Active"))
        .each([](flecs::entity e, CPosition& pos) {
            pos.position -= pos.rotation * glm::vec3(1, 0, 0) * e.delta_time();
        });
    world.system<CPosition, InputActionState>()
        .arg(2).obj(world.entity("InputAction_MoveRight_State"))
        .each([](flecs::entity e, CPosition& pos, InputActionState& state) {
            if (state.active)
                pos.position += pos.rotation * glm::vec3(1, 0, 0) * e.delta_time();
        });
    world.system<CPosition, InputAxisState>()
        .arg(2).obj(world.entity("InputAxis_MoveUp_State"))
        .each([](flecs::entity e, CPosition& pos, InputAxisState& axis) {
            pos.position += pos.rotation * glm::vec3(0, 1, 0) * e.delta_time() * (float)axis.value;
        });
    world.system<CPosition>()
        .term<InputAxisState>(world.entity("InputAxis_CameraX_State"))
        .term<InputAxisState>(world.entity("InputAxis_CameraY_State"))
        .term(world.entity("InputAction_MouseLook_Active"))
        .iter([](flecs::iter iter) {
            auto pos = iter.term<CPosition>(1);
            auto x = iter.term<InputAxisState>(2);
            auto y = iter.term<InputAxisState>(3);
            for (auto i : iter) {
                glm::quat yaw = glm::angleAxis(glm::radians(x[i].value), glm::dvec3{0., -1., 0.});
                glm::quat pitch = glm::angleAxis(glm::radians(y[i].value), glm::dvec3{-1., 0., 0.});
                pos[i].rotation = yaw * pos[i].rotation * pitch;
            }
        });

    world.system().term(world.entity("InputAction_Exit_OnDeactivate"))
        .iter([](flecs::iter iter) {
            iter.world().quit();
        });

    return handler;
}

InputHandler::InputHandler(flecs::entity listens_tag) : listens_tag_(std::move(listens_tag)) {
    /*AddAction("MoveLeft", {.key = GLFW_KEY_A});
    AddAction("MoveRight", {.key = GLFW_KEY_D});
    AddAxis("MoveForward", {.key = GLFW_KEY_W}, 1.);
    AddAxis("MoveForward", {.key = GLFW_KEY_S}, -1.);
    AddAxis("MoveUp", {.key = GLFW_KEY_SPACE}, 1.);
    AddAxis("MoveUp", {.key = GLFW_KEY_LEFT_SHIFT}, -1.);
    AddAction("MouseLook", {.key = GLFW_MOUSE_BUTTON_RIGHT, .key_type = KeyStroke::KeyType::MOUSE});
    AddAxis("CameraX", InputAxis::MOUSE_X, .3);
    AddAxis("CameraY", InputAxis::MOUSE_Y, .3);

    AddAction("Exit", {.key = GLFW_KEY_ESCAPE});
    AddAction("Exit", {.key = GLFW_KEY_Q, .ctrl = true, .alt = true});*/

    LoadFromConfig(NG_PROJECT_BASEPATH"/engine/resources/config/input.yaml");
}

KeyStroke InputHandler::ParseKeyStroke(const std::string& key_stroke) {
    std::vector<std::string> tokens;
    {
        std::string token;
        std::istringstream tokenStream(key_stroke);
        while (std::getline(tokenStream, token, '+')) {
            tokens.push_back(token);
        }
    }

    KeyStroke ks{};

    {
        auto key = tokens.at(tokens.size() - 1);
        if (name_to_keyboard_key.contains(key)) {
            ks.key = name_to_keyboard_key.at(key);
            ks.key_type = KeyStroke::KeyType::KEYBOARD;
        } else if (name_to_mouse_key.contains(key)) {
            ks.key = name_to_mouse_key.at(key);
            ks.key_type = KeyStroke::KeyType::MOUSE;
        }
    }

    for (size_t i = 0; i < tokens.size() - 1; ++i) {
        const auto& mod_key = tokens[i];
        if (mod_key == "CTRL") {
            ks.ctrl = true;
        } else if (mod_key == "SHIFT") {
            ks.shift = true;
        } else if (mod_key == "ALT") {
            ks.alt = true;
        }
    }

    return ks;
}

void InputHandler::LoadFromConfig(const std::string &path) {
    YAML::Node doc = YAML::LoadFile(path);
    for (auto it = doc["actions"].begin(); it != doc["actions"].end(); ++it) {
        auto name = it->first.as<std::string>();
        if (it->second.IsScalar()) {
            AddAction(name, ParseKeyStroke(it->second.as<std::string>()));
        } else {
            for (auto kss = it->second.begin(); kss != it->second.end(); ++kss) {
                AddAction(name, ParseKeyStroke(kss->as<std::string>()));
            }
        }
    }
    for (auto it = doc["axes"].begin(); it != doc["axes"].end(); ++it) {
        auto name = it->first.as<std::string>();
        for (auto kss = it->second.begin(); kss != it->second.end(); ++kss) {
            auto key_name = kss->first.as<std::string>();
            auto value = kss->second.as<double>();
            if (name_to_input_axis.contains(key_name)) {
                AddAxis(name, name_to_input_axis.at(key_name), value);
            } else {
                AddAxis(name, ParseKeyStroke(key_name), value);
            }
        }
    }
}

void InputHandler::Update() {
    UpdateKeymaps();
    UpdateAxes();
    key_stroke_handler_.Update(keymap_, mouse_keymap_);

    // Update actions
    for (auto& pair : actions_) {
        Action& action = pair.second;
        action.setValue(key_stroke_handler_.IsAnyActive(action.key_strokes));
    }

    // Update axes
    for (auto& pair : axes_) {
        Axis& axis = pair.second;
        double axis_value = 0.;
        for (auto& pair : axis.key_strokes) {
            const KeyStroke& ks = pair.first;
            const double value = pair.second;
            if (key_stroke_handler_.IsActive(ks)) {
                axis_value += value;
            }
        }
        for (size_t i = 0; i < axis.input_axes.size(); ++i) {
            double value = axis.input_axes[i];
            axis_value += input_axes_map_[i] * value;
        }
        axis.setValue(axis_value);
    }
}

template <class Key, class Value>
std::vector<Value> get_map_unique_values(std::unordered_map<Key, Value> map) {
    std::set<Value> ret;
    for (auto& pair : map) {
        ret.insert(pair.second);
    }
    return {ret.begin(), ret.end()};
}

void InputHandler::UpdateKeymaps() {
    static const std::vector<int> keyboard_ids = get_map_unique_values(name_to_keyboard_key);
    static const std::vector<int> mouse_ids = get_map_unique_values(name_to_mouse_key);

    auto window_c = g_engine.world().entity("HipNg").get<CWindow>();
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

void InputHandler::AddAction(const std::string &name, KeyStroke keystroke) {
    if (!actions_.contains(name)) {
        actions_.emplace(name, Action{listens_tag_, name});
    }
    actions_.at(name).key_strokes.push_back(keystroke);
    RebuildKeyStrokesHandler();
}

void InputHandler::AddAxis(const std::string &name, KeyStroke keystroke, double value) {
    if (!axes_.contains(name)) {
        axes_.emplace(name, Axis{listens_tag_, name});
    }
    axes_.at(name).key_strokes.emplace(keystroke, value);
    RebuildKeyStrokesHandler();
}

void InputHandler::AddAxis(const std::string &name, InputAxis input_axis, double value) {
    if (!axes_.contains(name)) {
        axes_.emplace(name, Axis{listens_tag_, name});
    }
    axes_.at(name).input_axes[(size_t)input_axis] = value;
}

void InputHandler::RebuildKeyStrokesHandler() {
    std::unordered_set<KeyStroke, KeyStrokeHasher> all;
    for (auto& pair : actions_) {
        for (auto& ks : pair.second.key_strokes) {
            all.insert(ks);
        }
    }

    key_stroke_handler_.SetKeyStrokes({all.begin(), all.end()});
}

void InputHandler::UpdateAxes() {
    auto window_c = g_engine.world().entity("HipNg").get<CWindow>();
    if (!window_c || !window_c->glfw_window) {
        spdlog::error("OOPSIE FUCKY WUCKY!! INPUT HANDLER FOUND NO WINDOW PLEASE FIXIE uWu");
        return;
    }
    auto window = window_c->glfw_window.get();

    // TODO: Check if this poll happened after focus loss, so axis doesn't jump like crazy at such frame.
    // TODO: Also, should ignore first frame for the same reason, because initially pos is 0, 0
    double x, y;
    glfwGetCursorPos(window, &x, &y);

    input_axes_map_[(size_t)InputAxis::MOUSE_X] = x - mouse_position_.first;
    input_axes_map_[(size_t)InputAxis::MOUSE_Y] = y - mouse_position_.second;

    mouse_position_.first = x;
    mouse_position_.second = y;
}

Action::Action(const flecs::entity &base, std::string name) : name_(std::move(name)), base_(base) {
    flecs::world world = base.world();

    std::string event_name_base = "InputAction_" + name_;
    on_press_ = world.entity((event_name_base + "_OnActivate").c_str()).add(flecs::Tag);
    pressed_ = world.entity((event_name_base + "_Active").c_str()).add(flecs::Tag);
    on_release_ = world.entity((event_name_base + "_OnDeactivate").c_str()).add(flecs::Tag);
    state_ = world.entity((event_name_base + "_State").c_str());
    base_.set<InputActionState>(state_, {});
}

void Action::setValue(bool active) {
    auto state = base_.get<InputActionState>(state_);
    bool changed_this_frame = state != nullptr && state->active != active;
    base_.set<InputActionState>(state_, {changed_this_frame, active});

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

Axis::Axis(const flecs::entity &base, std::string name) : name_(std::move(name)), base_(base) {
    flecs::world world = base.world();

    std::string event_name_base = "InputAxis_" + name_;
    state_ = world.entity((event_name_base + "_State").c_str());
    base_.set<InputAxisState>(state_, {});
}

void Axis::setValue(float value) {
    base_.set<InputAxisState>(state_, {value});
}

void KeyStrokeHandler::Update(const std::vector<InputActionState> &keyboard_keymap,
                              const std::vector<InputActionState> &mouse_keymap) {
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

void KeyStrokeHandler::SetKeyStrokes(const std::vector<KeyStroke> &keystrokes) {
    std::unordered_map<KeyStroke, bool, KeyStrokeHasher> new_strokes;
    for (auto& ks : keystrokes) {
        new_strokes[ks] = key_strokes_[ks];
    }
    key_strokes_ = std::move(new_strokes);
}

bool KeyStrokeHandler::IsActive(const KeyStroke &keystroke) {
    return key_strokes_[keystroke];
}

bool KeyStrokeHandler::IsAnyActive(const std::vector<KeyStroke> &keystrokes) {
    for (auto& ks : keystrokes) {
        if (key_strokes_[ks]) {
            return true;
        }
    }
    return false;
}

size_t KeyStrokeHasher::operator()(const KeyStroke &ks) const {
    using std::hash;
    size_t res = hash<int>()(ks.key);
    res = hash_combine(res, hash<bool>()(ks.shift));
    res = hash_combine(res, hash<bool>()(ks.ctrl));
    res = hash_combine(res, hash<bool>()(ks.alt));
    res = hash_combine(res, hash<int>()((int)ks.key_type));
    return res;
}

size_t KeyStrokeHasher::hash_combine(size_t lhs, size_t rhs) {
    lhs ^= rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2);
    return lhs;
}


const std::unordered_map<std::string, int> InputHandler::name_to_keyboard_key = {
    { "SPACE", GLFW_KEY_SPACE },
    { "APOSTROPHE", GLFW_KEY_APOSTROPHE }, /* ' */
    { "COMMA", GLFW_KEY_COMMA }, /* , */
    { "MINUS", GLFW_KEY_MINUS }, /* - */
    { "PERIOD", GLFW_KEY_PERIOD }, /* . */
    { "SLASH", GLFW_KEY_SLASH }, /* / */
    { "0", GLFW_KEY_0 },
    { "1", GLFW_KEY_1 },
    { "2", GLFW_KEY_2 },
    { "3", GLFW_KEY_3 },
    { "4", GLFW_KEY_4 },
    { "5", GLFW_KEY_5 },
    { "6", GLFW_KEY_6 },
    { "7", GLFW_KEY_7 },
    { "8", GLFW_KEY_8 },
    { "9", GLFW_KEY_9 },
    { "SEMICOLON", GLFW_KEY_SEMICOLON }, /* ; */
    { "EQUAL", GLFW_KEY_EQUAL }, /* = */
    { "A", GLFW_KEY_A },
    { "B", GLFW_KEY_B },
    { "C", GLFW_KEY_C },
    { "D", GLFW_KEY_D },
    { "E", GLFW_KEY_E },
    { "F", GLFW_KEY_F },
    { "G", GLFW_KEY_G },
    { "H", GLFW_KEY_H },
    { "I", GLFW_KEY_I },
    { "J", GLFW_KEY_J },
    { "K", GLFW_KEY_K },
    { "L", GLFW_KEY_L },
    { "M", GLFW_KEY_M },
    { "N", GLFW_KEY_N },
    { "O", GLFW_KEY_O },
    { "P", GLFW_KEY_P },
    { "Q", GLFW_KEY_Q },
    { "R", GLFW_KEY_R },
    { "S", GLFW_KEY_S },
    { "T", GLFW_KEY_T },
    { "U", GLFW_KEY_U },
    { "V", GLFW_KEY_V },
    { "W", GLFW_KEY_W },
    { "X", GLFW_KEY_X },
    { "Y", GLFW_KEY_Y },
    { "Z", GLFW_KEY_Z },
    { "LEFT_BRACKET", GLFW_KEY_LEFT_BRACKET }, /* [ */
    { "BACKSLASH", GLFW_KEY_BACKSLASH }, /* \ */
    { "RIGHT_BRACKET", GLFW_KEY_RIGHT_BRACKET }, /* ] */
    { "GRAVE", GLFW_KEY_GRAVE_ACCENT }, /* ` */
    { "ESCAPE", GLFW_KEY_ESCAPE},
    { "ENTER", GLFW_KEY_ENTER },
    { "TAB", GLFW_KEY_TAB },
    { "BACKSPACE", GLFW_KEY_BACKSPACE },
    { "INSERT", GLFW_KEY_INSERT },
    { "DELETE", GLFW_KEY_DELETE },
    { "RIGHT", GLFW_KEY_RIGHT },
    { "LEFT", GLFW_KEY_LEFT },
    { "DOWN", GLFW_KEY_DOWN },
    { "UP", GLFW_KEY_UP },
    { "PAGE_UP", GLFW_KEY_PAGE_UP },
    { "PAGE_DOWN", GLFW_KEY_PAGE_DOWN },
    { "HOME", GLFW_KEY_HOME },
    { "END", GLFW_KEY_END },
    { "CAPS_LOCK", GLFW_KEY_CAPS_LOCK },
    { "SCROLL_LOCK", GLFW_KEY_SCROLL_LOCK },
    { "NUM_LOCK", GLFW_KEY_NUM_LOCK },
    { "PRINT_SCREEN", GLFW_KEY_PRINT_SCREEN },
    { "PAUSE", GLFW_KEY_PAUSE },
    { "F1", GLFW_KEY_F1 },
    { "F2", GLFW_KEY_F2 },
    { "F3", GLFW_KEY_F3 },
    { "F4", GLFW_KEY_F4 },
    { "F5", GLFW_KEY_F5 },
    { "F6", GLFW_KEY_F6 },
    { "F7", GLFW_KEY_F7 },
    { "F8", GLFW_KEY_F8 },
    { "F9", GLFW_KEY_F9 },
    { "F10", GLFW_KEY_F10 },
    { "F11", GLFW_KEY_F11 },
    { "F12", GLFW_KEY_F12 },
    { "F13", GLFW_KEY_F13 },
    { "F14", GLFW_KEY_F14 },
    { "F15", GLFW_KEY_F15 },
    { "F16", GLFW_KEY_F16 },
    { "F17", GLFW_KEY_F17 },
    { "F18", GLFW_KEY_F18 },
    { "F19", GLFW_KEY_F19 },
    { "F20", GLFW_KEY_F20 },
    { "F21", GLFW_KEY_F21 },
    { "F22", GLFW_KEY_F22 },
    { "F23", GLFW_KEY_F23 },
    { "F24", GLFW_KEY_F24 },
    { "F25", GLFW_KEY_F25 },
    { "KP_0", GLFW_KEY_KP_0 },
    { "KP_1", GLFW_KEY_KP_1 },
    { "KP_2", GLFW_KEY_KP_2 },
    { "KP_3", GLFW_KEY_KP_3 },
    { "KP_4", GLFW_KEY_KP_4 },
    { "KP_5", GLFW_KEY_KP_5 },
    { "KP_6", GLFW_KEY_KP_6 },
    { "KP_7", GLFW_KEY_KP_7 },
    { "KP_8", GLFW_KEY_KP_8 },
    { "KP_9", GLFW_KEY_KP_9 },
    { "KP_DECIMAL", GLFW_KEY_KP_DECIMAL },
    { "KP_DIVIDE", GLFW_KEY_KP_DIVIDE },
    { "KP_MULTIPLY", GLFW_KEY_KP_MULTIPLY },
    { "KP_SUBTRACT", GLFW_KEY_KP_SUBTRACT },
    { "KP_ADD", GLFW_KEY_KP_ADD },
    { "KP_ENTER", GLFW_KEY_KP_ENTER },
    { "KP_EQUAL", GLFW_KEY_KP_EQUAL },
    { "LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT },
    { "LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL },
    { "LEFT_ALT", GLFW_KEY_LEFT_ALT },
    { "LEFT_SUPER", GLFW_KEY_LEFT_SUPER },
    { "RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT },
    { "RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL },
    { "RIGHT_ALT", GLFW_KEY_RIGHT_ALT },
    { "RIGHT_SUPER", GLFW_KEY_RIGHT_SUPER },
    { "MENU", GLFW_KEY_MENU },
};

const std::unordered_map<std::string, int> InputHandler::name_to_mouse_key = {
    { "MOUSE_1", GLFW_MOUSE_BUTTON_1 },
    { "MOUSE_2", GLFW_MOUSE_BUTTON_2 },
    { "MOUSE_3", GLFW_MOUSE_BUTTON_3 },
    { "MOUSE_4", GLFW_MOUSE_BUTTON_4 },
    { "MOUSE_5", GLFW_MOUSE_BUTTON_5 },
    { "MOUSE_6", GLFW_MOUSE_BUTTON_6 },
    { "MOUSE_7", GLFW_MOUSE_BUTTON_7 },
    { "MOUSE_8", GLFW_MOUSE_BUTTON_8 },
    { "MOUSE_LEFT", GLFW_MOUSE_BUTTON_LEFT },
    { "MOUSE_RIGHT", GLFW_MOUSE_BUTTON_RIGHT },
    { "MOUSE_MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE },
};

const std::unordered_map<std::string, InputAxis> InputHandler::name_to_input_axis = {
    { "MOUSE_X", InputAxis::MOUSE_X },
    { "MOUSE_Y", InputAxis::MOUSE_Y },
    { "GAMEPAD_LEFT_X", InputAxis::GAMEPAD_LEFT_X },
    { "GAMEPAD_LEFT_Y", InputAxis::GAMEPAD_LEFT_Y },
    { "GAMEPAD_RIGHT_X", InputAxis::GAMEPAD_RIGHT_X },
    { "GAMEPAD_RIGHT_Y", InputAxis::GAMEPAD_RIGHT_Y },
    { "GAMEPAD_LEFT_TRIGGER", InputAxis::GAMEPAD_LEFT_TRIGGER },
    { "GAMEPAD_RIGHT_TRIGGER", InputAxis::GAMEPAD_RIGHT_TRIGGER },
};
