#include "wineng.hpp"

int main()
{
    using namespace we;

    App app;
    AppConfig cfg;
    cfg.w = 1200;
    cfg.h = 720;
    cfg.title = L"wineng++ — ECS + Physics + Lighting (C++ rewrite)";
    cfg.resizable = true;

    if(!app.init(cfg)) return 1;

    // -------- Assets (optional) --------
    Image tiles_png;
    bool have_tiles = app.wic.load(tiles_png, "tiles.png"); // optional
    Tileset tileset = make_tileset(have_tiles ? &tiles_png : nullptr, 16, 16);

    Image player_png;
    bool have_player = app.wic.load(player_png, "player.png"); // optional

    // -------- World --------
    World world;
    world.ts = tileset;
    world.tile_px = 32;
    world.bilinear = true;
    world.blend = true;

    // give the player a flat spawn platform near origin
    for(int x=-15;x<=15;x++){
        world.set(x, 10, 2); // stone line
        world.set(x,  9, 4); // grass
    }

    // -------- ECS --------
    ECS ecs;

    // Create player entity
    Entity player = ecs.reg.create();
    ecs.tr.add(player, CTransform{ V2(0, 200), 0.0f, V2(1,1) });
    ecs.vel.add(player, CVel{ V2(0,0) });
    ecs.col.add(player, CCollider{ V2(14,20), false });
    ecs.player.add(player, CPlayer{ 360.0f, 640.0f });

    // Sprite (optional)
    if(have_player){
        CSprite sp;
        sp.img = &player_png;
        sp.sx = 0; sp.sy = 0; sp.sw = player_png.w; sp.sh = player_png.h;
        sp.bilinear = true;
        sp.blend = true;
        sp.tint = RGBA(255,255,255,255);
        ecs.spr.add(player, sp);
    }

    // Torch light entity (follows player)
    Entity torch = ecs.reg.create();
    ecs.tr.add(torch, CTransform{ V2(0,0), 0.0f, V2(1,1) });
    ecs.light.add(torch, CLight{ 12, 255 });

    // -------- Camera --------
    Camera2D cam;
    cam.pos = V2(0, 150);
    cam.zoom = 1.0f;
    cam.rot = 0.0f;

    // -------- Lighting --------
    LightMap lightmap;
    lightmap.ambient = 35; // darkness level

    std::vector<LightSource> lights;

    // -------- Particles --------
    Particles particles;
    particles.init(8000, 0x123456u);

    // -------- UI --------
    UI ui;
    int wx=20, wy=20, ww=360, wh=340;
    bool wopen=true;

    bool show_grid=true;
    bool show_debug=true;

    while(app.frame_begin())
    {
        cam.viewport = V2((f32)app.fb.w, (f32)app.fb.h);

        // Follow camera towards player smoothly (nice feel)
        if(auto* t = ecs.tr.get(player)){
            cam.pos.x = lerp(cam.pos.x, t->pos.x, 1.0f - std::exp(-6.0f*app.dt));
            cam.pos.y = lerp(cam.pos.y, t->pos.y, 1.0f - std::exp(-6.0f*app.dt));
        }

        // Zoom wheel (stable, correct)
        if(app.in.wheel != 0){
            f32 steps = (f32)app.in.wheel / 120.0f;
            cam.zoom *= std::pow(1.15f, steps);
            cam.zoom = clampf(cam.zoom, 0.35f, 4.0f);
        }

        // Dig/place
        v2 mw = cam.screen_to_world(app.in.mouse_x, app.in.mouse_y);
        int tx = (int)std::floor(mw.x / (f32)world.tile_px);
        int ty = (int)std::floor(mw.y / (f32)world.tile_px);

        if(app.in.mouse_pressed[0]){ // dig
            if(world.get(tx,ty) != 0){
                world.set(tx,ty,0);
                particles.emit_burst(V2((tx+0.5f)*world.tile_px,(ty+0.5f)*world.tile_px),
                                     60, 120, 560, 0.18f, 0.55f, 2, 7,
                                     RGBA(230,220,180,220), RGBA(90,70,50,0));
            }
        }
        if(app.in.mouse_pressed[1]){ // place dirt
            if(world.get(tx,ty) == 0){
                world.set(tx,ty,1);
                particles.emit_burst(V2((tx+0.5f)*world.tile_px,(ty+0.5f)*world.tile_px),
                                     30, 80, 360, 0.15f, 0.35f, 2, 6,
                                     RGBA(120,220,255,220), RGBA(60,120,220,0));
            }
        }

        // Update systems
        sys_player(ecs, app.in, app.dt);
        sys_physics(ecs, world, app.dt);

        // Torch follows player
        if(auto* pt=ecs.tr.get(player)){
            if(auto* tt=ecs.tr.get(torch)){
                tt->pos = add(pt->pos, V2(0,-20));
            }
        }

        // Lighting build
        gather_lights(ecs, lights);
        lightmap.build(world, cam, lights);

        // Particles update
        particles.update(app.dt);

        // Render
        app.fb.clear(RGBA(14,15,18,255));

        // world
        sys_render_world(app.fb, world, cam);

        // optional grid highlight
        if(show_grid){
            v2 ts = cam.world_to_screen(V2((f32)(tx*world.tile_px),(f32)(ty*world.tile_px)));
            int sx=(int)std::floor(ts.x);
            int sy=(int)std::floor(ts.y);
            int s = (int)(world.tile_px * cam.zoom);
            app.fb.rect_outline(sx,sy,s,s,2,RGBA(255,240,140,220));
        }

        // entities (sprites)
        sys_render_sprites(app.fb, ecs, cam);

        // particles
        particles.draw(app.fb, cam);

        // darkness overlay (affects everything)
        lightmap.draw_darkness_overlay(app.fb, world, cam);

        // HUD
        if(show_debug){
            char buf[256];
            auto* pt=ecs.tr.get(player);
            auto* pv=ecs.vel.get(player);
            auto* pc=ecs.col.get(player);
            std::snprintf(buf,sizeof(buf),
                "Tile: (%d,%d)  L=%u\n"
                "Player: (%.1f, %.1f) v=(%.1f, %.1f) ground=%d\n"
                "WASD move, SPACE jump, wheel zoom, LMB dig, RMB place",
                tx,ty,(unsigned)lightmap.sample_tile(tx,ty),
                pt?pt->pos.x:0, pt?pt->pos.y:0,
                pv?pv->v.x:0, pv?pv->v.y:0,
                pc? (pc->on_ground?1:0) : 0
            );
            draw_text(app.fb, 14, app.fb.h-66, 2, RGBA(240,240,245,240), buf);
        }

        // UI
        ui.begin(app.fb, app.in);
        if(ui.window_begin("Engine Control Panel", wx,wy,ww,wh, wopen)){
            ui.label("Rendering / Debug");
            ui.checkbox("Show Debug HUD", show_debug);
            ui.checkbox("Show Tile Highlight", show_grid);

            ui.label("Lighting");
            lightmap.ambient = (u8)ui.sliderf("Ambient", (f32)lightmap.ambient, 0, 120);

            ui.label("Camera");
            cam.zoom = ui.sliderf("Zoom", cam.zoom, 0.35f, 4.0f);

            if(ui.button("Spawn Explosion")){
                v2 at = cam.screen_to_world(app.in.mouse_x, app.in.mouse_y);
                particles.emit_burst(at, 240, 140, 900, 0.30f, 1.10f, 2, 9,
                                     RGBA(255,180,80,230), RGBA(255,40,40,0));
            }

            ui.window_end();
        }
        ui.end();

        app.frame_end();
    }

    app.shutdown();
    return 0;
}
