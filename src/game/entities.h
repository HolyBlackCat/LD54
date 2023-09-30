#pragma once

struct Game : Ent::BasicTag<Game,
    Ent::Mixins::ComponentsAsCategories,
    Ent::Mixins::GlobalEntityLists,
    Ent::Mixins::EntityCallbacks,
    Ent::Mixins::EntityLinks
> {};

extern Game::Controller game;

struct Camera
{
    IMP_STANDALONE_COMPONENT(Game)

    ivec2 pos;
};

struct Tickable
{
    IMP_COMPONENT(Game)

    virtual void Tick() = 0;
};
using AllTickable = Game::Category<Ent::OrderedList, Tickable>;

struct PreRenderable
{
    IMP_COMPONENT(Game)

    virtual void PreRender() const = 0;
};
using AllPreRenderable = Game::Category<Ent::OrderedList, PreRenderable>;

struct Renderable
{
    IMP_COMPONENT(Game)

    virtual void Render() const = 0;
};
using AllRenderable = Game::Category<Ent::OrderedList, Renderable>;

struct GuiRenderable
{
    IMP_COMPONENT(Game)

    virtual void GuiRender() const = 0;
};
using AllGuiRenderable = Game::Category<Ent::OrderedList, GuiRenderable>;
