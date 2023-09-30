#include "draw.h"

#include "game/main.h"

namespace Draw
{
    void Rect(irect2 rect, fvec3 color, float alpha, float beta)
    {
        r.iquad(rect.a, ivec2(rect.size().x, 1)).color(color).alpha(alpha).beta(beta);
        r.iquad(rect.a with(.y += 1), ivec2(1, rect.size().y-2)).color(color).alpha(alpha).beta(beta);
        r.iquad(rect.a with(.y += rect.size().y - 1), ivec2(rect.size().x, 1)).color(color).alpha(alpha).beta(beta);
        r.iquad(rect.a + ivec2(rect.size().x - 1, 1), ivec2(1, rect.size().y-2)).color(color).alpha(alpha).beta(beta);
    }
}
