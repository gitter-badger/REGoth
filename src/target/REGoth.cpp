/*
* Copyright 2011-2016 Branimir Karadzic. All rights reserved.
* License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
*/

#include <common.h>
#include "bgfx_utils.h"
#include <content/StaticLevelMesh.h>
#include <vdfs/fileIndex.h>
#include <zenload/zenParser.h>
#include <zenload/zCMesh.h>
#include <engine/World.h>
#include <engine/GameEngine.h>
#include <utils/bgfx_lib.h>
#include <content/VertexTypes.h>
#include <render/WorldRender.h>
#include <debugdraw/debugdraw.h>
#include <bx/uint32_t.h>
#include <zenload/ztex2dds.h>
#include <render/RenderSystem.h>
#include "config.h"
#include <imgui/imgui.h>
#include <ui/DialogBox.h>
#include <ZenLib/utils/logger.h>
#include <json.hpp>
#include <fstream>
#include <ui/Console.h>
#include <components/VobClasses.h>
#include <logic/NpcScriptState.h>
#include <logic/PlayerController.h>

using json = nlohmann::json;

#if BX_PLATFORM_ANDROID
#include "engine/PlatformAndroid.h"
#define PLATFORM_CLASS Engine::PlatformAndroid
#elif BX_PLATFORM_LINUX || BX_PLATFORM_OSX || BX_PLATFORM_WINDOWS
#include "engine/PlatformGLFW.h"
#define PLATFORM_CLASS Engine::PlatformGLFW
#else
#error Unknown platform
#endif


struct PosColorVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_abgr;

	static void init()
	{
		ms_decl
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	};

	static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosColorVertex::ms_decl;

static PosColorVertex s_cubeVertices[8] =
{
	{ -1.0f,  1.0f,  1.0f, 0xff000000 },
	{ 1.0f,  1.0f,  1.0f, 0xff0000ff },
	{ -1.0f, -1.0f,  1.0f, 0xff00ff00 },
	{ 1.0f, -1.0f,  1.0f, 0xff00ffff },
	{ -1.0f,  1.0f, -1.0f, 0xffff0000 },
	{ 1.0f,  1.0f, -1.0f, 0xffff00ff },
	{ -1.0f, -1.0f, -1.0f, 0xffffff00 },
	{ 1.0f, -1.0f, -1.0f, 0xffffffff },
};

static const uint16_t s_cubeIndices[36] =
{
	0, 1, 2, // 0
	1, 3, 2,
	4, 6, 5, // 2
	5, 6, 7,
	0, 2, 4, // 4
	4, 2, 6,
	1, 5, 3, // 6
	5, 7, 3,
	0, 4, 1, // 8
	4, 5, 1,
	2, 3, 6, // 10
	6, 3, 7,
};


struct PosColorTexCoord0Vertex
{
    float m_x;
    float m_y;
    float m_z;
    uint32_t m_abgr;
    float m_u;
    float m_v;

    static void init()
    {
        ms_decl
                .begin()
                .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .end();
    }

    static bgfx::VertexDecl ms_decl;
};

bgfx::VertexDecl PosColorTexCoord0Vertex::ms_decl;



class ExampleCubes : public /*entry::AppI*/ PLATFORM_CLASS
{
    void renderScreenSpaceQuad(uint32_t _view, bgfx::ProgramHandle _program, float _x, float _y, float _width, float _height)
    {
        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;

        if (bgfx::allocTransientBuffers(&tvb, PosColorTexCoord0Vertex::ms_decl, 4, &tib, 6) )
        {
            PosColorTexCoord0Vertex* vertex = (PosColorTexCoord0Vertex*)tvb.data;

            float zz = 1.0f;

            const float minx = _x;
            const float maxx = _x + _width;
            const float miny = _y;
            const float maxy = _y + _height;

            float minu = 0.0f;
            float minv = 0.0f;
            float maxu =  _width;
            float maxv =  _height;

            vertex[0].m_x = minx;
            vertex[0].m_y = miny;
            vertex[0].m_z = zz;
            vertex[0].m_abgr = 0xffffffff;
            vertex[0].m_u = minu;
            vertex[0].m_v = minv;

            vertex[1].m_x = maxx;
            vertex[1].m_y = miny;
            vertex[1].m_z = zz;
            vertex[1].m_abgr = 0xffffffff;
            vertex[1].m_u = maxu;
            vertex[1].m_v = minv;

            vertex[2].m_x = maxx;
            vertex[2].m_y = maxy;
            vertex[2].m_z = zz;
            vertex[2].m_abgr = 0xffffffff;
            vertex[2].m_u = maxu;
            vertex[2].m_v = maxv;

            vertex[3].m_x = minx;
            vertex[3].m_y = maxy;
            vertex[3].m_z = zz;
            vertex[3].m_abgr = 0xffffffff;
            vertex[3].m_u = minu;
            vertex[3].m_v = maxv;

            uint16_t* indices = (uint16_t*)tib.data;

            indices[0] = 0;
            indices[1] = 2;
            indices[2] = 1;
            indices[3] = 0;
            indices[4] = 3;
            indices[5] = 2;


            bgfx::setIndexBuffer(&tib);
            bgfx::setVertexBuffer(&tvb);
            bgfx::submit(_view, _program);
        }
    }

    void drawLog()
    {
        const std::list<std::string>& logs = Utils::Log::getLastLogLines();
        auto it = logs.begin();

        for(int i=49; i>=0 && it != logs.end() ;i++)
        {
            bgfx::dbgTextPrintf(0, i + 1, 0x4f, (*it).c_str());

            it++;
        }
    }

    void showSplash()
    {
        float view[16];
        float proj[16];

        bx::mtxIdentity(view);
        bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f);



        bgfx::setViewRect(1, 0, 0, (uint16_t)getWindowWidth(), (uint16_t)getWindowHeight());

        // Set view and projection matrix for view 0.
        bgfx::setViewTransform(1, NULL, proj);


        bgfx::touch(0);

#if BX_PLATFORM_ANDROID

#else
        Textures::TextureAllocator alloc(&m_pEngine->getVDFSIndex());
        Handle::TextureHandle txh = alloc.loadTextureVDF("STARTSCREEN.TGA");

        if(!txh.isValid())
            return;

        Textures::Texture& texture = alloc.getTexture(txh);
        bgfx::frame();

        bgfx::setState(BGFX_STATE_DEFAULT);
        const Render::RenderConfig& cfg = m_pEngine->getDefaultRenderSystem().getConfig();
        bgfx::setTexture(0, cfg.uniforms.diffuseTexture, texture.m_TextureHandle);
        renderScreenSpaceQuad(1, cfg.programs.fullscreenQuadProgram, 0.0f, 0.0f, 1280.0f, 720.0f);
#endif


        bgfx::dbgTextPrintf(0, 1, 0x4f, "Loading...");
        bgfx::frame();
    }

	void init(int _argc, char** _argv) BX_OVERRIDE
	{
        std::cout << "Running REGoth Engine" << std::endl;

//		Args args(_argc, _argv);

		axis = 0;
		m_debug = BGFX_DEBUG_TEXT;
#if BX_PLATFORM_ANDROID
        m_reset = 0;
#else
		m_reset = BGFX_RESET_MAXANISOTROPY | BGFX_RESET_MSAA_X8;
#endif

        m_Width = getWindowWidth();
        m_Height = getWindowHeight();

//		bgfx::init(args.m_type, args.m_pciId);
        bgfx::init();
		bgfx::reset((uint32_t)m_Width, (uint32_t)m_Height, m_reset);

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
			, 0x7EC0EEff
			, 1.0f
			, 0
			);

		// Create vertex stream declaration.
		PosColorVertex::init();
		Meshes::UVNormColorVertex::init();
		Meshes::UVNormVertex::init();
		Meshes::UVVertex::init();
		Meshes::PositionVertex::init();
		Meshes::PositionColorVertex::init();
		Meshes::PositionUVVertex::init();
		Meshes::SkeletalVertex::init();

        PosColorTexCoord0Vertex::init();

		m_pEngine = new Engine::GameEngine;

#if BX_PLATFORM_ANDROID
        // Content is somewhere else on android
        m_pEngine->setContentBasePath("/sdcard/REGoth/");
#endif

		m_pEngine->initEngine(_argc, _argv);


        showSplash();

        // Add startworld
        Handle::WorldHandle w = m_pEngine->addWorld(m_pEngine->getEngineArgs().startupZEN);

		m_timeOffset = bx::getHPCounter();

		ddInit();


        // Imgui.
        float fontSize = 18.0f;//18

#ifdef ANDROID
        fontSize = 23.0f;
#endif

        m_Console.registerCommand("test", [](const std::vector<std::string>& args) -> std::string {
            return "Hello World!";
        });

        m_Console.registerCommand("heroexport", [this](const std::vector<std::string>& args) -> std::string {
            auto& s = m_pEngine->getMainWorld().get().getScriptEngine();

            VobTypes::NpcVobInformation player = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), s.getPlayerEntity());

            json pex;
            player.playerController->exportObject(pex);

            std::ofstream f("hero.json");
            f << Utils::iso_8859_1_to_utf8(pex.dump(4));
            f.close();

            return "Hero successfully exported to: hero.json";
        });

        m_Console.registerCommand("heroimport", [this](const std::vector<std::string>& args) -> std::string {
            auto& s = m_pEngine->getMainWorld().get().getScriptEngine();

            std::ifstream f("hero.json");
            std::stringstream saveData;
            saveData << f.rdbuf();

            json hero = json::parse(saveData);

            VobTypes::NpcVobInformation player = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), s.getPlayerEntity());
            player.playerController->importObject(hero, true);

            return "Hero successfully imported from: hero.json";
        });

        m_Console.registerCommand("switchlevel", [this](const std::vector<std::string>& args) -> std::string {

            auto& s1 = m_pEngine->getMainWorld().get().getScriptEngine();

            if(args.size() < 2)
                return "Missing argument. Usage: switchlevel <zenfile>";

            std::string file = args[1];
            if(!m_pEngine->getVDFSIndex().hasFile(file))
                return "File '" + file + "' not found.";

            // Export hero
            VobTypes::NpcVobInformation player = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), s1.getPlayerEntity());

            json pex;
            player.playerController->exportObject(pex);

            // Temporary save
            m_Console.submitCommand("save " + m_pEngine->getMainWorld().get().getZenFile() + ".json");

            // Check if a savegame for this world exists
            if(Utils::fileExists(file + ".json"))
            {
                m_Console.submitCommand("load " + file + " " + file + ".json");
            }else
            {
                clearActions();
                m_pEngine->removeWorld(m_pEngine->getMainWorld());
                m_pEngine->addWorld(args[1]);
            }

            // Import hero again
            auto& s2 = m_pEngine->getMainWorld().get().getScriptEngine();
            if(s2.getPlayerEntity().isValid())
            {
                player = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(),
                                            s2.getPlayerEntity()); // World and player changed
                player.playerController->importObject(pex, true);
            }else
            {
                LogError() << "Player not inserted into new world!";
            }

            return "Successfully switched world to: " + file;
        });

        m_Console.registerCommand("load", [this](const std::vector<std::string>& args) -> std::string {

            if(args.size() < 2)
                return "Missing argument. Usage: load <zenfile> <savegame>";

            std::string savegame = "";

            if(args.size() == 3)
            {
                std::string file = args[2];
                if (!Utils::fileExists(file))
                    return "File '" + file + "' not found.";

                savegame = file;
            }

            clearActions();
            m_pEngine->removeWorld(m_pEngine->getMainWorld());
            m_pEngine->addWorld(args[1], savegame);

            if(!savegame.empty())
                return "Successfully loaded savegame: " + savegame;
            else
                return "Successfully loaded world: " + args[1];
        });

        m_Console.registerCommand("save", [this](const std::vector<std::string>& args) -> std::string {

            if(args.size() < 2)
                return "Missing argument. Usage: save <savegame>";

            json j;
            m_pEngine->getMainWorld().get().exportWorld(j);

            // Save
            std::ofstream f(args[1]);
            f << Utils::iso_8859_1_to_utf8(j.dump(4));
            f.close();

            return "World saved to file: " + args[1];
        });

        m_Console.registerCommand("knockout", [this](const std::vector<std::string>& args) -> std::string {

            VobTypes::NpcVobInformation npc;
            auto& s = m_pEngine->getMainWorld().get().getScriptEngine();

            if(args.size() == 1)
                npc = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), s.getPlayerEntity());
            else
            {
                std::string n = args[1];
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);

                for(Handle::EntityHandle e : s.getWorldNPCs())
                {
                    VobTypes::NpcVobInformation test = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), e);
                    if(test.isValid())
                    {
                        std::string nt = test.playerController->getScriptInstance().name[0];
                        std::transform(nt.begin(), nt.end(), nt.begin(), ::tolower);
                        if(n == nt)
                        {
                            npc = test;
                            break;
                        }
                    }
                }

                if(!npc.isValid())
                    return "Invalid NPC";
            }


            Logic::EventMessages::StateMessage sm;
            sm.subType = Logic::EventMessages::StateMessage::EV_StartState;
            sm.functionSymbol = Logic::NPC_PRGAISTATE_UNCONSCIOUS;
            sm.isPrgState = true;

            npc.playerController->getEM().onMessage(sm);

            return npc.playerController->getScriptInstance().name[0] + " is now in UNCONSCIOUS state";
        });

        m_Console.registerCommand("kill", [this](const std::vector<std::string>& args) -> std::string {

            VobTypes::NpcVobInformation npc;
            auto& s = m_pEngine->getMainWorld().get().getScriptEngine();



            if(args.size() == 1)
                npc = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), s.getPlayerEntity());
            else
            {
                // Fix spaces in names happening
                std::string name;
                for(int i=1;i<args.size();i++)
                    name += args[i] + " ";
                name.pop_back();

                std::string n = name;
                std::transform(n.begin(), n.end(), n.begin(), ::tolower);

                for(Handle::EntityHandle e : s.getWorldNPCs())
                {
                    VobTypes::NpcVobInformation test = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(), e);
                    if(test.isValid())
                    {
                        std::string nt = test.playerController->getScriptInstance().name[0];
                        std::transform(nt.begin(), nt.end(), nt.begin(), ::tolower);
                        if(n == nt)
                        {
                            npc = test;
                            break;
                        }
                    }
                }

                if(!npc.isValid())
                    return "Invalid NPC";
            }


            Logic::EventMessages::StateMessage sm;
            sm.subType = Logic::EventMessages::StateMessage::EV_StartState;
            sm.functionSymbol = Logic::NPC_PRGAISTATE_DEAD;
            sm.isPrgState = true;

            npc.playerController->die(s.getPlayerEntity());

            return npc.playerController->getScriptInstance().name[0] + " is now in DEAD state";
        });

        m_Console.registerCommand("interrupt", [this](const std::vector<std::string>& args) -> std::string {

            VobTypes::NpcVobInformation player = VobTypes::asNpcVob(m_pEngine->getMainWorld().get(),
                                                                    m_pEngine->getMainWorld().get().getScriptEngine().getPlayerEntity());

            player.playerController->interrupt();

            return "Interrupted player, cleared EM";
        });

        imguiCreate(nullptr, 0, fontSize);
        m_scrollArea = 0;
	}

	virtual int shutdown() BX_OVERRIDE
	{
		// Cleanup.

		delete m_pEngine;

		ddShutdown();

        imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() BX_OVERRIDE
	{
        if(!m_ConsoleOpen)
            Engine::Input::fireBindings();
        else
            Engine::Input::clearTriggered();


        // Check for resize
        if(m_Width != getWindowWidth() || m_Height != getWindowHeight())
        {
            m_Width = getWindowWidth();
            m_Height = getWindowHeight();

            // Notify bgfx about framebuffer resize
            bgfx::reset((uint32_t)m_Width, (uint32_t)m_Height);
        }

        int64_t now = bx::getHPCounter();
        static int64_t last = now;
        const int64_t frameTime = now - last;
        last = now;
        const double freq = double(bx::getHPFrequency());
        const double toMs = 1000.0 / freq;

        float time = (float)((now - m_timeOffset) / double(bx::getHPFrequency()));
        const float dt = float(frameTime/freq);

        Engine::Input::MouseState ms; Engine::Input::getMouseState(ms);
        // Prepare rendering of debug-textboxes, etc
        imguiBeginFrame(ms.m_mx
                ,  ms.m_my
                , (ms.m_buttons[0] ? IMGUI_MBUT_LEFT   : 0)
                  | (ms.m_buttons[1] ? IMGUI_MBUT_RIGHT  : 0)
                  | (ms.m_buttons[2] ? IMGUI_MBUT_MIDDLE : 0)
                ,  ms.m_mz
                , (uint16_t)getWindowWidth()
                , (uint16_t)getWindowHeight()
        );

        // Use debug font to print information about this example.
        bgfx::dbgTextClear();
        bgfx::dbgTextPrintf(0, 1, 0x4f, "REGoth-Engine (%s)", m_pEngine->getEngineArgs().startupZEN.c_str());
        bgfx::dbgTextPrintf(0, 2, 0x0f, "Frame: % 7.3f[ms] %.1f[fps]", 1000.0 * dt, 1.0f / (double(dt)));


        // This dummy draw call is here to make sure that view 0 is cleared
        // if no other draw callvm.getDATFile().getSymbolByIndex(self)s are submitted to view 0.
        //bgfx::touch(0);



        // Set render states.
        //bgfx::setState(BGFX_STATE_DEFAULT);
        //bgfx::setTexture(0, m_texUniform, m_texHandle);

        ddBegin(0);

        m_pEngine->frameUpdate(dt, (uint16_t)getWindowWidth(), (uint16_t)getWindowHeight());
        // Draw and process all UI-Views
        // Set render states.


        m_pEngine->getRootUIView().update(dt, ms, m_pEngine->getDefaultRenderSystem().getConfig());


        imguiBeginArea("Debug", 220, 20, 200, 150);

        auto loadWorld = [&](const std::string& world, const std::string& save){
            clearActions();
            m_pEngine->removeWorld(m_pEngine->getMainWorld());
            m_pEngine->addWorld(world, save);
        };

        if(imguiButton(m_ConsoleOpen ? "Close Console" : "Open Console"))
            m_ConsoleOpen = !m_ConsoleOpen;

        if(imguiButton("Load World"))
            loadWorld("world.zen", "testsave.jsav");

        if(imguiButton("Load Newworld"))
            loadWorld("newworld.zen", "testsave.jsav");

        if(imguiButton("Load Addonworld"))
            loadWorld("Addonworld.zen", "testsave.jsav");

        if(imguiButton("Save world"))
        {
            json j;
            m_pEngine->getMainWorld().get().exportWorld(j);

            std::ofstream f("testsave.jsav");

            f << Utils::iso_8859_1_to_utf8(j.dump(4));
        }

        imguiEndArea();

        ddSetTransform(nullptr);
        ddDrawAxis(0.0f, 0.0f, 0.0f);

        ddPush();
        ddSetColor(0xff00ff00);
        ddSetTransform(Math::Matrix::CreateTranslation(10,0,0).mv);
        Aabb aabb =
                {
                        {  -1.0f, -1.0f, -1.0f },
                        { 1.0f, 1.0f, 1.0f },
                };
        ddDraw(aabb);

        ddSetTransform(nullptr);
        ddSetColor(0xff0000ff);
        ddMoveTo(0,0,0);
        ddLineTo(10,0,0);
        ddPop();

        ddEnd();

        imguiEndFrame();

        if(m_ConsoleOpen)
        {
            for (int i = 0; i < NUM_KEYS; i++)
            {
                if (getKeysTriggered()[i])
                {
                    m_Console.onKeyDown(i);
                }
            }

            m_Console.update();
        }

        // Advance to next frame. Rendering thread will be kicked to
        // process submitted rendering primitives.
        bgfx::frame();

        return true;
	}

	Engine::GameEngine* m_pEngine;
	uint32_t m_debug;
	uint32_t m_reset;
    int m_Width, m_Height;
	int64_t m_timeOffset;
	float axis;
    int32_t m_scrollArea;
    UI::Console m_Console;
    bool m_ConsoleOpen = false;
};

//ENTRY_IMPLEMENT_MAIN(ExampleCubes);

//Usage of Platform:

int main(int argc, char** argv)
{
    ExampleCubes app;
    return app.run(argc, argv);
}
