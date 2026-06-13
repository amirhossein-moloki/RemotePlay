#pragma once
#include <vector>
#include <string>

namespace Client {

class Overlay {
public:
    static void Render();
    static void Toggle() { m_visible = !m_visible; }
    static bool IsVisible() { return m_visible; }

private:
    static bool m_visible;
};

} // namespace Client
