#include "Keybind.h"

#include <sstream>

#include "ConfigurationFile.h"
#include "Utility.h"

// ── Constructors ──────────────────────────────────────────────────────────────

Keybind::Keybind(std::string_view nickname, std::string_view displayName, std::string_view category, ScanCode key, Modifier mod, bool saveToConfig)
    : nickname_(nickname)
    , displayName_(displayName)
    , category_(category)
    , saveToConfig_(saveToConfig)
{
    keyCombo({ key, mod });
    languageChangeCallbackID_ = GetBaseCore().languageChangeEvent().AddCallback([this]() { UpdateDisplayString(); });
}

Keybind::Keybind(std::string_view nickname, std::string_view displayName, std::string_view category)
    : nickname_(nickname)
    , displayName_(displayName)
    , category_(category)
{
    if (auto keys = INIConfigurationFile::i().ini().GetValue("Keybinds.2", nickname_.c_str()))
    {
        ParseConfig(keys);
    }
    else
    {
        auto keys2 = INIConfigurationFile::i().ini().GetValue("Keybinds", nickname_.c_str());
        if (keys2)
            ParseKeys(keys2);
        else
            keyCombo({ ScanCode::None, Modifier::None });
    }
    languageChangeCallbackID_ = GetBaseCore().languageChangeEvent().AddCallback([this]() { UpdateDisplayString(); });
}

Keybind::~Keybind()
{
    GetBaseCore().languageChangeEvent().RemoveCallback(std::move(languageChangeCallbackID_));
}

// ── Single-combo setters (backward compat) ────────────────────────────────────

void Keybind::keyCombo(const KeyCombo& ks)
{
    key_ = ks.key();
    mod_ = ks.mod();
    if (keyCombos_.empty())
        keyCombos_.push_back(ks);
    else
        keyCombos_[0] = ks;
    ApplyKeys();
}

void Keybind::key(ScanCode key)
{
    keyCombo({ key, mod_ });
}

void Keybind::modifier(Modifier mod)
{
    keyCombo({ key_, mod });
}

void Keybind::keyCombos(std::vector<KeyCombo> kcs)
{
    keyCombos_ = std::move(kcs);
    if (!keyCombos_.empty())
    {
        key_ = keyCombos_[0].key();
        mod_ = keyCombos_[0].mod();
    }
    else
    {
        key_ = ScanCode::None;
        mod_ = Modifier::None;
    }
    ApplyKeys();
}

// ── Legacy ParseKeys (old VK-based config) ────────────────────────────────────

void Keybind::ParseKeys(const char* keys)
{
    key_ = ScanCode::None;
    mod_ = Modifier::None;
    keyCombos_.clear();

    if (strnlen_s(keys, 256) > 0)
    {
        std::stringstream ss(keys);
        ScanCode          parsedKey = ScanCode::None;
        Modifier          parsedMod = Modifier::None;

        while (ss.good())
        {
            std::string substr;
            std::getline(ss, substr, ',');
            auto val      = std::stoi(substr);
            val           = MapVirtualKeyA(val, MAPVK_VK_TO_VSC);
            ScanCode code = ScanCode(u32(val));

            if (IsModifier(code))
            {
                if (parsedKey != ScanCode::None)
                    parsedMod = parsedMod | ToModifier(code);
                else
                    parsedKey = code;
            }
            else
            {
                if (IsModifier(parsedKey))
                    parsedMod = parsedMod | ToModifier(parsedKey);
                parsedKey = code;
            }
        }
        keyCombos_.push_back({ parsedKey, parsedMod });
    }

    if (!keyCombos_.empty())
    {
        key_ = keyCombos_[0].key();
        mod_ = keyCombos_[0].mod();
    }
    ApplyKeys();
}

// ── New config format: "sc,mod | sc,mod | sc,mod" ────────────────────────────

void Keybind::ParseConfig(const char* keys)
{
    keyCombos_.clear();

    // Split on '|' to get individual combos
    std::string              input(keys);
    std::vector<std::string> parts;
    {
        size_t start = 0, pos;
        while ((pos = input.find('|', start)) != std::string::npos)
        {
            parts.push_back(input.substr(start, pos - start));
            start = pos + 1;
        }
        parts.push_back(input.substr(start));
    }

    for (auto& part : parts)
    {
        // Each part is "scancode, modifier"
        std::vector<std::string> k;
        SplitString(part.c_str(), ",", std::back_inserter(k));
        if (k.empty())
            continue;

        ScanCode sc = ScanCode(u32(std::stoi(k[0])));
        Modifier m  = (k.size() >= 2) ? Modifier(u16(std::stoi(k[1]))) : Modifier::None;
        if (sc != ScanCode::None)
            keyCombos_.push_back({ sc, m });
    }

    if (keyCombos_.empty())
    {
        key_ = ScanCode::None;
        mod_ = Modifier::None;
    }
    else
    {
        key_ = keyCombos_[0].key();
        mod_ = keyCombos_[0].mod();
    }
    ApplyKeys();
}

void Keybind::ApplyKeys()
{
    UpdateDisplayString();

    if (saveToConfig_)
    {
        auto& cfg = INIConfigurationFile::i();

        if (!keyCombos_.empty() && keyCombos_[0].key() != ScanCode::None)
        {
            // Serialize all combos joined by " | "
            std::string settingValue;
            for (size_t i = 0; i < keyCombos_.size(); ++i)
            {
                if (i > 0)
                    settingValue += " | ";
                settingValue += std::to_string(u32(keyCombos_[i].key())) + ", " + std::to_string(u32(keyCombos_[i].mod()));
            }
            cfg.ini().SetValue("Keybinds.2", nickname_.c_str(), settingValue.c_str());
        }
        else
        {
            cfg.ini().DeleteValue("Keybinds.2", nickname_.c_str(), nullptr);
        }
        cfg.Save();
    }
}

bool Keybind::matches(const KeyCombo& ks) const
{
    for (const auto& kc : keyCombos_)
    {
        if (kc.key() == ks.key() && (kc.mod() & ks.mod()) == kc.mod())
            return true;
    }
    return false;
}

void Keybind::UpdateDisplayString(std::optional<std::vector<KeyCombo>> kcs) const
{
    const auto& combos = kcs ? *kcs : keyCombos_;

    if (combos.empty() || combos[0].key() == ScanCode::None)
    {
        keysDisplayString_[0] = '\0';
        return;
    }

    std::wstring result;
    for (size_t i = 0; i < combos.size(); ++i)
    {
        if (i > 0)
            result += L" , ";
        const auto& k = combos[i];
        if (NotNone(k.mod() & Modifier::Ctrl))
            result += L"CTRL + ";
        if (NotNone(k.mod() & Modifier::Alt))
            result += L"ALT + ";
        if (NotNone(k.mod() & Modifier::Shift))
            result += L"SHIFT + ";
        result += GetScanCodeName(k.key());
    }

    Log::i().Print(Severity::Debug, L"Setting keybind '{}' display '{}'", utf8_decode(nickname()), result);
    strcpy_s(keysDisplayString_.data(), keysDisplayString_.size(), utf8_encode(result).c_str());
}