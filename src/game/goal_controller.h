#pragma once

#include "game/entities.h"

struct GoalController : Tickable, FadeRenderable
{
    IMP_STANDALONE_COMPONENT(Game)

    phmap::flat_hash_set<Game::Id> goal_blocks;

    bool condition_met = false;

    float fade_timer = 1;
    int initial_delay = 15;

    [[nodiscard]] bool ShouldSwitchToNextLevel() const;

    void Tick() override;
    void FadeRender() const override;
};
