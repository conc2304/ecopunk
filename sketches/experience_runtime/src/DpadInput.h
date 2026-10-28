#pragma once

// DpadInput — the device-neutral five-way input (D-pad Up/Down/Left/Right +
// centre OK, plus Back for the keyboard's Esc). OF-free. InputRouter produces
// it from keys; a hardware D-pad produces the same values.
enum class DpadInput { Up, Down, Left, Right, Ok, Back };
