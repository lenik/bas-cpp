#ifndef APP_DEVDEMO_HPP
#define APP_DEVDEMO_HPP

#include "tanks_rbac.hpp"

#include <cstddef>
#include <cwchar>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ncurses.h>

struct TankSim {
    int x = 10;
    int y = 4;
    int facing = 0; // 0=N 1=E 2=S 3=W
    bool running = false;
    std::string lastOp;
    std::string lastResult;
    struct Bullet {
        int x = 0;
        int y = 0;
        int facing = 0;
    };
    std::vector<Bullet> bullets;
};

constexpr int kMinFieldW = 12;
constexpr int kMinFieldH = 5;
constexpr int kMaxFieldW = 48;
constexpr int kMaxFieldH = 24;
constexpr int kHudH = 7;
constexpr int kMinLogLines = 2;
constexpr int kMinAclW = 16;
constexpr int kMaxAclW = 44;
constexpr int kMinGameCols = 36;
constexpr int kMinGameRows = 20;

extern const wchar_t kTankGlyph[4][2];
extern const wchar_t kBulletGlyph[2];

struct CurseGuard {
    CurseGuard();
    ~CurseGuard();
};

struct GameWindows {
    WINDOW* left = nullptr;
    WINDOW* right = nullptr;
    WINDOW* acl = nullptr;
    WINDOW* hud = nullptr;
    WINDOW* log = nullptr;

    ~GameWindows();
};

struct GameLayout {
    int fieldW = 22;
    int fieldH = 9;
    int fieldWinW = 24;
    int fieldWinH = 11;
    int leftX = 1;
    int rightX = 26;
    int tankRowY = 1;
    int mainW = 49;
    int hudY = 12;
    int hudH = kHudH;
    int logY = 19;
    int logH = 8;
    int logLines = 6;
    int aclX = 0;
    int aclW = 0;
    int aclH = 0;
    bool showAcl = false;
};

enum class FieldReadResult { Ok, Cancelled, Empty };

struct LoginInput {
    std::string user;
    std::string pass;
};

struct LoginFormResult {
    enum class Status { Submitted, Cancelled };
    Status status = Status::Cancelled;
    LoginInput input;
};

void addWideGlyph(WINDOW* win, int y, int x, const wchar_t* glyph);
void paintModalWindow(WINDOW* win, int colorPair);
void repaintInputField(WINDOW* dlg, int y, int x, int fieldW, std::string_view value, bool secret);

void destroyGameWindows(GameWindows& gw);

std::optional<GameLayout> computeLayout(int rows, int cols);
void clampTankSim(TankSim& sim, const GameLayout& layout);
void initTankSim(TankSim& sim, const GameLayout& layout);

void mvwPrintClipped(WINDOW* win, int y, int x, std::string_view text);
void drawTitleBar();
bool showTerminalSizeError(int rows, int cols);
bool createGameWindows(GameWindows& gw, const GameLayout& layout);
bool relayoutGame(GameWindows& gw, GameLayout& layout, std::vector<TankSim>& sims,
                  std::deque<std::string>& log, int rows, int cols);

const char* facingLabel(int facing);
std::string truncStr(std::string s, std::size_t maxLen);

void applySim(TankSim& sim, const std::string& op, bool allowed, const GameLayout& layout);
void pushLog(std::deque<std::string>& log, const std::string& line, int maxLines);
void drawBox(WINDOW* win, int pairActive);
void advanceBullets(TankSim& sim, const GameLayout& layout);
void drawField(WINDOW* win, const TankSim& sim, bool active, const GameLayout& layout);
void drawHud(WINDOW* win, const DemoContext& ctx, std::size_t active,
             const std::vector<TankSim>& sims);
void drawPolicyPanel(WINDOW* win, const DemoContext& ctx, std::size_t active);
void drawLog(WINDOW* win, const std::deque<std::string>& log, int logLines);
void redrawGame(GameWindows& gw, DemoContext& ctx, std::size_t active, std::vector<TankSim>& sims,
                const std::deque<std::string>& log, const GameLayout& layout);

FieldReadResult readLoginField(WINDOW* dlg, int y, int x, int maxLen, std::string& out, bool secret,
                               const std::vector<DemoAccount>* demoAccounts = nullptr);
bool cursesNoticeDialog(const std::string& title, const std::string& message);
bool cursesErrorDialog(const std::string& title, const std::string& message);
LoginFormResult cursesLoginForm(const DeviceSlot& device);
LoginFormResult cursesElevateForm(const DeviceSlot& device, const std::string& message,
                                  bool asLogin);
bool cursesLogin(DemoContext& ctx, const DeviceSlot& device, std::deque<std::string>& log,
                 int logLines);
bool tryElevatedOp(DemoContext& ctx, const DeviceSlot& device, const std::string& op,
                   OpResult& result, std::deque<std::string>& log, int logLines);
void performOp(DemoContext& ctx, std::size_t devIdx, std::vector<TankSim>& sims,
               std::deque<std::string>& log, const std::string& op, const GameLayout& layout);
void runGame(DemoContext& ctx);

void printUsage(const char* prog);

#endif // APP_DEVDEMO_HPP
