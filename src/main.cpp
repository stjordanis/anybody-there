#include <raylib.h>
//#include <bits/stdc++.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
// #include <algorithm>
#include <queue>
#include <functional>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#define ASSETS ""
#else
#define ASSETS "data/"
#endif

static constexpr int WINDOW_WIDTH = 960;
static constexpr int WINDOW_HEIGHT = 540;
static constexpr int LOGIC_SCREEN_WIDTH = WINDOW_WIDTH * 2;
static constexpr int LOGIC_SCREEN_HEIGHT = WINDOW_HEIGHT * 2;
static const Vector2 LOGIC_SCREEN_CENTER = {
    (float) LOGIC_SCREEN_WIDTH / 2,
    (float) LOGIC_SCREEN_HEIGHT / 2
};

static std::vector<std::function<bool(void)> > CALLBACKS;

static Sound UP_SFX[10];
static Sound DOWN_SFX[10];

static RenderTexture2D screen;

inline float sqr(float v) {
    return v * v;
}

inline float ssqr(float v) {
    return v * std::abs(v);
}

float dist2(const Vector2 &v1, const Vector2 &v2) {
    return sqr(v1.x - v2.x) + sqr(v1.y - v2.y);
}

template<typename A, typename B>
bool collide(A *a, B *b) {
    return sqr(a->radius() + b->radius()) > dist2(a->pos, b->pos);
}

template<typename T>
inline T coerceIn(T v, T from, T to) {
    return v < from ? from : to < v ? to : v;
}

struct Palette {
    Color playerHeart;
    Color playerBody;
    Color bulletGood;
    Color bulletBad;
    Color helpText;
    Color messageBox;
    Color messageText;
    Color messageHint;
    Color bg;
};

static Palette PALETTES[] = {
    {
        .playerHeart = { 130, 130, 130, 255 } ,
        .playerBody = { 170, 170, 170, 255 },
        .bulletGood = { 170, 170, 170, 255 },
        .bulletBad = GOLD,
        .helpText = {255, 255, 255, 222},
        .messageBox = {240, 51, 65, 192},
        .messageText = {250, 240, 240, 232},
        .messageHint = {250, 240, 240, 232},
        .bg = { 220, 61, 75, 255 },
    },
    {
        .playerHeart = MAROON,
        .playerBody = { 220, 61, 75, 255 },
        .bulletGood = { 220, 61, 75, 255 },
        .bulletBad = SKYBLUE,
        .helpText = {255, 255, 255, 222},
        .messageBox = {130, 130, 130, 192},
        .messageText = {250, 240, 240, 232},
        .messageHint = {250, 240, 240, 232},
        .bg = { 170, 170, 170, 255 },
    }
};

static Palette *PALETTE = &PALETTES[0];
static Palette *NEXT_PALETTE = &PALETTES[1];
static float TRANSITION_LIFETIME;
static float TRANSITION2_LIFETIME;
static int LVL = 0;

static struct Player {
    Vector2 pos = LOGIC_SCREEN_CENTER;
    float additRadius = 0;
    int hp = 0;
    float deltaAddit = 0.3;
    bool transition = false;

    [[nodiscard]] static float radius() {
        return 40.0f;
    }

    void update() {
        float vel = 10;
        if (!transition) {
            if (TRANSITION2_LIFETIME <= 0) {
                if (IsKeyDown(KEY_LEFT)) pos.x -= vel;
                else if (IsKeyDown(KEY_RIGHT)) pos.x += vel;
                if (IsKeyDown(KEY_UP)) pos.y -= vel;
                else if (IsKeyDown(KEY_DOWN)) pos.y += vel;
            }
        } else {
            pos.x += vel;
            pos.y += vel / 4;
        }

//        float d = std::sqrt(1 + dist2(LOGIC_SCREEN_CENTER, pos));
//        float vel = coerceIn(2000.0f / d, 0, 10);

        if (!transition) {
            pos.x -= coerceIn(ssqr(pos.x - LOGIC_SCREEN_CENTER.x) / 60000, -30.f, 30.f);
            pos.y -= coerceIn(ssqr(pos.y - LOGIC_SCREEN_CENTER.y) / 20000, -20.f, 20.f);
        }

        additRadius += deltaAddit;
        if (additRadius > 30) {
            additRadius = 30;
            deltaAddit = -deltaAddit;
        }
        if (additRadius < 0) {
            additRadius = 0;
            deltaAddit = -deltaAddit;
        }
    };

    void draw() const {
        float chainThickness = 10.0f;
        if (transition) {
            chainThickness /= 600 / coerceIn(TRANSITION_LIFETIME - 120, 1.0f, 360.0f);
        }
        DrawLineBezier(pos, LOGIC_SCREEN_CENTER, chainThickness, PALETTE->playerBody);
        DrawCircleV(pos, radius(), PALETTE->playerBody);
        DrawCircleV(pos, float(5 + additRadius), PALETTE->playerHeart);
    };

    void touch(bool good) {
        hp = coerceIn(hp + (good ? 1 : -1), 0, 10);
        if (good) PlaySound(UP_SFX[hp - 1]);
        else PlaySound(DOWN_SFX[hp]);

        deltaAddit *= (float(hp) + 1.f) * 0.3f / std::abs(deltaAddit);
    }
} PLAYER; // NOLINT(cert-err58-cpp)


static struct {
    bool ready = false;

    Music tracks[5] = {0};
    float volumes[5] = {0};
    bool on[5] = {false};

    void init() {
        char fname[32];
        for (int i = 0; i < 5; ++i) {
            std::snprintf(fname, 32, ASSETS "%d_bg0.mp3", i);
            tracks[i] = LoadMusicStream(fname);
        }
        for (auto &track : tracks) {
            PlayMusicStream(track);
        }

        ready = true;
    }

    void update() {
        if (!ready) return;

        float pos = GetMusicTimePlayed(tracks[0]);
        float len = GetMusicTimeLength(tracks[0]);
        float diff = std::min(pos, len - pos);
        float factor = diff > 0.05f
                       ? 1.0f
                       : diff / 0.05f;

        for (int i = 0; i < 5; ++i) {
            on[i] = i <= PLAYER.hp / 2;
        }
        for (int i = 0; i < 5; ++i) {
            float &vol = volumes[i];
            vol = coerceIn(vol + (on[i] ? 0.005f : -0.005f), 0.f, 1.f);
            SetMusicVolume(tracks[i], vol * factor);
            if (i && pos <= 1e-3f && vol <= 1e-3f) {
                StopMusicStream(tracks[i]);
                PlayMusicStream(tracks[i]);
            }
        }
        for (auto &track : tracks) {
            UpdateMusicStream(track);
        }
    }
} MUSIC;

struct Bullet {
    int goodPercent;
    float lifetime;
    bool good;
    float angle;
    float velScalar;
    bool touched;

    Vector2 pos = {0};
    Vector2 vel = {0};

    explicit Bullet(int goodPercent) { // NOLINT(cppcoreguidelines-pro-type-member-init)
        this->goodPercent = goodPercent;
        reset();
    }

    void reset() {
        lifetime = 660.0f + (float) GetRandomValue(60, 600);
        good = GetRandomValue(1, 100) <= this->goodPercent;
        angle = float(GetRandomValue(0, 359)) * DEG2RAD;
        velScalar = float(GetRandomValue(1000, 1500)) * 0.01f;
        touched = false;

        pos = {
            float(GetRandomValue(0, LOGIC_SCREEN_WIDTH)) + LOGIC_SCREEN_WIDTH * 1.5f,
            float(GetRandomValue(0, LOGIC_SCREEN_HEIGHT)) + LOGIC_SCREEN_HEIGHT * 1.5f
        };

        vel = {
            std::cos(angle) * velScalar,
            std::sin(angle) * velScalar,
        };
    }

    void update() {
        if (lifetime < 0) return;
        --lifetime;
        if (touched) lifetime -= 4;
        pos.x += vel.x;
        pos.y += vel.y;

        static float constexpr MARGIN = 30.f;
        if (pos.x < -MARGIN) pos.x = LOGIC_SCREEN_WIDTH + MARGIN;
        if (pos.y < -MARGIN) pos.y = LOGIC_SCREEN_WIDTH + MARGIN;
        if (pos.x > LOGIC_SCREEN_WIDTH + MARGIN) pos.x = -MARGIN;
        if (pos.y > LOGIC_SCREEN_WIDTH + MARGIN) pos.y = -MARGIN;

        if (!touched && lifetime > 70.f && collide(this, &PLAYER)) {
            touched = true;
            PLAYER.touch(good);
            lifetime = std::min(150.0f, lifetime);
        }
    };

    [[nodiscard]] float radius() const {
        return float(std::min(150.0f, lifetime) / 5.0f);
    }

    void draw() const {
        if (lifetime < 0) return;
        DrawCircleV(pos, radius(), good ? PALETTE->bulletGood : PALETTE->bulletBad);
    };
};

static std::vector<Bullet> BULLETS;

static struct {
    bool show = true;

    void draw() const {
        if (show) {
            char msg[128];
            std::snprintf(
                msg, 128,
                ""
                "Controls:\n\n"
                "ARROWS  --  move\n"
                #ifndef PLATFORM_WEB
                "F  --  toggle fullscreen\n"
                "ESC  --  exit fullscreen\n"
                #endif
                "H  --  show help\n"
                "\n"
                "\n"
                "CIRCLE #%d"
                "",
                LVL + 1
            );
            DrawText(msg, 24, 24, 60, PALETTE->helpText);
        }
    }
} HELP;

static struct {
    std::queue<std::pair<std::string, std::function<void(void)> > > messages;

    void next() {
        if (messages.empty()) return;
        auto second = messages.front().second;
        messages.pop();
        second();
    }

    void add(const std::string &msg) {
        messages.emplace(msg, []() {});
    }

    void add(const std::string &msg, const std::function<void(void)> &callback) {
        messages.emplace(msg, callback);
    }

    void update() {
        if (IsKeyPressed(KEY_SPACE)) next();
    }

    void clear() {
        while (!messages.empty()) messages.pop();
    }

    void draw() const {
        if (messages.empty()) return;

        static const Rectangle MSG_BOX_RECT = {24, 868, 1872, 188};

        DrawRectangleRounded(MSG_BOX_RECT, 0.3, 32, PALETTE->messageBox);
        DrawText(
            messages.front().first.c_str(),
            56, 900, 30, PALETTE->messageText
        );
        DrawText(
            "\n\nPress SPACE",
            1650, 900, 30, PALETTE->messageHint
        );
    }
} MESSAGES; // NOLINT(cert-err58-cpp)

//----------------------------------------------------------------------------------------------------------------------

static bool ALL_BULLETS_TOUCHED = false;

void touchAllBullets() {
    if (ALL_BULLETS_TOUCHED) return;
    ALL_BULLETS_TOUCHED = true;
    for (auto &obj : BULLETS) {
        obj.touched = true;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void startLevel01();

void startLevel02();

void startLevel03();

void startTransitionLevel();

//----------------------------------------------------------------------------------------------------------------------

static std::vector<std::vector<std::string> > BEGIN_MESSAGES = { // NOLINT(cert-err58-cpp)
    {
        "*** Human! Hello!\n"
        "*** Oh my God, how lucky I am!\n"
        "*** I'm glad I'm not alone.",

        "*** I seem to be stuck.\n"
        "*** Can you help me out, please?",

        "*** Help me COLLECT the POWER.",
        "*** I'm talking about these flying RED ORBS."
    },
    {
        "*** There is still a little more...",

        "*** Try to avoid WEAKNESS this time.\n"
        "*** I mean these spheres of a different color."
    },
    {
        "*** I hope this is the last time...",

        "*** Ready?"
    }
};


static std::vector<std::vector<std::string> > END_MESSAGES = { // NOLINT(cert-err58-cpp)
    {
        "*** Fine!"
    },
    {
        "*** Excellent!"
    },
    {
        "*** CONGRATULATIONS!",

        "*** I am free!\n" // NOLINT(bugprone-suspicious-missing-comma)
        "*** And you too, right?!",

        "...................",

        "Thank you for playing!\n"
        "This is a game for Ludum Dare 47 Compo\n"
        "Programming and music: Dmitry 'FUMYBULB' Shatov",

        "Thanks to raysan5 for the excellent RAYLIB library!"
    }
};

static std::function<void(void)> NEXTNEXT;

void startLevel(
    int msgId,
    int bulletsCount,
    int goodPercent
//    const std::function<void(void)> &next
) {
    bulletsCount = coerceIn(bulletsCount, 15, 100);
    goodPercent = coerceIn(goodPercent, 19, 100);
    ALL_BULLETS_TOUCHED = false;
    BULLETS.clear();
    CALLBACKS.clear();
    MESSAGES.clear();
    Player p;
    std::swap(p, PLAYER);
    PLAYER.pos = p.pos;

    std::cout << "HELLO" << msgId << std::endl;

    for (int i = 0; i < BEGIN_MESSAGES[msgId].size() - 1; ++i) {
        MESSAGES.add(BEGIN_MESSAGES[msgId][i]);
    }
    MESSAGES.add(BEGIN_MESSAGES[msgId].back(), [msgId, bulletsCount, goodPercent]() {
        HELP.show = false;
        for (int i = 0; i < bulletsCount; ++i) {
            BULLETS.emplace_back(goodPercent);
            Bullet &obj = BULLETS[BULLETS.size() - 1];
        }

        CALLBACKS.emplace_back([]() {
            if (PLAYER.hp == 10) {
                touchAllBullets();
            }
            return false;
        });

        CALLBACKS.emplace_back([msgId]() {
            if (BULLETS.empty()) {
                CALLBACKS.clear();
                for (int i = 0; i < END_MESSAGES[msgId].size() - 1; ++i) {
                    MESSAGES.add(END_MESSAGES[msgId][i]);
                }
                MESSAGES.add(END_MESSAGES[msgId].back(), NEXTNEXT);
            }
            return true;
        });
    });
}

void startLevel01() {
    NEXTNEXT = startLevel02;
    startLevel(0, 15, 100 - 5 * LVL);
}


void startLevel02() {
    NEXTNEXT = startLevel03;
    startLevel(1, 15 + 5 * LVL, 70 - 5 * LVL);
}

void startLevel03() {
    NEXTNEXT = startTransitionLevel;
    startLevel(2, 30 + 5 * LVL, 45 - 2 * LVL);
}


//----------------------------------------------------------------------------------------------------------------------

bool trans03() {
    if (0 <= --TRANSITION2_LIFETIME) return false;

    char msg[32];
    std::snprintf(msg, 32, "Welcome to LEVEL %d!", 1 + LVL);
    MESSAGES.add(msg, startLevel01);
    return true;
}

void trans02() {
    std::swap(PALETTE, NEXT_PALETTE);
    TRANSITION2_LIFETIME = 300;
    Player p;
    std::swap(p, PLAYER);

    if (!LVL) {
        BEGIN_MESSAGES[0].pop_back();
        BEGIN_MESSAGES[1].pop_back();
    }
    ++LVL;
    CALLBACKS.emplace_back(trans03);
}

bool trans01() {
    if (0 <= --TRANSITION_LIFETIME) return false;

    PLAYER.hp = 2;
    CALLBACKS.clear();
    MESSAGES.add("WAIT ***************************");
    MESSAGES.add("WAIT A ****************************************************");
    MESSAGES.add("WAIT A WHILE . . . . . . *************************************************************");
    MESSAGES.add("*** ......llo?", trans02);

    return true;
}

void startTransitionLevel() {
    ALL_BULLETS_TOUCHED = false;
    BULLETS.clear();
    CALLBACKS.clear();
    MESSAGES.clear();

    TRANSITION_LIFETIME = 360;
    PLAYER.transition = true;

    CALLBACKS.emplace_back(trans01);
}

void startLevel00() {
    MESSAGES.add("Press [H] for help", []() {
        InitAudioDevice();
        UP_SFX[0] = LoadSound(ASSETS "1_up01.mp3");
        UP_SFX[1] = LoadSound(ASSETS "2_up02.mp3");
        UP_SFX[2] = LoadSound(ASSETS "3_up03.mp3");
        UP_SFX[3] = LoadSound(ASSETS "4_up04.mp3");
        UP_SFX[4] = LoadSound(ASSETS "5_up05.mp3");
        UP_SFX[5] = LoadSound(ASSETS "6_up06.mp3");
        UP_SFX[6] = LoadSound(ASSETS "7_up07.mp3");
        UP_SFX[7] = LoadSound(ASSETS "8_up08.mp3");
        UP_SFX[8] = LoadSound(ASSETS "9_up09.mp3");
        UP_SFX[9] = LoadSound(ASSETS "10_up10.mp3");

        DOWN_SFX[0] = LoadSound(ASSETS "11_down01.mp3");
        DOWN_SFX[1] = LoadSound(ASSETS "12_down02.mp3");
        DOWN_SFX[2] = LoadSound(ASSETS "13_down03.mp3");
        DOWN_SFX[3] = LoadSound(ASSETS "14_down04.mp3");
        DOWN_SFX[4] = LoadSound(ASSETS "15_down05.mp3");
        DOWN_SFX[5] = LoadSound(ASSETS "16_down06.mp3");
        DOWN_SFX[6] = LoadSound(ASSETS "17_down07.mp3");
        DOWN_SFX[7] = LoadSound(ASSETS "18_down08.mp3");
        DOWN_SFX[8] = LoadSound(ASSETS "19_down09.mp3");
        DOWN_SFX[9] = LoadSound(ASSETS "20_down10.mp3");

        CALLBACKS.emplace_back([]() {
            if (IsKeyPressed(KEY_Z)) PlaySound(UP_SFX[0]);
            if (IsKeyPressed(KEY_X)) PlaySound(UP_SFX[1]);
            if (IsKeyPressed(KEY_C)) PlaySound(UP_SFX[2]);
            if (IsKeyPressed(KEY_V)) PlaySound(UP_SFX[3]);
            if (IsKeyPressed(KEY_B)) PlaySound(UP_SFX[4]);
            if (IsKeyPressed(KEY_N)) PlaySound(UP_SFX[5]);
            if (IsKeyPressed(KEY_M)) PlaySound(UP_SFX[6]);
            return false;
        });

        MUSIC.init();

        MESSAGES.add("PRESS [Z X C V B N M] to test sound");
#ifdef PLATFORM_WEB
        MESSAGES.add(
            "If you have audio glitches and you are using Chrome,\n"
            "SORRY!"
        );
        MESSAGES.add(
            "Possible workarounds:\n"
            "1. Use Firefox\n"
            "2. Switch between tabs for a while"
        );
        MESSAGES.add(
            "Best workaround:\n"
            "Use NATIVE VERSION for Desktop\n"
            "It looks like the web version is buggy."
        );
#endif
        MESSAGES.add("PRESS [ space ] to start playing", []() {
            startLevel01();
        });
    });
}

//----------------------------------------------------------------------------------------------------------------------

void toggleFullscreen() {
    if (IsWindowFullscreen()) {
        ToggleFullscreen();
        SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    } else {
        SetWindowSize(GetMonitorWidth(0), GetMonitorHeight(0));
        ToggleFullscreen();
    }
}

void update() {
#ifndef PLATFORM_WEB
    if (IsKeyPressed(KEY_F)) toggleFullscreen();
    else if (IsKeyPressed(KEY_ESCAPE)) {
        if (IsWindowFullscreen()) {
            toggleFullscreen();
        }
    }
#endif
    if (IsKeyPressed(KEY_H)) HELP.show = !HELP.show;
    MESSAGES.update();
    MUSIC.update();

    bool anyAlive = false;
    for (auto &obj : BULLETS) {
        obj.update();
        anyAlive = anyAlive || 0 < obj.lifetime;
        if (obj.lifetime < 0 && !ALL_BULLETS_TOUCHED) {
            obj.reset();
        }
    }
    if (ALL_BULLETS_TOUCHED && !anyAlive) {
        BULLETS.clear();
    }

    PLAYER.update();

    for (auto &f : CALLBACKS) {
        if (f()) {
            break;
        }
    }
}

void drawScreenTexture() {
    float scale = std::min(
        (float) GetScreenWidth() / LOGIC_SCREEN_WIDTH,
        (float) GetScreenHeight() / LOGIC_SCREEN_HEIGHT
    );
    DrawTexturePro(
        screen.texture,
        (Rectangle) {
            0.0f, 0.0f, (float) screen.texture.width, (float) -screen.texture.height
        },
        (Rectangle) {
            ((float) GetScreenWidth() - ((float) LOGIC_SCREEN_WIDTH * scale)) * 0.5f,
            ((float) GetScreenHeight() - ((float) LOGIC_SCREEN_HEIGHT * scale)) * 0.5f,
            (float) LOGIC_SCREEN_WIDTH * scale, (float) LOGIC_SCREEN_HEIGHT * scale
        },
        (Vector2) {0, 0}, 0.0f, WHITE
    );
}

void draw() {
    BeginDrawing();
    ClearBackground(BLACK);
    BeginTextureMode(screen);

    //----------------------------------------------------------------

    ClearBackground(PALETTE->bg);
    PLAYER.draw();
    for (auto &obj : BULLETS) {
        obj.draw();
    }
    if (0 < TRANSITION2_LIFETIME) {
        DrawCircleV(LOGIC_SCREEN_CENTER, TRANSITION2_LIFETIME * 10, PALETTE->playerBody);
    }
    HELP.draw();
    MESSAGES.draw();

    //----------------------------------------------------------------

    EndTextureMode();
    drawScreenTexture();
    EndDrawing();
}

//----------------------------------------------------------------------------------------------------------------------

void init() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE); // NOLINT(hicpp-signed-bitwise)
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "LD 47 Entry");
    SetTargetFPS(60);
    screen = LoadRenderTexture(LOGIC_SCREEN_WIDTH, LOGIC_SCREEN_HEIGHT);
    SetTextureFilter(screen.texture, FILTER_TRILINEAR);
#ifdef _WIN32
    toggleFullscreen();
#endif
    SetExitKey(0);
}

void updateAndDraw() {
    update();
    draw();
}

int main() {
    init();

    startLevel00();
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(updateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        updateAndDraw();
    }

    UnloadRenderTexture(screen);
//    UnloadSound(UP_SFX[0]); // TODO!
    CloseAudioDevice();
    CloseWindow();
    return 0;
#endif
}