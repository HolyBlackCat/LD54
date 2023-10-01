#pragma once

#include "utils/mat.h"

namespace Draw
{
    void Rect(irect2 rect, fvec3 color, float alpha = 1, float beta = 1);

    enum class Color
    {
        selection,
        danger,
    };
    [[nodiscard]] fvec3 ColorFromEnum(Color color);

    void StyledRect(irect2 rect, Color color, float alpha = 1, bool filled = false);
}
