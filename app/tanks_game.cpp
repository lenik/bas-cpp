#include "tanks_game.hpp"

#include <bas/security/AccessDenied.hpp>

#include <algorithm>
#include <clocale>
#include <iostream>

#include <unistd.h>

const wchar_t kTankGlyph[4][2] = {
    {L'\u25b2', L'\0'}, // ▲ north
    {L'\u25b6', L'\0'}, // ▶ east
    {L'\u25bc', L'\0'}, // ▼ south
    {L'\u25c0', L'\0'}, // ◀ west
};
const wchar_t kBulletGlyph[2] = {L'\u00b7', L'\0'}; // ·

CurseGuard::CurseGuard() {
    setlocale(LC_ALL, "");
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    intrflush(stdscr, FALSE);
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);
        init_pair(6, COLOR_WHITE, COLOR_RED);
        init_pair(7, COLOR_WHITE, COLOR_BLACK);
    }
    clear();
    refresh();
}

CurseGuard::~CurseGuard() { endwin(); }

GameWindows::~GameWindows() { destroyGameWindows(*this); }

void addWideGlyph(WINDOW* win, int y, int x, const wchar_t* glyph) {
    mvwaddnwstr(win, y, x, glyph, 1);
}

void paintModalWindow(WINDOW* win, int colorPair) {
    int rows = 0;
    int cols = 0;
    getmaxyx(win, rows, cols);
    const chtype bg = ' ' | COLOR_PAIR(colorPair);
    wbkgd(win, bg);
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            mvwaddch(win, y, x, bg);
        }
    }
}

void repaintInputField(WINDOW* dlg, int y, int x, int fieldW, std::string_view value, bool secret) {
    wattron(dlg, COLOR_PAIR(5));
    for (int col = 0; col < fieldW; ++col) {
        mvwaddch(dlg, y, x + col, ' ');
    }
    wmove(dlg, y, x);
    for (char ch : value) {
        waddch(dlg, secret ? '*' : ch);
    }
    wattroff(dlg, COLOR_PAIR(5));
    wrefresh(dlg);
}

void destroyGameWindows(GameWindows& gw) {
    if (gw.left) {
        delwin(gw.left);
        gw.left = nullptr;
    }
    if (gw.right) {
        delwin(gw.right);
        gw.right = nullptr;
    }
    if (gw.acl) {
        delwin(gw.acl);
        gw.acl = nullptr;
    }
    if (gw.hud) {
        delwin(gw.hud);
        gw.hud = nullptr;
    }
    if (gw.log) {
        delwin(gw.log);
        gw.log = nullptr;
    }
}

std::optional<GameLayout> computeLayout(int rows, int cols) {
    constexpr int kMargin = 1;
    constexpr int kTankGap = 1;
    constexpr int kSectionGap = 1;
    constexpr int kMinLogH = kMinLogLines + 2;

    if (rows < kMinGameRows || cols < kMinGameCols) {
        return std::nullopt;
    }

    const int availRows = rows - 1;
    const int availCols = cols - 2 * kMargin;

    GameLayout L;
    int tankAreaCols = availCols;

    L.showAcl = availCols >= 2 * (kMinFieldW + 2) + kTankGap + kMinAclW;
    if (L.showAcl) {
        L.aclW = std::clamp(availCols / 3, kMinAclW, kMaxAclW);
        tankAreaCols = availCols - L.aclW - 1;
        if (tankAreaCols < 2 * (kMinFieldW + 2) + kTankGap) {
            L.showAcl = false;
            L.aclW = 0;
            tankAreaCols = availCols;
        }
    }

    L.fieldWinW = (tankAreaCols - kTankGap) / 2;
    L.fieldW = std::clamp(L.fieldWinW - 2, kMinFieldW, kMaxFieldW);
    L.fieldWinW = L.fieldW + 2;
    L.leftX = kMargin;
    L.rightX = L.leftX + L.fieldWinW + kTankGap;
    L.mainW = L.rightX + L.fieldWinW - L.leftX;

    if (L.showAcl) {
        L.aclX = L.leftX + L.mainW + 1;
        L.aclW = std::min(L.aclW, cols - L.aclX - 1);
        if (L.aclW < kMinAclW) {
            L.showAcl = false;
            L.aclW = 0;
        }
    }

    const int minFieldWinH = kMinFieldH + 2;
    const int minTotal = minFieldWinH + kSectionGap + kHudH + kMinLogH;
    if (availRows < minTotal) {
        return std::nullopt;
    }

    int leftover = availRows - minTotal;
    L.fieldH = kMinFieldH;
    L.logLines = kMinLogLines;

    const int fieldGrow = std::min(leftover, kMaxFieldH - kMinFieldH);
    L.fieldH += fieldGrow;
    leftover -= fieldGrow;
    L.logLines += leftover;

    L.fieldWinH = L.fieldH + 2;
    L.tankRowY = 1;
    L.hudY = L.tankRowY + L.fieldWinH + kSectionGap;
    L.hudH = kHudH;
    L.logH = L.logLines + 2;
    L.logY = L.hudY + L.hudH;

    if (L.logY + L.logH > rows) {
        L.logH = std::max(kMinLogH, rows - L.logY);
        L.logLines = std::max(kMinLogLines, L.logH - 2);
    }

    L.aclH = std::min(L.fieldWinH + kSectionGap + L.hudH + L.logH, availRows);
    return L;
}

void clampTankSim(TankSim& sim, const GameLayout& layout) {
    sim.x = std::clamp(sim.x, 1, std::max(1, layout.fieldW - 2));
    sim.y = std::clamp(sim.y, 1, std::max(1, layout.fieldH - 2));
}

void initTankSim(TankSim& sim, const GameLayout& layout) {
    sim.x = layout.fieldW / 2;
    sim.y = layout.fieldH / 2;
}

void mvwPrintClipped(WINDOW* win, int y, int x, std::string_view text) {
    int rows = 0;
    int cols = 0;
    getmaxyx(win, rows, cols);
    (void)rows;
    if (x >= cols || y >= rows) {
        return;
    }
    const int maxLen = cols - x - 1;
    if (maxLen <= 0) {
        return;
    }
    mvwprintw(win, y, x, "%.*s", maxLen, std::string(text).c_str());
}

void drawTitleBar() {
    int rows = 0;
    int cols = 0;
    getmaxyx(stdscr, rows, cols);
    (void)rows;
    attron(COLOR_PAIR(4));
    mvhline(0, 0, ' ', cols);
    mvprintw(0, 2, " TANK AC - dual device access control demo ");
    attroff(COLOR_PAIR(4));
    refresh();
}

bool showTerminalSizeError(int rows, int cols) {
    clear();
    mvprintw(2, 2, "Terminal too small for tanks_game.");
    mvprintw(3, 2, "Need at least %dx%d, have %dx%d.", kMinGameCols, kMinGameRows, cols, rows);
    mvprintw(5, 2, "Resize the terminal and try again. Press any key to exit.");
    refresh();
    getch();
    return false;
}

bool createGameWindows(GameWindows& gw, const GameLayout& L) {
    destroyGameWindows(gw);

    gw.left = newwin(L.fieldWinH, L.fieldWinW, L.tankRowY, L.leftX);
    gw.right = newwin(L.fieldWinH, L.fieldWinW, L.tankRowY, L.rightX);
    gw.hud = newwin(L.hudH, L.mainW, L.hudY, L.leftX);
    gw.log = newwin(L.logH, L.mainW, L.logY, L.leftX);
    if (L.showAcl && L.aclW >= kMinAclW) {
        gw.acl = newwin(L.aclH, L.aclW, L.tankRowY, L.aclX);
    }

    return gw.left && gw.right && gw.hud && gw.log;
}

bool relayoutGame(GameWindows& gw, GameLayout& layout, std::vector<TankSim>& sims,
                  std::deque<std::string>& log, int rows, int cols) {
    const auto next = computeLayout(rows, cols);
    if (!next) {
        return false;
    }
    layout = *next;
    for (auto& sim : sims) {
        clampTankSim(sim, layout);
    }
    while (log.size() > static_cast<std::size_t>(layout.logLines)) {
        log.pop_front();
    }
    if (!createGameWindows(gw, layout)) {
        return false;
    }
    clearok(stdscr, TRUE);
    clear();
    return true;
}

const char* facingLabel(int facing) {
    static const char* labels[] = {"N", "E", "S", "W"};
    if (facing >= 0 && facing < 4) {
        return labels[facing];
    }
    return "?";
}

std::string truncStr(std::string s, std::size_t maxLen) {
    if (s.size() <= maxLen) {
        return s;
    }
    if (maxLen <= 1) {
        return s.substr(0, maxLen);
    }
    s.resize(maxLen - 1);
    s.push_back('.');
    return s;
}

void applySim(TankSim& sim, const std::string& op, bool allowed, const GameLayout& layout) {
    if (!allowed) {
        return;
    }
    sim.lastOp = op;
    if (op == "start") {
        sim.running = true;
        return;
    }
    if (op == "stop") {
        sim.running = false;
        return;
    }
    if (op == "fire") {
        TankSim::Bullet bullet;
        bullet.x = sim.x;
        bullet.y = sim.y;
        bullet.facing = sim.facing;
        sim.bullets.push_back(bullet);
        return;
    }
    if (op == "left") {
        sim.facing = (sim.facing + 3) % 4;
        return;
    }
    if (op == "right") {
        sim.facing = (sim.facing + 1) % 4;
        return;
    }
    int dx = 0;
    int dy = 0;
    switch (sim.facing) {
    case 0:
        dy = -1;
        break;
    case 1:
        dx = 1;
        break;
    case 2:
        dy = 1;
        break;
    case 3:
        dx = -1;
        break;
    }
    if (op == "backward") {
        dx = -dx;
        dy = -dy;
    }
    if (op == "forward" || op == "backward") {
        sim.x = std::clamp(sim.x + dx, 1, layout.fieldW - 2);
        sim.y = std::clamp(sim.y + dy, 1, layout.fieldH - 2);
    }
}

void pushLog(std::deque<std::string>& log, const std::string& line, int maxLines) {
    log.push_back(line);
    while (log.size() > static_cast<std::size_t>(maxLines)) {
        log.pop_front();
    }
}

void drawBox(WINDOW* win, int pairActive) {
    if (pairActive >= 0) {
        wattron(win, COLOR_PAIR(pairActive));
    }
    box(win, 0, 0);
    if (pairActive >= 0) {
        wattroff(win, COLOR_PAIR(pairActive));
    }
}

void advanceBullets(TankSim& sim, const GameLayout& layout) {
    std::vector<TankSim::Bullet> next;
    next.reserve(sim.bullets.size());
    for (auto bullet : sim.bullets) {
        int dx = 0;
        int dy = 0;
        switch (bullet.facing) {
        case 0:
            dy = -1;
            break;
        case 1:
            dx = 1;
            break;
        case 2:
            dy = 1;
            break;
        case 3:
            dx = -1;
            break;
        }
        bullet.x += dx;
        bullet.y += dy;
        if (bullet.x >= 1 && bullet.x <= layout.fieldW - 2 && bullet.y >= 1 &&
            bullet.y <= layout.fieldH - 2) {
            next.push_back(bullet);
        }
    }
    sim.bullets = std::move(next);
}

void drawField(WINDOW* win, const TankSim& sim, bool active, const GameLayout& layout) {
    werase(win);
    const int pair = active ? 1 : 4;
    drawBox(win, pair);

    for (int y = 1; y <= layout.fieldH; ++y) {
        for (int x = 1; x <= layout.fieldW; ++x) {
            mvwaddch(win, y, x, '.');
        }
    }

    for (const auto& bullet : sim.bullets) {
        wattron(win, COLOR_PAIR(3));
        addWideGlyph(win, bullet.y + 1, bullet.x + 1, kBulletGlyph);
        wattroff(win, COLOR_PAIR(3));
    }

    const int tx = sim.x + 1;
    const int ty = sim.y + 1;
    const int facing = std::clamp(sim.facing, 0, 3);
    wattron(win, A_BOLD);
    if (active && has_colors()) {
        wattron(win, COLOR_PAIR(2));
    }
    addWideGlyph(win, ty, tx, kTankGlyph[facing]);
    if (active && has_colors()) {
        wattroff(win, COLOR_PAIR(2));
    }
    wattroff(win, A_BOLD);

    if (sim.running) {
        wattron(win, COLOR_PAIR(2));
        mvwprintw(win, layout.fieldH + 1, 2, "ENGINE ON ");
        wattroff(win, COLOR_PAIR(2));
    } else {
        mvwprintw(win, layout.fieldH + 1, 2, "ENGINE OFF");
    }
    wrefresh(win);
}

void drawHud(WINDOW* win, const DemoContext& ctx, std::size_t active,
             const std::vector<TankSim>& sims) {
    werase(win);
    drawBox(win, 4);
    mvwprintw(win, 0, 2, " Controls ");
    mvwPrintClipped(win, 1, 2, "TAB device  L login  F1/F2 engine  Q quit");
    mvwPrintClipped(win, 2, 2, "Arrows move/turn  Space fire");
    mvwprintw(win, 3, 2, "Active: %s", ctx.devices[active].name.c_str());
    for (std::size_t i = 0; i < ctx.devices.size(); ++i) {
        const auto& d = ctx.devices[i];
        const std::string user = truncStr(primaryUser(*ctx.sm, d.realm), 10);
        const std::string role = truncStr(primaryRole(*ctx.sm, d.realm), 10);
        const std::string op = sims[i].lastOp.empty() ? "" : sims[i].lastOp;
        std::string line = std::string(i == active ? ">" : " ") + " " + d.name + "  user=" + user +
                           "  role=" + role + "  " + facingLabel(sims[i].facing);
        if (!op.empty()) {
            line += "  " + op;
        }
        mvwPrintClipped(win, 4 + static_cast<int>(i), 2, line);
    }
    wrefresh(win);
}

void drawPolicyPanel(WINDOW* win, const DemoContext& ctx, std::size_t active) {
    const auto& device = ctx.devices[active];
    int maxRows = 0;
    int maxCols = 0;
    getmaxyx(win, maxRows, maxCols);

    werase(win);
    drawBox(win, 1);
    mvwprintw(win, 0, 2, " Policy %s ", truncStr(device.name, maxCols - 12).c_str());

    const auto* policy = defaultPolicyFor(ctx, device.realm);
    int row = 1;

    auto printRow = [&](std::string_view text, int colorPair = 0, bool bold = false,
                        bool dim = false) -> bool {
        if (row >= maxRows - 1) {
            return false;
        }
        if (has_colors() && colorPair > 0) {
            wattron(win, COLOR_PAIR(colorPair));
        } else if (dim) {
            wattron(win, A_DIM);
            if (has_colors()) {
                wattron(win, COLOR_PAIR(7));
            }
        }
        if (bold) {
            wattron(win, A_BOLD);
        }
        mvwPrintClipped(win, row, 2, text);
        if (bold) {
            wattroff(win, A_BOLD);
        }
        if (dim) {
            if (has_colors()) {
                wattroff(win, COLOR_PAIR(7));
            }
            wattroff(win, A_DIM);
        } else if (has_colors() && colorPair > 0) {
            wattroff(win, COLOR_PAIR(colorPair));
        }
        ++row;
        return row < maxRows - 1;
    };

    auto printHeader = [&](std::string_view text) -> bool {
        return printRow(text, has_colors() ? 2 : 0, false);
    };

    if (!printHeader("BINDINGS")) {
        wrefresh(win);
        return;
    }
    if (!printRow("identity      aclId")) {
        wrefresh(win);
        return;
    }

    if (policy && !policy->bindings().empty()) {
        for (const auto& binding : policy->bindings()) {
            const bool matches = identityMatchesCurrent(ctx, device.realm, binding.identity);
            char line[80];
            std::snprintf(line, sizeof(line), "%-13s %s",
                          truncStr(formatIdentity(binding.identity), 13).c_str(),
                          truncStr(binding.aclId, maxCols - 16).c_str());
            const int color = matches ? 1 : 0;
            if (!printRow(line, color, false, !matches)) {
                printRow("...");
                break;
            }
        }
    } else if (!printRow("(none)")) {
        wrefresh(win);
        return;
    }

    if (!printRow("")) {
        wrefresh(win);
        return;
    }
    if (!printHeader("ACLS")) {
        wrefresh(win);
        return;
    }

    if (policy && !policy->acls().empty()) {
        for (const auto& acl : policy->acls()) {
            const bool aclActive = isAclActiveForCurrent(ctx, device.realm, policy, acl.id);
            char line[64];
            std::snprintf(line, sizeof(line), "%-10s %zu entries", truncStr(acl.id, 10).c_str(),
                          acl.entries.size());
            if (!printRow(line, 0, aclActive)) {
                break;
            }
            for (const auto& entry : acl.entries) {
                const char mode = entry.effect.isAllow() ? 'A' : entry.effect.isDeny() ? 'D' : '?';
                std::snprintf(line, sizeof(line), "  %-14s %c",
                              truncStr(entry.permission.toString(), 14).c_str(), mode);
                const int color = aclActive ? effectColorPair(entry.effect) : 0;
                if (!printRow(line, color, false, !aclActive)) {
                    printRow("  ...");
                    wrefresh(win);
                    return;
                }
            }
        }
    } else if (!printRow("(none)")) {
        wrefresh(win);
        return;
    }

    if (!printRow("")) {
        wrefresh(win);
        return;
    }
    if (!printHeader("GRANTS")) {
        wrefresh(win);
        return;
    }
    const int contentW = std::max(10, maxCols - 4);
    const int idW = std::min(13, contentW / 2);
    const int permW = std::max(6, contentW - idW - 3);
    {
        char header[64];
        std::snprintf(header, sizeof(header), "%-*s %-*s M", idW, "identity", permW, "permission");
        if (!printRow(header)) {
            wrefresh(win);
            return;
        }
    }

    if (policy && !policy->grants().empty()) {
        for (const auto& grant : policy->grants()) {
            const bool highlight = shouldHighlightGrant(ctx, device.realm, grant);
            const char mode = grant.effect.isAllow() ? 'A' : grant.effect.isDeny() ? 'D' : '?';
            char line[128];
            std::snprintf(line, sizeof(line), "%-*s %-*s %c", idW,
                          truncStr(formatIdentity(grant.identity), idW).c_str(), permW,
                          truncStr(grant.permission.toString(), permW).c_str(), mode);
            const int color = highlight ? effectColorPair(grant.effect) : 0;
            if (!printRow(line, color, false, !highlight)) {
                printRow("...");
                break;
            }
        }
    } else {
        printRow("(none)");
    }
    wrefresh(win);
}

void drawLog(WINDOW* win, const std::deque<std::string>& log, int logLines) {
    werase(win);
    drawBox(win, 4);
    mvwprintw(win, 0, 2, " Event log ");
    int row = 1;
    for (const auto& line : log) {
        if (row > logLines) {
            break;
        }
        if (line.rfind("DENIED", 0) == 0) {
            wattron(win, COLOR_PAIR(3));
            mvwPrintClipped(win, row++, 2, line);
            wattroff(win, COLOR_PAIR(3));
        } else {
            wattron(win, COLOR_PAIR(1));
            mvwPrintClipped(win, row++, 2, line);
            wattroff(win, COLOR_PAIR(1));
        }
    }
    wrefresh(win);
}

void redrawGame(GameWindows& gw, DemoContext& ctx, std::size_t active, std::vector<TankSim>& sims,
                const std::deque<std::string>& log, const GameLayout& layout) {
    for (auto& sim : sims) {
        advanceBullets(sim, layout);
    }
    drawTitleBar();
    drawField(gw.left, sims[0], active == 0, layout);
    drawField(gw.right, sims[1], active == 1, layout);
    if (gw.acl) {
        drawPolicyPanel(gw.acl, ctx, active);
    }
    drawHud(gw.hud, ctx, active, sims);
    drawLog(gw.log, log, layout.logLines);
    doupdate();
}

FieldReadResult readLoginField(WINDOW* dlg, int y, int x, int maxLen, std::string& out, bool secret,
                               const std::vector<DemoAccount>* demoAccounts) {
    out.clear();
    const int fieldW = maxLen;
    int demoPick = -1;
    noecho();
    repaintInputField(dlg, y, x, fieldW, out, secret);

    while (static_cast<int>(out.size()) < maxLen) {
        const int ch = wgetch(dlg);
        if (ch == 27) {
            return FieldReadResult::Cancelled;
        }
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            return out.empty() ? FieldReadResult::Empty : FieldReadResult::Ok;
        }
        if (!secret && demoAccounts && !demoAccounts->empty() && (out.empty() || demoPick >= 0)) {
            if (ch == KEY_DOWN) {
                demoPick =
                    demoPick < 0 ? 0 : (demoPick + 1) % static_cast<int>(demoAccounts->size());
                out = (*demoAccounts)[static_cast<std::size_t>(demoPick)].user;
                repaintInputField(dlg, y, x, fieldW, out, secret);
                continue;
            }
            if (ch == KEY_UP) {
                const int n = static_cast<int>(demoAccounts->size());
                demoPick = demoPick < 0 ? n - 1 : (demoPick + n - 1) % n;
                out = (*demoAccounts)[static_cast<std::size_t>(demoPick)].user;
                repaintInputField(dlg, y, x, fieldW, out, secret);
                continue;
            }
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (!out.empty()) {
                out.pop_back();
                if (out.empty()) {
                    demoPick = -1;
                }
                repaintInputField(dlg, y, x, fieldW, out, secret);
            }
            continue;
        }
        if (ch >= 32 && ch < 127) {
            demoPick = -1;
            out.push_back(static_cast<char>(ch));
            repaintInputField(dlg, y, x, fieldW, out, secret);
        }
    }

    return FieldReadResult::Ok;
}

bool cursesNoticeDialog(const std::string& title, const std::string& message) {
    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);

    const int dlgW = std::min(52, maxX - 4);
    const int dlgH = 7;
    const int y0 = (maxY - dlgH) / 2;
    const int x0 = (maxX - dlgW) / 2;

    WINDOW* dlg = newwin(dlgH, dlgW, y0, x0);
    if (!dlg) {
        return false;
    }
    keypad(dlg, TRUE);
    paintModalWindow(dlg, 6);
    drawBox(dlg, 6);
    wattron(dlg, COLOR_PAIR(6));
    mvwprintw(dlg, 0, 2, " %s ", title.c_str());
    mvwprintw(dlg, 2, 2, "%.*s", dlgW - 4, message.c_str());
    mvwprintw(dlg, dlgH - 2, 2, "Enter/Esc=OK");
    wattroff(dlg, COLOR_PAIR(6));
    wrefresh(dlg);

    while (true) {
        const int ch = wgetch(dlg);
        if (ch == 27 || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            break;
        }
    }
    delwin(dlg);
    touchwin(stdscr);
    refresh();
    return true;
}

bool cursesErrorDialog(const std::string& title, const std::string& message) {
    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);

    const int dlgW = std::min(52, maxX - 4);
    const int dlgH = 7;
    const int y0 = (maxY - dlgH) / 2;
    const int x0 = (maxX - dlgW) / 2;

    WINDOW* dlg = newwin(dlgH, dlgW, y0, x0);
    if (!dlg) {
        return false;
    }
    keypad(dlg, TRUE);
    paintModalWindow(dlg, 6);
    drawBox(dlg, 6);
    wattron(dlg, COLOR_PAIR(6));
    mvwprintw(dlg, 0, 2, " %s ", title.c_str());
    mvwprintw(dlg, 2, 2, "%.*s", dlgW - 4, message.c_str());
    mvwprintw(dlg, dlgH - 2, 2, "Enter=retry   Esc=cancel");
    wattroff(dlg, COLOR_PAIR(6));
    wrefresh(dlg);

    const int ch = wgetch(dlg);
    delwin(dlg);
    touchwin(stdscr);
    refresh();
    return ch != 27 && ch != 'q' && ch != 'Q';
}

LoginFormResult cursesLoginForm(const DeviceSlot& device) {
    LoginFormResult result;
    const auto& accounts = demoAccountsFor(device);
    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);

    const int dlgW = std::min(46, maxX - 4);
    const int dlgH = 8 + static_cast<int>(accounts.size());
    const int y0 = (maxY - dlgH) / 2;
    const int x0 = (maxX - dlgW) / 2;

    WINDOW* dlg = newwin(dlgH, dlgW, y0, x0);
    if (!dlg) {
        return result;
    }
    keypad(dlg, TRUE);
    paintModalWindow(dlg, 5);
    drawBox(dlg, 5);
    wattron(dlg, COLOR_PAIR(5));
    mvwprintw(dlg, 0, 2, " Login %s ", device.name.c_str());
    mvwprintw(dlg, 2, 2, "user: ");
    mvwprintw(dlg, 3, 2, "pass: ");
    mvwprintw(dlg, 5, 2, "Up/Down demo user  Enter=OK  Esc=cancel");
    mvwprintw(dlg, 6, 2, "demo accounts (user/pass):");
    int row = 7;
    for (const auto& acct : accounts) {
        mvwprintw(dlg, row++, 4, "%s / %s  (%s)", acct.user, acct.password, acct.role);
    }
    wattroff(dlg, COLOR_PAIR(5));
    wrefresh(dlg);

    std::string user;
    std::string pass;
    curs_set(1);

    const FieldReadResult userRead = readLoginField(dlg, 2, 8, 32, user, false, &accounts);
    if (userRead == FieldReadResult::Cancelled) {
        curs_set(0);
        noecho();
        delwin(dlg);
        touchwin(stdscr);
        refresh();
        return result;
    }
    if (userRead == FieldReadResult::Empty) {
        curs_set(0);
        noecho();
        delwin(dlg);
        touchwin(stdscr);
        refresh();
        return result;
    }

    const FieldReadResult passRead = readLoginField(dlg, 3, 8, 32, pass, true);
    curs_set(0);
    noecho();
    delwin(dlg);
    touchwin(stdscr);
    refresh();

    if (passRead == FieldReadResult::Cancelled) {
        return result;
    }
    if (passRead == FieldReadResult::Empty) {
        return result;
    }

    result.status = LoginFormResult::Status::Submitted;
    result.input.user = trim(user);
    result.input.pass = trim(pass);
    if (result.input.user.empty()) {
        result.status = LoginFormResult::Status::Cancelled;
    }
    return result;
}

LoginFormResult cursesElevateForm(const DeviceSlot& device, const std::string& message,
                                  bool asLogin) {
    LoginFormResult result;
    const auto& accounts = demoAccountsFor(device);
    int maxY = 0;
    int maxX = 0;
    getmaxyx(stdscr, maxY, maxX);

    const int dlgW = std::min(52, maxX - 4);
    const int dlgH = 10 + static_cast<int>(accounts.size());
    const int y0 = (maxY - dlgH) / 2;
    const int x0 = (maxX - dlgW) / 2;

    WINDOW* dlg = newwin(dlgH, dlgW, y0, x0);
    if (!dlg) {
        return result;
    }
    keypad(dlg, TRUE);
    paintModalWindow(dlg, 6);
    drawBox(dlg, 6);
    wattron(dlg, COLOR_PAIR(6));
    mvwprintw(dlg, 0, 2, asLogin ? " Login %s " : " Elevate %s ", device.name.c_str());
    mvwprintw(dlg, 2, 2, "%.*s", dlgW - 4, message.c_str());
    mvwprintw(dlg, 4, 2, "user: ");
    mvwprintw(dlg, 5, 2, "pass: ");
    mvwprintw(dlg, 7, 2,
              asLogin ? "Up/Down demo user  Enter=login  Esc=cancel"
                      : "Up/Down demo user  Enter=authorize  Esc=cancel");
    mvwprintw(dlg, 8, 2, "demo accounts (user/pass):");
    int row = 9;
    for (const auto& acct : accounts) {
        mvwprintw(dlg, row++, 4, "%s / %s  (%s)", acct.user, acct.password, acct.role);
    }
    wattroff(dlg, COLOR_PAIR(6));
    wrefresh(dlg);

    std::string user;
    std::string pass;
    curs_set(1);

    const FieldReadResult userRead = readLoginField(dlg, 4, 8, 32, user, false, &accounts);
    if (userRead == FieldReadResult::Cancelled || userRead == FieldReadResult::Empty) {
        curs_set(0);
        noecho();
        delwin(dlg);
        touchwin(stdscr);
        refresh();
        return result;
    }

    const FieldReadResult passRead = readLoginField(dlg, 5, 8, 32, pass, true);
    curs_set(0);
    noecho();
    delwin(dlg);
    touchwin(stdscr);
    refresh();

    if (passRead == FieldReadResult::Cancelled || passRead == FieldReadResult::Empty) {
        return result;
    }

    result.status = LoginFormResult::Status::Submitted;
    result.input.user = trim(user);
    result.input.pass = trim(pass);
    if (result.input.user.empty()) {
        result.status = LoginFormResult::Status::Cancelled;
    }
    return result;
}

bool cursesLogin(DemoContext& ctx, const DeviceSlot& device, std::deque<std::string>& log,
                 int logLines) {
    while (true) {
        const LoginFormResult form = cursesLoginForm(device);
        if (form.status != LoginFormResult::Status::Submitted) {
            pushLog(log, device.name + ": login cancelled", logLines);
            return false;
        }
        if (loginUser(ctx, device, form.input.user, form.input.pass)) {
            pushLog(log, device.name + ": logged in as " + form.input.user, logLines);
            return true;
        }
        pushLog(log, "DENIED " + device.name + " login failed for " + form.input.user, logLines);
        const std::string msg = "Invalid username or password for " + device.name + ".";
        if (!cursesErrorDialog("Login Failed", msg)) {
            return false;
        }
    }
}

bool tryElevatedOp(DemoContext& ctx, const DeviceSlot& device, const std::string& op,
                   OpResult& result, std::deque<std::string>& log, int logLines) {
    const auto permIt = kOpPermissions.find(op);
    if (permIt == kOpPermissions.end()) {
        return false;
    }

    const bool sessionEmpty = primaryUser(*ctx.sm, device.realm) == "(none)";

    sec::AccessRequestOptions opts;
    opts.realmHint = device.realm;
    opts.allowConsoleInteraction = false;
    opts.allowGuiInteraction = false;
    opts.allowAutoLogin = false;
    opts.reason = sessionEmpty ? "login device " + op : "elevated device " + op;

    const sec::Permission permission{permIt->second};

    while (true) {
        const LoginFormResult form = cursesElevateForm(device, result.message, sessionEmpty);
        if (form.status != LoginFormResult::Status::Submitted) {
            return false;
        }

        sec::Credential cred;
        cred.meta.type = "password";
        cred.meta.subjectHint = form.input.user;
        cred.meta.serviceHint = "bas.identity.user.store";
        cred.meta.realm = device.realm;
        cred.secret = sec::SecretValue(form.input.pass);

        opts.nameHint = form.input.user;
        const auto identities = ctx.sm->authenticate(std::move(cred), opts);
        if (!identities.has_value()) {
            if (!cursesErrorDialog("Auth Failed",
                                   "Invalid username or password for " + device.name + ".")) {
                return false;
            }
            continue;
        }

        const sec::Subject subject = sec::Subject::fromIdentitySet(*identities);
        if (ctx.sm->checkSubjectPermission(permission, subject, opts) != sec::AccessEffect::Allow) {
            const std::string msg =
                form.input.user + " lacks " + permission.toString() + " on " + device.name + ".";
            if (!cursesErrorDialog("Access Denied", msg)) {
                return false;
            }
            continue;
        }

        result.allowed = true;
        if (sessionEmpty) {
            ctx.sm->activate(*identities);
            pushLog(log, device.name + ": logged in as " + form.input.user, logLines);
            result.message = device.name + ": " + op + " OK";
        } else {
            result.message = device.name + ": " + op + " OK (elevated as " + form.input.user + ")";
        }
        return true;
    }
}

void performOp(DemoContext& ctx, std::size_t devIdx, std::vector<TankSim>& sims,
               std::deque<std::string>& log, const std::string& op, const GameLayout& layout) {
    const auto& device = ctx.devices[devIdx];
    OpResult res = requestOp(ctx, device, op);
    if (!res.allowed) {
        tryElevatedOp(ctx, device, op, res, log, layout.logLines);
    }
    applySim(sims[devIdx], op, res.allowed, layout);
    sims[devIdx].lastResult = res.message;
    pushLog(log, res.message, layout.logLines);
}

void runGame(DemoContext& ctx) {
    ctx.sm->logoutAll();
    ctx.sm->setLoginUi(nullptr);

    CurseGuard guard;

    int termRows = 0;
    int termCols = 0;
    getmaxyx(stdscr, termRows, termCols);
    const auto initialLayout = computeLayout(termRows, termCols);
    if (!initialLayout) {
        showTerminalSizeError(termRows, termCols);
        return;
    }

    GameLayout layout = *initialLayout;
    GameWindows gw;
    if (!createGameWindows(gw, layout)) {
        showTerminalSizeError(termRows, termCols);
        return;
    }

    mvwprintw(gw.left, 0, 2, " %s ", ctx.devices[0].name.c_str());
    mvwprintw(gw.right, 0, 2, " %s ", ctx.devices[1].name.c_str());

    std::vector<TankSim> sims(ctx.devices.size());
    for (auto& sim : sims) {
        initTankSim(sim, layout);
    }
    std::deque<std::string> log;
    std::size_t active = 0;

    pushLog(log, "Tank AC demo - per-device user store & ACL", layout.logLines);
    pushLog(log, "L login  arrows move  space fire  denied=elevate/login", layout.logLines);
    pushLog(log, "tank-a: alice=operator(ACL) bob=gunner(grants) admin=commander", layout.logLines);
    pushLog(log, "tank-b: charlie=cadet(ACL) dana=instructor(grants)", layout.logLines);

    redrawGame(gw, ctx, active, sims, log, layout);

    bool running = true;
    while (running) {
        redrawGame(gw, ctx, active, sims, log, layout);

        const int ch = getch();
        switch (ch) {
#ifdef KEY_RESIZE
        case KEY_RESIZE:
            getmaxyx(stdscr, termRows, termCols);
            if (!relayoutGame(gw, layout, sims, log, termRows, termCols)) {
                showTerminalSizeError(termRows, termCols);
                running = false;
            } else {
                mvwprintw(gw.left, 0, 2, " %s ", ctx.devices[0].name.c_str());
                mvwprintw(gw.right, 0, 2, " %s ", ctx.devices[1].name.c_str());
            }
            break;
#endif
        case '\t':
            active = (active + 1) % ctx.devices.size();
            pushLog(log, "switch -> " + ctx.devices[active].name, layout.logLines);
            break;
        case 'q':
        case 'Q':
        case 27:
            running = false;
            break;
        case 'l':
        case 'L':
            cursesLogin(ctx, ctx.devices[active], log, layout.logLines);
            break;
        case KEY_F(1):
            performOp(ctx, active, sims, log, "start", layout);
            break;
        case KEY_F(2):
            performOp(ctx, active, sims, log, "stop", layout);
            break;
        case ' ':
            performOp(ctx, active, sims, log, "fire", layout);
            break;
        case KEY_UP:
            performOp(ctx, active, sims, log, "forward", layout);
            break;
        case KEY_DOWN:
            performOp(ctx, active, sims, log, "backward", layout);
            break;
        case KEY_LEFT:
            performOp(ctx, active, sims, log, "left", layout);
            break;
        case KEY_RIGHT:
            performOp(ctx, active, sims, log, "right", layout);
            break;
        default:
            break;
        }
    }
}

void printUsage(const char* prog) {
    std::cerr << "usage: " << prog << " [--batch]\n"
              << "  default: ncurses tank game (TAB switch, arrows, F1/F2, space)\n"
              << "  --batch: line-oriented mode for scripts\n";
}

int main(int argc, char** argv) {
    bool batch = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--batch") {
            batch = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "unknown argument: " << arg << '\n';
            printUsage(argv[0]);
            return 1;
        }
    }

    try {
        DemoContext ctx = makeContext();
        if (batch || !isatty(STDOUT_FILENO)) {
            runBatch(ctx);
        } else {
            runGame(ctx);
        }
        return 0;
    } catch (const sec::AccessDenied& e) {
        std::cerr << "access denied: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
}
