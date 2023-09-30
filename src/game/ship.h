#pragma once

#include "game/entities.h"
#include "game/main.h"
#include "game/map.h"

struct BasicShipPart
{
    virtual ~BasicShipPart() = default;
};

struct ShipPartBlocks : Tickable, Renderable, BasicShipPart, Game::LinkMany<"pistons">
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
    Map<ShipGrid> map;

    void Tick() override;
    void Render() const override;
};

struct ShipPartPiston : Renderable, BasicShipPart, Game::LinkOne<"a">, Game::LinkOne<"b">
{
    IMP_STANDALONE_COMPONENT(Game)

    bool is_vertical = false;

    ivec2 pos_relative_to_a;

    // The top/left end of the EDGE attaching this to B.
    ivec2 pos_relative_to_b;


    void Render() const override;
};

// Split this ship into multiple entities, by continuous map regions.
// This entity itself is then destroyed.
void DecomposeToComponentsAndDelete(ShipPartBlocks &self);

struct ConnectedShipParts
{
    // This can be true only if `skip_piston_direction` isn't null.
    // If true, we have a cycle in the graph, and the results don't make sense.
    bool cant_skip_because_of_cycle = false;

    phmap::flat_hash_set<ShipPartBlocks *> blocks;
    phmap::flat_hash_set<ShipPartPiston *> pistons;

};
// Finds all connected parts of a ship, starting from `blocks_or_piston`.
// `skip_piston_direction` can only be non-null if we start from a piston. Then `false` means we skip direction A of initial piston, and `true` means we skip B.
// If `skip_piston_direction` is set, but there's a cycle that includes this piston, the execution aborts and `cant_skip_because_of_cycle` is returned.
[[nodiscard]] ConnectedShipParts FindConnectedShipParts(std::variant<ShipPartBlocks *, ShipPartPiston *> blocks_or_piston, std::optional<bool> skip_piston_direction = {});
