#include "Theme.hpp"

namespace calcium::ui {

const Theme& Theme::dark() {
    static Theme instance;   // default-constructed above IS the dark theme
    return instance;
}

} // namespace calcium::ui
