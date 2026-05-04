#include "ActivationKeybind.h"
#include "Input.h"

ActivationKeybind::~ActivationKeybind()
{
    Input::f([&](auto& i) { i.UnregisterKeybind(this); });
}

void ActivationKeybind::Bind()
{
    for (const auto& kc : keyCombos_)
        if (NotNone(kc.key()))
            Input::i().RegisterKeybind(this, kc);
}

void ActivationKeybind::Rebind()
{
    Input::i().UnregisterKeybind(this);
    Bind();
}