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

    DynamicSolidTree() : aabb_tree(ivec2(2)) {}

    // AABB boxes should are automatically expanded by this amount before inserting into the tree.
    static constexpr int slack_margin = 2;

    // Here, if `entity_filter` returns false, the entity is ignored.
    // `entity_callback` defaults to `return collide(ivec2{});`. You can return false unconditionally to ignore this entity,
    // or you can change the offset before calling `collide` to imaginarily offset that entity.

    using EntityFilterFunc = std::function<bool(const Game::Entity &e)>;
    using EntityCallbackFunc = std::function<bool(Game::Entity &e, std::function<bool(ivec2 offset)> collide)>;

    [[nodiscard]] bool BoxCollisionTest(
        irect2 box,
        EntityFilterFunc entity_filter = nullptr,
        EntityCallbackFunc entity_callback = nullptr
    ) const;

    [[nodiscard]] bool ShipBlocksCollisionTest(
        const ShipPartBlocks &ship,
        ivec2 ship_offset,
        EntityFilterFunc entity_filter = nullptr,
        EntityCallbackFunc entity_callback = nullptr
    ) const;
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
        aabb = aabb.expand(DynamicSolidTree::slack_margin);

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

    struct Gravity
    {
        float speed = 0;
        float speed_comp = 0;
        // Which direction the gravity was applied last. When it changes, we reset the velocity.
        ivec2 last_dir;

        bool enabled = true;
    };
    Gravity gravity;

    bool can_move = true;


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

    [[nodiscard]] ivec2 RenderOffset() const;
    void PreRender() const override;
    void Render() const override;
};

struct ShipPartPiston :
    Tickable, DynamicSolid,
    Renderable, PreRenderable,
    BasicShipPart,
    Game::LinkOne<"a">, Game::LinkOne<"b">
{
    IMP_STANDALONE_COMPONENT(Game)

    bool is_vertical = false;

    ivec2 pos_relative_to_a; // The top/left end of the EDGE attaching this to A.
    ivec2 pos_relative_to_b; // The top/left end of the EDGE attaching this to B.

    irect2 last_rect;

    // This oscilates when moving, to determine which side to move next.
    bool dir_flip_flop = false;

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

    // Returns distance to `point`, with some metric suitable for picking pistons with mouse.
    // Currently this uses 8-way distance.
    [[nodiscard]] int DistanceToPoint(ivec2 point) const;

    bool BoxCollisionTest(irect2 box) const override
    {
        return last_rect.touches(box);
    }

    bool ShipBlocksCollisionTest(const ShipPartBlocks &ship, ivec2 ship_offset) const override
    {
        return ship.map.CollidesWithBox(last_rect - ship.pos - ship_offset);
    }

    enum class ExtendRetractStatus
    {
        ok, // Successfully changed length.
        ok_pushed, // Successfully changed length, but had to push something.
        stuck, // Can't move, something solid is interfering.
        at_min_length, // Can't move, already at minimal length.
        at_max_length, // Can't move, already at maximal length.
        cycle, // Can't move, the parts form a cycle that includes this piston.
    };
    [[nodiscard]] static constexpr bool ExtendOrRetractWasSuccessful(ExtendRetractStatus status)
    {
        return status == ExtendRetractStatus::ok || status == ExtendRetractStatus::ok_pushed;
    }

    // Try to extend or retract the piston by one pixel.
    ExtendRetractStatus ExtendOrRetract(bool extend, int max_length);

    void Tick() override;

    void RenderLow(bool pre) const;
    void PreRender() const override;
    void Render() const override;
};

// Applies configurable gravity to all ships.
struct GravityController :
    Tickable
{
    IMP_STANDALONE_COMPONENT(Game)

    // Reloading the level disables this.
    // The editor enables it when done editing.
    bool enabled = true;

    ivec2 dir = ivec2(0,1);
    float acc = 0.1f;
    float max_speed = 2;

    void Tick() override;
};


// Split this ship into multiple entities, by continuous map regions.
// This entity itself is then destroyed.
// `finalize_blocks` is called on every new blocks entity.
void DecomposeToComponentsAndDelete(ShipPartBlocks &self, std::function<void(ShipPartBlocks &blocks)> finalize_blocks = nullptr);

struct ConnectedShipParts
{
    // This can be true only if `skip_piston_direction` isn't null.
    // If true, we have a cycle in the graph, and the results don't make sense.
    bool cant_skip_because_of_cycle = false;

    phmap::flat_hash_set<ShipPartBlocks *> blocks;
    phmap::flat_hash_set<ShipPartPiston *> pistons;

    // The ids of all entities from `blocks` and `pistons`.
    // Note that the user can desync those. This variable is only used by the lambda below.
    phmap::flat_hash_set<Game::Id> entity_ids;

    void AddSingleBlocksObject(Game::Id id)
    {
        blocks.insert(&game.get(id).get<ShipPartBlocks>());
        entity_ids.insert(id);
    }

    void Append(const ConnectedShipParts &other)
    {
        blocks.insert(other.blocks.begin(), other.blocks.end());
        pistons.insert(other.pistons.begin(), other.pistons.end());
        entity_ids.insert(other.entity_ids.begin(), other.entity_ids.end());
    }

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
// If `blocks_or_piston` is set to an entity pointer which is neither blocks nor a piston, throws.
[[nodiscard]] ConnectedShipParts FindConnectedShipParts(std::variant<ShipPartBlocks *, ShipPartPiston *, Game::Entity *> blocks_or_piston, std::optional<bool> skip_piston_direction = {});

struct PushAction
{
    // This should be initially set to the blocks we're starting with.
    ConnectedShipParts all_pushed_parts;

    // This gets set to true if we pushed at least something in addition to ourselves.
    // The value only makes sense if `CollideShipParts` returned false.
    bool at_least_one_pushed = false;
};
struct PushParams
{
    PushAction *result = nullptr;

    // If this returns false for an entity we're trying to push, the push fails.
    // If you want to blacklist a ship, you need to blacklist all its components yourself.
    DynamicSolidTree::EntityFilterFunc allowed_entities;
};

// Checks collision for ship `parts` at `offset`, against `map` and/or `tree`.
// Both maps and the tree is optional.
// If `entity_filter` is specified and returns false, that entity is ignored.
// If `extra` is `PushParams`, will try to push entities along, and return false on success.
//   Then, on success, it will be updated with a list of all things you need to push. Move them, instead of `parts`.
// If `extra` is a callback, it will be used.
[[nodiscard]] bool CollideShipParts(
    const ConnectedShipParts &parts, ivec2 offset,
    const MapObject *map, const DynamicSolidTree *tree,
    DynamicSolidTree::EntityFilterFunc entity_filter = nullptr,
    std::variant<std::monostate, const PushParams *, DynamicSolidTree::EntityCallbackFunc> extra = {}
);

// Adds parts that are dragged because they're sitting on the moving ones.
// Has no effect without gravity.
// Returns a superset of the passed `parts`.
[[nodiscard]] ConnectedShipParts AddDraggedParts(const ConnectedShipParts &parts, ivec2 offset, const MapObject *map, const DynamicSolidTree *tree);

// Moves the `parts` by the `offset`, and updates their AABB boxes.
void MoveShipParts(const ConnectedShipParts &parts, ivec2 offset);

// Move ships by offset `gravity`, either all of them or those matching `fitler.
void MoveShipsByGravity(ivec2 dir, float acc, float max_speed, DynamicSolidTree::EntityFilterFunc filter = nullptr);
