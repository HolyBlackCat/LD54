#pragma once

#include "game/entities.h"
#include "game/main.h"
#include "game/map.h"
#include "utils/aabb_tree.h"

struct ShipPartBlocks;

struct DynamicSolidTree
{
    IMP_STANDALONE_COMPONENT(Game)

    using Tree = AabbTree<ivec2, Game::Id>;
    Tree aabb_tree;

    DynamicSolidTree() : aabb_tree(ivec2(1)) {}

    [[nodiscard]] bool BoxCollisionTest(irect2 box, std::function<bool(const Game::Entity &e)> entity_filter = nullptr) const;
    [[nodiscard]] bool ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset, std::function<bool(const Game::Entity &e)> entity_filter = nullptr) const;
};

struct DynamicSolid
{
    IMP_COMPONENT(Game)
    virtual ~DynamicSolid() = default;

    [[nodiscard]] virtual bool BoxCollisionTest(irect2 box) const = 0;
    [[nodiscard]] virtual bool ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset) const = 0;

    void _deinit(Game::Controller &, Game::Entity &)
    {
        ResetAabb();
    }

    [[nodiscard]] bool HasAabb() const
    {
        return node_index != DynamicSolidTree::Tree::null_index;
    }

    void SetAabb(irect2 aabb)
    {
        if (HasAabb())
        {
            game.get<DynamicSolidTree>()->aabb_tree.ModifyNode(node_index, aabb, ivec2(0));
        }
        else
        {
            auto &tree = game.get<DynamicSolidTree>()->aabb_tree;
            node_index = tree.AddNode(aabb);
            tree.GetNodeUserData(node_index) = dynamic_cast<Game::Entity &>(*this).id();
        }
    }

    void ResetAabb()
    {
        if (HasAabb())
        {
            // The tree can be missing if we're destroying the controller right now, and `_deinit()` calls this.
            if (auto tree = game.get<DynamicSolidTree>().get_opt())
                tree->aabb_tree.RemoveNode(node_index);
            node_index = DynamicSolidTree::Tree::null_index;
        }
    }

  private:
    DynamicSolidTree::Tree::NodeIndex node_index = DynamicSolidTree::Tree::null_index;
};

struct BasicShipPart
{
    IMP_COMPONENT(Game)
    virtual ~BasicShipPart() = default;
};

struct ShipPartBlocks :
    Tickable, DynamicSolid,
    Renderable, PreRenderable,
    BasicShipPart,
    Game::LinkMany<"pistons">
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
    Map<ShipGrid> map;

    irect2 CalculateRect() const
    {
        return pos.rect_size(map.cells.size() * ShipGrid::tile_size);
    }

    void UpdateAabb()
    {
        SetAabb(CalculateRect());
    }

    bool BoxCollisionTest(irect2 box) const override
    {
        return map.CollidesWithBox(box - pos);
    }

    bool ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset) const override
    {
        return ship.map.CollidesWithMap(map, ship.pos + ship_offset - pos);
    }

    void Tick() override;
    void PreRender() const override;
    void Render() const override;
};

struct ShipPartPiston :
    Tickable, DynamicSolid,
    Renderable,
    BasicShipPart,
    Game::LinkOne<"a">, Game::LinkOne<"b">
{
    IMP_STANDALONE_COMPONENT(Game)

    bool is_vertical = false;

    ivec2 pos_relative_to_a; // The top/left end of the EDGE attaching this to A.
    ivec2 pos_relative_to_b; // The top/left end of the EDGE attaching this to B.

    irect2 last_rect;

    irect2 CalculateRect() const
    {
        ivec2 a = game.get_link<"a">(*this).get<ShipPartBlocks>().pos + pos_relative_to_a;
        ivec2 b = game.get_link<"b">(*this).get<ShipPartBlocks>().pos + pos_relative_to_b;
        b += ivec2::axis(!is_vertical, ShipGrid::tile_size);
        return a.rect_to(b);
    }

    void UpdateAabb()
    {
        last_rect = CalculateRect();
        SetAabb(last_rect);
    }

    bool BoxCollisionTest(irect2 box) const override
    {
        return last_rect.touches(box);
    }

    bool ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset) const override
    {
        return ship.map.CollidesWithBox(last_rect - ship.pos - ship_offset);
    }

    void Tick() override;
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

    // The ids of all entities from `blocks` and `pistons`.
    phmap::flat_hash_set<Game::Id> entity_ids;

    // Returns a predicate on `Game::Entity &` that tests that it's not in `entity_ids`.
    auto LambdaNoSuchEntityHere()
    {
        return [this](const Game::Entity &e)
        {
            return !entity_ids.contains(e.id());
        };
    }
};
// Finds all connected parts of a ship, starting from `blocks_or_piston`.
// `skip_piston_direction` can only be non-null if we start from a piston. Then `false` means we skip direction A of initial piston, and `true` means we skip B.
// If `skip_piston_direction` is set, but there's a cycle that includes this piston, the execution aborts and `cant_skip_because_of_cycle` is returned.
[[nodiscard]] ConnectedShipParts FindConnectedShipParts(std::variant<ShipPartBlocks *, ShipPartPiston *> blocks_or_piston, std::optional<bool> skip_piston_direction = {});

// Checks collision for ship `parts` at `offset`, against `map` and/or `tree`.
// Both maps and the tree is optional.
[[nodiscard]] bool CollideShipParts(const ConnectedShipParts &parts, ivec2 offset, const MapObject *map, const DynamicSolidTree *tree, std::function<bool(const Game::Entity &e)> entity_filter = nullptr);

// Moves the `parts` by the `offset`, and updates their AABB boxes.
void MoveShipParts(const ConnectedShipParts &parts, ivec2 offset);
