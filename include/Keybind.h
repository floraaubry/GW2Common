#pragma once
#include <array>
#include <vector>

#include "Common.h"
#include "Input.h"
#include "KeyCombo.h"

class Keybind
{
public:
    Keybind(std::string_view nickname, std::string_view displayName, std::string_view category, KeyCombo ks, bool saveToConfig)
        : Keybind(nickname, displayName, category, ks.key(), ks.mod(), saveToConfig)
    {}

    Keybind(std::string_view nickname, std::string_view displayName, std::string_view category, ScanCode key, Modifier mod, bool saveToConfig);
    Keybind(std::string_view nickname, std::string_view displayName, std::string_view category);

    virtual ~Keybind();

    // Primary combo (slot 0)
    KeyCombo keyCombo() const
    {
        return keyCombos_.empty() ? KeyCombo{ ScanCode::None, Modifier::None } : keyCombos_[0];
    }
    ScanCode key() const
    {
        return keyCombo().key();
    }
    Modifier modifier() const
    {
        return keyCombo().mod();
    }

    // Multi-combo access
    const std::vector<KeyCombo>& keyCombos() const
    {
        return keyCombos_;
    }
    void                             keyCombos(std::vector<KeyCombo> kcs);

    // Single-combo setters (backward compat — replaces slot 0)
    void                             keyCombo(const KeyCombo& ks);
    void                             key(ScanCode key);
    void                             modifier(Modifier mod);

    void                             ParseKeys(const char* keys);
    void                             ParseConfig(const char* keys);

    [[nodiscard]] const std::string& displayName() const
    {
        return displayName_;
    }
    void displayName(const std::string& n)
    {
        displayName_ = n;
    }

    [[nodiscard]] const std::string& nickname() const
    {
        return nickname_;
    }
    void nickname(const std::string& n)
    {
        nickname_ = n;
    }

    [[nodiscard]] bool isSet() const
    {
        return !keyCombos_.empty() && keyCombos_[0].key() != ScanCode::None;
    }

    [[nodiscard]] const char* keysDisplayString() const
    {
        return keysDisplayString_.data();
    }
    [[nodiscard]] char* keysDisplayString()
    {
        return keysDisplayString_.data();
    }
    [[nodiscard]] size_t keysDisplayStringSize() const
    {
        return keysDisplayString_.size();
    }

    [[nodiscard]] bool matches(const KeyCombo& ks) const;

    i32                keyCount() const
    {
        if (keyCombos_.empty() || keyCombos_[0].key() == ScanCode::None)
            return 0;
        const auto& kc = keyCombos_[0];
        return 1 + (NotNone(kc.mod() & Modifier::Ctrl) ? 1 : 0) + (NotNone(kc.mod() & Modifier::Shift) ? 1 : 0) + (NotNone(kc.mod() & Modifier::Alt) ? 1 : 0);
    }

    void UpdateDisplayString(std::optional<std::vector<KeyCombo>> kcs = std::nullopt) const;

protected:
    virtual void                  ApplyKeys();

    std::string                   displayName_, nickname_, category_;
    // Legacy mirrors of keyCombos_[0] — kept for ActivationKeybind compat
    ScanCode                      key_          = ScanCode::None;
    Modifier                      mod_          = Modifier::None;
    bool                          saveToConfig_ = true;
    EventCallbackHandle           languageChangeCallbackID_;

    std::vector<KeyCombo>         keyCombos_;
    mutable std::array<char, 256> keysDisplayString_{};
};

namespace ImGui
{
void KeybindInput(Keybind& keybind, Keybind** keybindBeingModified, const char* tooltip, bool allowMultiple = true);
}