#include "ship.h"

static ivec2 GlobalFloatingOffset()
{
    int offsets[] = {1,0};
    return ivec2(0, offsets[window.Ticks() / 130 % std::size(offsets)]);
}

bool DynamicSolidTree::BoxCollisionTest(
    irect2 box,
    EntityFilterFunc entity_filter,
    EntityCallbackFunc entity_callback
) const
{
    return aabb_tree.CollideAabb(box, [&](Tree::NodeIndex node_index)
    {
        auto &e = game.get(aabb_tree.GetNodeUserData(node_index));
        if (entity_filter && !entity_filter(e))
            return false;

        auto Collide = [&](ivec2 offset)
        {
            return e.get<DynamicSolid>().BoxCollisionTest(box - offset);
        };
        if (entity_callback)
            return entity_callback(e, Collide);
        else
            return Collide(ivec2{});
    });
}

bool DynamicSolidTree::ShipBlocksCollisionTest(
    const ShipPartBlocks &ship,
    ivec2 ship_offset,
    EntityFilterFunc entity_filter,
    EntityCallbackFunc entity_callback
) const
{
    return aabb_tree.CollideAabb(ship.CalculateRect() + ship_offset, [&](Tree::NodeIndex node_index)
    {
        auto &e = game.get(aabb_tree.GetNodeUserData(node_index));
        if (entity_filter && !entity_filter(e))
            return false;

        auto Collide = [&](ivec2 offset)
        {
            return e.get<DynamicSolid>().ShipBlocksCollisionTest(ship, ship_offset - offset);
        };
        if (entity_callback)
            return entity_callback(e, Collide);
        else
            return Collide(ivec2{});
    });
}

void ShipPartBlocks::Tick()
{
    // std::cout << (map.CollidesWithMap(game.get<MapObject>()->map, pos - game.get<MapObject>()->pos)) << '\n';
}

ivec2 ShipPartBlocks::RenderOffset() const
{
    if (can_move && (!gravity.enabled || !game.get<GravityController>()->emerald_enabled))
        return GlobalFloatingOffset();

    return {};
}

void ShipPartBlocks::PreRender() const
{
    map.Render<TileDrawMethods::RenderMode::pre>(game.get<Camera>()->pos - pos - RenderOffset());
}

void ShipPartBlocks::Render() const
{
    // Bounding box:
    // r.iquad(pos - game.get<Camera>()->pos, map.cells.size() * ShipGrid::tile_size).color(fvec3(0)).alpha(0.1f);

    map.Render<TileDrawMethods::RenderMode::normal>(game.get<Camera>()->pos - pos - RenderOffset());

    // ID:
    // r.itext(pos - game.get<Camera>()->pos + map.cells.size() * ShipGrid::tile_size / 2, Graphics::Text(Fonts::main, FMT("{}", dynamic_cast<const Game::Entity &>(*this).id().get_value()))).align(ivec2(0)).color(fvec3(1,0,0));
}

int ShipPartPiston::DistanceToPoint(ivec2 point) const
{
    ivec2 a = game.get_link<"a">(*this).get<ShipPartBlocks>().pos + pos_relative_to_a + ivec2::axis(!is_vertical, ShipGrid::tile_size / 2);
    int relative_pos = point[is_vertical] - a[is_vertical];
    int length = game.get_link<"b">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_b[is_vertical] - a[is_vertical];

    int ret = clamp_min(abs(point[!is_vertical] - a[!is_vertical]) - ShipGrid::tile_size / 2);
    if (relative_pos < 0)
        clamp_var_min(ret, -relative_pos);

    if (relative_pos > length)
        clamp_var_min(ret, relative_pos - length);

    return ret;
}

ShipPartPiston::ExtendRetractStatus ShipPartPiston::ExtendOrRetract(bool extend, int max_length)
{
    constexpr int min_length = ShipGrid::tile_size;

    int current_length = (game.get_link<"b">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_b[is_vertical])
        - (game.get_link<"a">(*this).get<ShipPartBlocks>().pos[is_vertical] + pos_relative_to_a[is_vertical]);

    if (!extend && current_length <= min_length)
        return ExtendRetractStatus::at_min_length; // At minimal length.
    if (extend && current_length >= max_length)
        return ExtendRetractStatus::at_max_length; // At maximal length.

    ConnectedShipParts parts_a = FindConnectedShipParts(this, true);
    ConnectedShipParts parts_b = FindConnectedShipParts(this, false);

    if (parts_a.cant_skip_because_of_cycle || parts_b.cant_skip_because_of_cycle)
        return ExtendRetractStatus::cycle;

    // Those don't touch `entity_ids`, which is exactly what we want here.
    parts_a.pistons.erase(this);
    parts_b.pistons.erase(this);

    auto map = game.get<MapObject>().get_opt();
    auto tree = game.get<DynamicSolidTree>().get_opt();

    ivec2 offset_a = ivec2::axis(is_vertical, extend ? -1 : 1);
    ivec2 offset_b = ivec2::axis(is_vertical, extend ? 1 : -1);

    PushAction push_program_a;
    PushAction push_program_b;
    PushParams push_params_a{.result = &push_program_a, .allowed_entities = parts_b.LambdaNoSuchEntityHere()};
    PushParams push_params_b{.result = &push_program_b, .allowed_entities = parts_a.LambdaNoSuchEntityHere()};

    bool can_move_a = !CollideShipParts(parts_a, offset_a, map, tree, nullptr, &push_params_a);
    bool can_move_b = !CollideShipParts(parts_b, offset_b, map, tree, nullptr, &push_params_b);

    bool ground_a = false;
    bool ground_b = false;

    ivec2 gravity;
    if (auto con = game.get<GravityController>().get_opt(); con && con->IsEnabled())
        gravity = con->dir;
    if (gravity)
    {
        // Need to exclude both sets for both calls.
        // Imagine if A rubs the ground, but B rubs only A. If the lambdas were different,
        // they'd both have ground flags, which is wrong. Only A should have it in this case.
        ground_a = CollideShipParts(parts_a, gravity, map, tree, [&, l = parts_b.LambdaNoSuchEntityHere()](const Game::Entity &e){if (!l(e)) return false; auto blocks = e.get_opt<ShipPartBlocks>(); if (!blocks) return true; return blocks->gravity.enabled;});
        ground_b = CollideShipParts(parts_b, gravity, map, tree, [&, l = parts_a.LambdaNoSuchEntityHere()](const Game::Entity &e){if (!l(e)) return false; auto blocks = e.get_opt<ShipPartBlocks>(); if (!blocks) return true; return blocks->gravity.enabled;});
    }

    if (!can_move_a && !can_move_b)
        return ExtendRetractStatus::stuck;

    bool move_b = false;
    if (can_move_a != can_move_b)
        move_b = can_move_b;
    else if (ground_a != ground_b)
        move_b = ground_a;
    else if (push_program_a.at_least_one_pushed != push_program_b.at_least_one_pushed)
        move_b = push_program_a.at_least_one_pushed;
    else
        move_b = dir_flip_flop;

    dir_flip_flop = !dir_flip_flop;

    if (move_b)
        MoveShipParts(AddDraggedParts(push_program_b.at_least_one_pushed ? push_program_b.all_pushed_parts : parts_b, offset_b, map, tree), offset_b);
    else
        MoveShipParts(AddDraggedParts(push_program_a.at_least_one_pushed ? push_program_a.all_pushed_parts : parts_a, offset_a, map, tree), offset_a);

    // We exclude this piston from `parts_{a,b}`, so we need to manually update it.
    UpdateAabb();

    return ExtendRetractStatus::ok;
}

void ShipPartPiston::Tick()
{

}

void ShipPartPiston::RenderLow(bool pre) const
{
    constexpr int extra_halfwidth = ShipGrid::tile_size / 2, width = ShipGrid::tile_size * 2, segment_length = ShipGrid::tile_size * 4;

    ivec2 pos_a = game.get_link<"a">(*this).get<ShipPartBlocks>().pos + pos_relative_to_a - game.get<Camera>()->pos - ivec2::axis(!is_vertical, extra_halfwidth);
    ivec2 pos_b = game.get_link<"b">(*this).get<ShipPartBlocks>().pos + pos_relative_to_b - game.get<Camera>()->pos - ivec2::axis(!is_vertical, extra_halfwidth);

    int remaining_pixel_len = pos_b[is_vertical] - pos_a[is_vertical];
    int num_segments = (remaining_pixel_len + segment_length - 1) / segment_length;

    ivec2 offset;
    if (!game.get<GravityController>()->emerald_enabled)
        offset = GlobalFloatingOffset();

    for (int i = 0; i < num_segments; i++)
    {
        ivec2 sprite_size(clamp_max(segment_length, remaining_pixel_len), width);
        remaining_pixel_len -= segment_length;

        auto quad = r.iquad(pos_a + offset + ivec2::axis(is_vertical, segment_length * i),
            "ship_tiles"_image with(= (_.a + ivec2(1,2) * ShipGrid::tile_size + ivec2(segment_length * pre, 0)).rect_size(sprite_size))).pixel_center(fvec2{});
        if (is_vertical)
            quad.flip_x(is_vertical).matrix(ivec2(0,-1).to_rotation_matrix());
    }

    // ID:
    // r.itext(last_rect.center() - game.get<Camera>()->pos, Graphics::Text(Fonts::main, FMT("{}", dynamic_cast<const Game::Entity &>(*this).id().get_value()))).align(ivec2(0)).color(fvec3(1,0,0));
}

void ShipPartPiston::PreRender() const
{
    RenderLow(true);
}
void ShipPartPiston::Render() const
{
    RenderLow(false);
}

void GravityController::Tick()
{
    if (IsEnabled())
        MoveShipsByGravity(dir, acc, max_speed);
}

void DecomposeToComponentsAndDelete(ShipPartBlocks &self, std::function<void(ShipPartBlocks &blocks)> finalize_blocks)
{
    // Whether this tile is not empty and not special for splitting purposes.
    auto IsRegularTile = [&](ShipGrid::Tile tile)
    {
        auto piston = ShipGrid::GetTileInfo(tile).piston;
        return piston == ShipGrid::PistonRelation::solid_attachable || piston == ShipGrid::PistonRelation::solid_non_attachable;
    };

    Array2D<char/*bool*/, int> visited(self.map.cells.size());

    struct QueuedPiston
    {
        bool is_vertical = false;

        ShipPartBlocks *block_a = nullptr;
        // Absolute pixel position of the starting corner.
        ivec2 abs_pixel_pos_a;

        ShipPartBlocks *block_b = nullptr;
        ivec2 abs_pixel_pos_b;
    };
    // Maps block position to pistons ending here.
    phmap::flat_hash_map<ivec2, std::vector<QueuedPiston>> queued_pistons;

    for (ivec2 tile_pos : vector_range(self.map.cells.size()))
    {
        if (IsRegularTile(self.map.cells.safe_nonthrowing_at(tile_pos).tile) && !visited.safe_nonthrowing_at(tile_pos))
        {
            auto &new_part = game.create<ShipPartBlocks>();

            ivec2 new_part_tile_offset = tile_pos;

            auto lambda = [&](auto &lambda, ivec2 abs_tile_pos) -> void
            {
                if (!self.map.cells.bounds().contains(abs_tile_pos))
                    return;
                const ShipGrid::Tile this_tile = self.map.cells.safe_nonthrowing_at(abs_tile_pos).tile;
                if (!IsRegularTile(this_tile))
                    return;
                if (auto &flag = visited.safe_nonthrowing_at(abs_tile_pos))
                    return;
                else
                    flag = true;

                ivec2 rel_tile_pos = abs_tile_pos - new_part_tile_offset;

                if (!new_part.map.cells.bounds().contains(rel_tile_pos))
                {
                    ivec2 delta = clamp_max(rel_tile_pos, 0);
                    new_part.map.cells.resize(new_part.map.cells.bounds().combine(rel_tile_pos).size(), -delta);
                    new_part_tile_offset += delta;
                    rel_tile_pos -= delta;
                }

                new_part.map.cells.safe_nonthrowing_at(rel_tile_pos) = self.map.cells.safe_nonthrowing_at(abs_tile_pos);

                // Handle pistons arriving here.
                if (auto iter = queued_pistons.find(abs_tile_pos); iter != queued_pistons.end())
                {
                    for (QueuedPiston &elem : iter->second)
                    {
                        ASSERT(!elem.block_b, "Why are we visiting the same piston twice?");

                        if (elem.block_a == &new_part)
                            continue; // This piston links to itself, skip.

                        elem.block_b = &new_part;
                        elem.abs_pixel_pos_b = abs_tile_pos * ShipGrid::tile_size + self.pos;

                        // Make sure A and B are ordered top-left to bottom-right.
                        if (elem.abs_pixel_pos_a[elem.is_vertical] > elem.abs_pixel_pos_b[elem.is_vertical])
                        {
                            std::swap(elem.block_a, elem.block_b);
                            std::swap(elem.abs_pixel_pos_a, elem.abs_pixel_pos_b);

                            elem.abs_pixel_pos_a[elem.is_vertical] += ShipGrid::tile_size;
                            elem.abs_pixel_pos_b[elem.is_vertical] += ShipGrid::tile_size;
                        }
                    }
                }

                // Handle pistons starting here.
                if (ShipGrid::GetTileInfo(this_tile).piston == ShipGrid::PistonRelation::solid_attachable)
                {
                    for (const bool is_vertical : {false, true})
                    for (const bool is_backward : {false, true})
                    {
                        const ShipGrid::Tile piston_tile_type = is_vertical ? ShipGrid::Tile::piston_v : ShipGrid::Tile::piston_h;
                        const ivec2 step = ivec2::axis(is_vertical, is_backward ? -1 : 1);

                        ivec2 piston_tile_pos = abs_tile_pos;

                        while (true)
                        {
                            ivec2 new_pos = piston_tile_pos + step;
                            if (!self.map.cells.bounds().contains(new_pos) || self.map.cells.safe_nonthrowing_at(new_pos).tile != piston_tile_type)
                                break;
                            piston_tile_pos = new_pos;
                        }

                        if (piston_tile_pos != abs_tile_pos)
                        {
                            ivec2 end_tile_pos = piston_tile_pos + step;
                            if (
                                self.map.cells.bounds().contains(end_tile_pos) &&
                                ShipGrid::GetTileInfo(self.map.cells.safe_nonthrowing_at(end_tile_pos).tile).piston == ShipGrid::PistonRelation::solid_attachable &&
                                !visited.safe_nonthrowing_at(end_tile_pos) // This likely means that the piston has the same part on both sides.
                            )
                            {
                                queued_pistons[end_tile_pos].push_back({
                                    .is_vertical = is_vertical,
                                    .block_a = &new_part,
                                    .abs_pixel_pos_a = (abs_tile_pos + step) * ShipGrid::tile_size + self.pos,
                                });
                            }
                        }
                    }
                }

                for (int i = 0; i < 4; i++)
                    lambda(lambda, abs_tile_pos + ivec2::dir4(i));
            };
            lambda(lambda, tile_pos);

            new_part.pos = self.pos + new_part_tile_offset * ShipGrid::tile_size;

            new_part.UpdateAabb();

            if (finalize_blocks)
                finalize_blocks(new_part);
        }
    }

    // Add the pistons.
    for (const auto &queued_piston : queued_pistons)
    {
        for (const auto &elem : queued_piston.second)
        {
            ASSERT(elem.block_b, "Why do we have an unfinished piston?");

            auto &new_piston = game.create<ShipPartPiston>();
            game.link<"pistons", "a">(*elem.block_a, new_piston);
            game.link<"pistons", "b">(*elem.block_b, new_piston);
            new_piston.is_vertical = elem.is_vertical;
            new_piston.pos_relative_to_a = elem.abs_pixel_pos_a - elem.block_a->pos;
            new_piston.pos_relative_to_b = elem.abs_pixel_pos_b - elem.block_b->pos;
            new_piston.UpdateAabb();
        }
    }

    game.destroy(self);
}

ConnectedShipParts FindConnectedShipParts(std::variant<ShipPartBlocks *, ShipPartPiston *, Game::Entity *> blocks_or_piston, std::optional<bool> skip_piston_direction)
{
    ConnectedShipParts ret;

    // The starting piston, if `skip_piston_direction` isn't null.
    ShipPartPiston *half_skipped_piston = nullptr;

    // `prev_piston` is null if this is the first block.
    auto HandleBlocks = [&](auto &HandleBlocks, auto &HandlePiston, ShipPartBlocks &blocks, Game::Id blocks_id, ShipPartPiston *prev_piston) -> void
    {
        if (!ret.blocks.insert(&blocks).second)
            return;
        ret.entity_ids.insert(blocks_id);

        for (const auto &elem : game.get_links<"pistons">(blocks))
        {
            auto &piston = game.get(elem.id()).get<ShipPartPiston>();
            if (&piston == prev_piston)
                continue;
            HandlePiston(HandleBlocks, HandlePiston, piston, elem.id(), &blocks);
            if (ret.cant_skip_because_of_cycle)
                return;
        }
    };
    // `prev_blocks` is null if this is the first piston.
    auto HandlePiston = [&](auto &HandleBlocks, auto &HandlePiston, ShipPartPiston &piston, Game::Id piston_id, ShipPartBlocks *prev_blocks) -> void
    {
        // We don't check the result here, it shouldn't be necessary.
        // It also lets us check for piston cycles when `skip_piston_direction` is non-null below.
        ret.pistons.insert(&piston);
        ret.entity_ids.insert(piston_id);

        if (prev_blocks || !skip_piston_direction || skip_piston_direction.value() != false)
        {
            auto &blocks_entity = game.get_link<"a">(piston);
            if (auto &blocks = blocks_entity.get<ShipPartBlocks>(); &blocks != prev_blocks)
            {
                if (prev_blocks && half_skipped_piston == &piston && skip_piston_direction.value() == true)
                {
                    ret.cant_skip_because_of_cycle = true;
                    return;
                }

                HandleBlocks(HandleBlocks, HandlePiston, blocks, blocks_entity.id(), &piston);
                if (ret.cant_skip_because_of_cycle)
                    return;
            }
        }
        if (prev_blocks || !skip_piston_direction || skip_piston_direction.value() != true)
        {
            auto &blocks_entity = game.get_link<"b">(piston);
            if (auto &blocks = blocks_entity.get<ShipPartBlocks>(); &blocks != prev_blocks)
            {
                if (prev_blocks && half_skipped_piston == &piston && skip_piston_direction.value() == false)
                {
                    ret.cant_skip_because_of_cycle = true;
                    return;
                }

                HandleBlocks(HandleBlocks, HandlePiston, blocks, blocks_entity.id(), &piston);
                if (ret.cant_skip_because_of_cycle)
                    return;
            }
        }
    };

    auto StartWithBlocks = [&](ShipPartBlocks *blocks)
    {
        ASSERT(!skip_piston_direction, "Can't specify `skip_piston_direction` when starting from `ShipPartBlocks`.");
        HandleBlocks(HandleBlocks, HandlePiston, *blocks, dynamic_cast<Game::Entity &>(*blocks).id(), nullptr);
    };
    auto StartWithPiston = [&](ShipPartPiston *piston)
    {
        if (skip_piston_direction)
            half_skipped_piston = piston;
        HandlePiston(HandleBlocks, HandlePiston, *piston, dynamic_cast<Game::Entity &>(*piston).id(), nullptr);
    };

    std::visit(Meta::overload{
        StartWithBlocks,
        StartWithPiston,
        [&](Game::Entity *e)
        {
            if (auto blocks = e->get_opt<ShipPartBlocks>())
                StartWithBlocks(blocks);
            else if (auto piston = e->get_opt<ShipPartPiston>())
                StartWithPiston(piston);
        },
    }, blocks_or_piston);

    return ret;
}

bool CollideShipParts(
    const ConnectedShipParts &parts, ivec2 offset,
    const MapObject *map, const DynamicSolidTree *tree,
    DynamicSolidTree::EntityFilterFunc entity_filter,
    std::variant<std::monostate, const PushParams *, DynamicSolidTree::EntityCallbackFunc> extra
)
{
    auto entity_filter_excluding_self = [next_filter = entity_filter, &parts](const Game::Entity &e)
    {
        return (!next_filter || next_filter(e)) && !parts.entity_ids.contains(e.id());
    };

    DynamicSolidTree::EntityCallbackFunc entity_callback;
    if (auto callback_opt = std::get_if<DynamicSolidTree::EntityCallbackFunc>(&extra))
    {
        entity_callback = std::move(*callback_opt);
    }
    else if (auto push_opt = std::get_if<const PushParams *>(&extra))
    {
        auto push = *push_opt;
        push->result->all_pushed_parts.Append(parts);

        entity_callback = [push, offset, map, tree, entity_filter](Game::Entity &e, std::function<bool(ivec2 offset)> collide) -> bool
        {
            if (push->result->all_pushed_parts.entity_ids.contains(e.id()))
            {
                bool ret = collide(offset); // That entity was already pushed, just use its new position.
                if (ret)
                    std::cout << "Failed because of: " << e.id().get_value() << '\n';
                return ret;
            }

            // That entity wasn't pushed yet.
            bool ret = collide(ivec2{});
            if (ret)
            {
                if (push->allowed_entities && !push->allowed_entities(e))
                    return true; // We're not allowed to move this.

                if (auto blocks = e.get_opt<ShipPartBlocks>(); blocks && !blocks->can_move)
                    return true; // Those are immovable blocks.

                if (collide(offset))
                    return true; // Even if that entity moved, we still wouldn't be able to move.

                auto parts = FindConnectedShipParts(&e);

                ret = CollideShipParts(parts, offset, map, tree, entity_filter, push);
                if (!ret)
                    push->result->at_least_one_pushed = true;
                return ret;
            }
            else
            {
                return false;
            }
        };
    }

    for (const auto &blocks : parts.blocks)
    {
        if (map && blocks->map.CollidesWithMap(map->map, blocks->pos + offset - map->pos))
            return true;

        if (tree && tree->ShipBlocksCollisionTest(*blocks, offset, entity_filter_excluding_self, entity_callback))
            return true;
    }

    for (const auto &piston : parts.pistons)
    {
        if (map && map->map.CollidesWithBox(piston->last_rect + offset - map->pos))
            return true;

        if (tree && tree->BoxCollisionTest(piston->last_rect + offset, entity_filter_excluding_self, entity_callback))
            return true;
    }

    return false;
}

ConnectedShipParts AddDraggedParts(const ConnectedShipParts &parts, ivec2 offset, const MapObject *map, const DynamicSolidTree *tree)
{
    if (!tree)
        return parts;

    ivec2 gravity;
    if (auto con = game.get<GravityController>().get_opt(); con && con->IsEnabled())
        gravity = con->dir;
    if (!gravity)
        return parts;

    ConnectedShipParts new_parts;
    new_parts.Append(parts);

    (void)CollideShipParts(parts, -gravity, nullptr, tree, nullptr, [&](Game::Entity &e, std::function<bool(ivec2 offset)> collide)
    {
        if (new_parts.entity_ids.contains(e.id()))
            return false;

        if (collide(ivec2{}))
        {
            if (auto blocks = e.get_opt<ShipPartBlocks>(); blocks && (!blocks->can_move || !blocks->gravity.enabled))
                return false; // Immovable blocks.

            auto dragged_parts = FindConnectedShipParts(&e);

            bool can_move = !CollideShipParts(dragged_parts, offset, map, tree, nullptr, [&](Game::Entity &e, std::function<bool(ivec2 offset)> collide)
            {
                if (parts.entity_ids.contains(e.id()))
                    return false;
                return collide(ivec2{});
            });

            if (can_move)
                new_parts.Append(dragged_parts);
        }

        return false;
    });

    return new_parts;
}

void MoveShipParts(const ConnectedShipParts &parts, ivec2 offset)
{
    for (const auto &blocks : parts.blocks)
    {
        blocks->pos += offset;
        blocks->UpdateAabb();
    }

    // Pistons don't store their position (other than in AABB), so it doesn't need to be updated.
    for (const auto &piston : parts.pistons)
        piston->UpdateAabb();
}

void MoveShipsByGravity(ivec2 dir, float acc, float max_speed, DynamicSolidTree::EntityFilterFunc filter)
{
    phmap::flat_hash_set<Game::Id> visited_ids;

    auto map = game.get<MapObject>().get_opt();
    auto tree = game.get<DynamicSolidTree>().get_opt();

    for (auto &blocks_entity : game.get<Game::Category<Ent::OrderedList, ShipPartBlocks>>())
    {
        if (visited_ids.contains(blocks_entity.id()))
            continue;

        if (filter && !filter(blocks_entity))
            continue;

        auto &blocks = blocks_entity.get<ShipPartBlocks>();

        if (dir != blocks.gravity.last_dir || !blocks.gravity.enabled)
        {
            // Gravity direction changed, reset speed.
            blocks.gravity.last_dir = blocks.gravity.enabled ? dir : ivec2{};
            blocks.gravity.speed = 0;
            blocks.gravity.speed_comp = 0;
        }
        if (!blocks.gravity.enabled)
            continue;

        clamp_var_max(blocks.gravity.speed += acc, max_speed);

        int step = Math::round_with_compensation(blocks.gravity.speed, blocks.gravity.speed_comp);

        auto parts = FindConnectedShipParts(&blocks);
        visited_ids.insert(parts.entity_ids.begin(), parts.entity_ids.end());

        while (step > 0)
        {
            step--;
            if (!CollideShipParts(parts, dir, map, tree))
            {
                MoveShipParts(parts, dir);
            }
            else
            {
                if (blocks.gravity.speed > 1)
                    audio.Play("block_lands"_sound, 1, ra.f.abs() <= 0.3f);

                blocks.gravity.speed = 0;
                blocks.gravity.speed_comp = 0;
            }
        }

        // Copy the gravity state to all connected parts.
        for (auto other_blocks : parts.blocks)
            other_blocks->gravity = blocks.gravity;
    }
}
