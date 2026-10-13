#include "ui/core/theme.hpp"

namespace ui {

const Theme& Theme::Default() {
    static const Theme theme;
    return theme;
}

} // namespace ui