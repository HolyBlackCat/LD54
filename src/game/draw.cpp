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

    fvec3 ColorFromEnum(Color color)
    {
        switch (color)
        {
          case Color::selection:
            return fvec3(0.7f,1,0);
          case Color::danger:
            return fvec3(1,0,0.25f);
          case Color::neutral:
            return fvec3(1,1,1);
        }
        return {};
    }

    void StyledRect(irect2 rect, Color color, float alpha, bool filled)
    {
        Draw::Rect(rect.expand(2), fvec3(0,0,0), 0.57f * alpha);

        irect2 inner_rect = rect.expand(1);
        fvec3 inner_rect_color = ColorFromEnum(color);
        float inner_rect_alpha = alpha;
        float inner_rect_beta = 0.55f;

        if (filled)
            r.iquad(inner_rect).color(inner_rect_color).alpha(inner_rect_alpha).beta(inner_rect_beta);
        else
            Draw::Rect(inner_rect, inner_rect_color, inner_rect_alpha, inner_rect_beta);
    }
}
