#define UNICODE
#define _UNICODE
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace {

constexpr int WINDOW_W = 1120;
constexpr int WINDOW_H = 780;
constexpr UINT TIMER_ID = 1;
constexpr int TIMER_MS = 35;

enum class Screen { Menu, Instructions, Game, Finish };
enum class Animal { Lion, Rabbit, Elephant, Giraffe, Monkey, Butterfly, Crocodile, Owl, Fox };

struct Challenge {
    Animal animal;
    std::array<Animal, 3> choices;
    int answer;
    const wchar_t* name;
};

const std::array<Challenge, 9> CHALLENGES{{
    {Animal::Lion,      {Animal::Rabbit, Animal::Lion,      Animal::Elephant}, 1, L"León"},
    {Animal::Rabbit,    {Animal::Rabbit, Animal::Elephant,  Animal::Lion},     0, L"Conejo"},
    {Animal::Elephant,  {Animal::Lion,   Animal::Elephant,  Animal::Rabbit},  1, L"Elefante"},
    {Animal::Giraffe,   {Animal::Monkey, Animal::Butterfly, Animal::Giraffe}, 2, L"Jirafa"},
    {Animal::Monkey,    {Animal::Giraffe,Animal::Monkey,    Animal::Butterfly},1,L"Mono"},
    {Animal::Butterfly, {Animal::Butterfly,Animal::Giraffe, Animal::Monkey},  0, L"Mariposa"},
    {Animal::Crocodile, {Animal::Fox,    Animal::Owl,       Animal::Crocodile},2,L"Cocodrilo"},
    {Animal::Owl,       {Animal::Owl,    Animal::Fox,       Animal::Crocodile},0,L"Búho"},
    {Animal::Fox,       {Animal::Crocodile,Animal::Fox,     Animal::Owl},      1,L"Zorro"}
}};

HWND g_window{};
Screen g_screen = Screen::Menu;
int g_question = 0;
int g_stars = 0;
int g_feedback = 0;
int g_feedbackTicks = 0;
int g_animation = 0;
bool g_correctFeedback = false;

COLORREF rgb(int r, int g, int b) { return RGB(r, g, b); }

void fillRect(HDC dc, int x1, int y1, int x2, int y2, COLORREF color) {
    RECT r{x1, y1, x2, y2};
    HBRUSH b = CreateSolidBrush(color);
    FillRect(dc, &r, b);
    DeleteObject(b);
}

void roundedRect(HDC dc, int x1, int y1, int x2, int y2, int radius, COLORREF fill,
                 COLORREF outline = RGB(255, 255, 255), int outlineWidth = 2) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, outlineWidth, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, x1, y1, x2, y2, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void ellipse(HDC dc, int x1, int y1, int x2, int y2, COLORREF fill,
             COLORREF outline = RGB(90, 75, 58), int width = 2) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, width, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Ellipse(dc, x1, y1, x2, y2);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void polygon(HDC dc, const POINT* points, int count, COLORREF fill,
             COLORREF outline = RGB(90, 75, 58), int width = 2) {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, width, outline);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Polygon(dc, points, count);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void line(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 3) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void text(HDC dc, const std::wstring& value, int x1, int y1, int x2, int y2,
          int size, COLORREF color, bool bold = false, UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    HFONT font = CreateFontW(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    RECT r{x1, y1, x2, y2};
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r, format);
    SelectObject(dc, old);
    DeleteObject(font);
}

void drawStar(HDC dc, int cx, int cy, int radius, COLORREF color) {
    POINT p[10];
    constexpr double pi = 3.141592653589793;
    for (int i = 0; i < 10; ++i) {
        const double angle = -pi / 2 + i * pi / 5;
        const int r = (i % 2 == 0) ? radius : radius * 43 / 100;
        p[i] = {cx + static_cast<int>(std::cos(angle) * r),
                cy + static_cast<int>(std::sin(angle) * r)};
    }
    polygon(dc, p, 10, color, color, 1);
}

void drawCloud(HDC dc, int x, int y, int s) {
    const COLORREF cloud = rgb(255, 255, 255);
    ellipse(dc, x, y + s / 3, x + s, y + s, cloud, cloud, 1);
    ellipse(dc, x + s / 5, y, x + s * 3 / 5, y + s * 3 / 4, cloud, cloud, 1);
    ellipse(dc, x + s * 2 / 5, y + s / 6, x + s * 9 / 10, y + s * 4 / 5, cloud, cloud, 1);
}

void drawFlower(HDC dc, int x, int y, int s, COLORREF petals) {
    line(dc, x, y, x, y + s, rgb(55, 138, 74), 3);
    for (int i = 0; i < 5; ++i) {
        const double a = i * 6.28318530718 / 5;
        int px = x + static_cast<int>(std::cos(a) * s / 4);
        int py = y + static_cast<int>(std::sin(a) * s / 4);
        ellipse(dc, px - s / 5, py - s / 5, px + s / 5, py + s / 5, petals, petals, 1);
    }
    ellipse(dc, x - s / 8, y - s / 8, x + s / 8, y + s / 8, rgb(255, 209, 74), rgb(255, 209, 74), 1);
}

void drawBackground(HDC dc) {
    fillRect(dc, 0, 0, WINDOW_W, 530, rgb(161, 224, 246));
    fillRect(dc, 0, 530, WINDOW_W, WINDOW_H, rgb(133, 205, 124));
    ellipse(dc, 45, 480, 1110, 740, rgb(114, 191, 105), rgb(114, 191, 105), 1);
    drawCloud(dc, 95 + (g_animation / 5) % 36, 80, 100);
    drawCloud(dc, 780 - (g_animation / 7) % 30, 115, 125);
    for (int i = 0; i < 8; ++i) {
        int x = 80 + i * 145;
        int base = 555 + (i % 2) * 28;
        fillRect(dc, x + 21, base - 122, x + 55, base + 8, rgb(119, 79, 50));
        ellipse(dc, x - 9, base - 193, x + 83, base - 90,
                i % 2 ? rgb(73, 161, 93) : rgb(62, 150, 86), rgb(62, 150, 86), 1);
        ellipse(dc, x + 6, base - 231, x + 74, base - 145,
                i % 2 ? rgb(95, 178, 102) : rgb(84, 169, 97), rgb(84, 169, 97), 1);
    }
    for (int i = 0; i < 15; ++i) {
        int x = (i * 89 + 38) % WINDOW_W;
        int y = 565 + (i * 37) % 180;
        drawFlower(dc, x, y, 23, i % 3 == 0 ? rgb(255, 130, 155) :
                   (i % 3 == 1 ? rgb(255, 220, 102) : rgb(255, 255, 255)));
    }
    for (int i = 0; i < 8; ++i) {
        int x = 90 + i * 137;
        int y = 56 + (i % 3) * 46;
        drawStar(dc, x, y, 8 + (i % 2) * 3, rgb(255, 225, 101));
    }
}

struct AnimalPainter {
    HDC dc;
    int ox, oy;
    double scale;
    bool silhouette;
    COLORREF ink;

    COLORREF color(int r, int g, int b) const {
        return silhouette ? ink : RGB(r, g, b);
    }
    void e(int x1, int y1, int x2, int y2, int r, int g, int b, int w = 3) {
        ellipse(dc, ox + static_cast<int>(x1 * scale), oy + static_cast<int>(y1 * scale),
                ox + static_cast<int>(x2 * scale), oy + static_cast<int>(y2 * scale),
                color(r, g, b), silhouette ? ink : rgb(91, 70, 55),
                silhouette ? 1 : std::max(1, static_cast<int>(w * scale)));
    }
    void p(const std::vector<POINT>& points, int r, int g, int b) {
        std::vector<POINT> scaled;
        for (const auto& point : points)
            scaled.push_back({ox + static_cast<int>(point.x * scale), oy + static_cast<int>(point.y * scale)});
        polygon(dc, scaled.data(), static_cast<int>(scaled.size()), color(r, g, b),
                silhouette ? ink : rgb(91, 70, 55), silhouette ? 1 : std::max(1, static_cast<int>(2 * scale)));
    }
    void l(int x1, int y1, int x2, int y2, COLORREF c, int width = 3) {
        line(dc, ox + static_cast<int>(x1 * scale), oy + static_cast<int>(y1 * scale),
             ox + static_cast<int>(x2 * scale), oy + static_cast<int>(y2 * scale),
             silhouette ? ink : c, silhouette ? 1 : std::max(1, static_cast<int>(width * scale)));
    }
};

void drawAnimal(HDC dc, Animal animal, int cx, int cy, int size, bool silhouette) {
    AnimalPainter a{dc, cx - size / 2, cy - size / 2, size / 200.0, silhouette, rgb(20, 24, 35)};
    switch (animal) {
    case Animal::Lion:
        a.e(34, 22, 166, 158, 239, 156, 52);
        for (int i = 0; i < 8; ++i) {
            double ang = i * 6.2831853 / 8;
            int x = 100 + static_cast<int>(std::cos(ang) * 66);
            int y = 90 + static_cast<int>(std::sin(ang) * 66);
            a.e(x - 20, y - 20, x + 20, y + 20, 235, 133, 42);
        }
        a.e(52, 37, 148, 141, 255, 204, 104);
        a.e(47, 62, 71, 85, 255, 188, 188); a.e(129, 62, 153, 85, 255, 188, 188);
        if (!silhouette) {
            a.e(76, 76, 86, 89, 45, 48, 52, 1); a.e(114, 76, 124, 89, 45, 48, 52, 1);
            a.e(89, 98, 111, 112, 89, 60, 51, 1);
            a.l(100, 109, 95, 119, rgb(100, 70, 60), 2); a.l(100, 109, 105, 119, rgb(100, 70, 60), 2);
            a.e(68, 98, 78, 103, 255, 145, 142, 1); a.e(122, 98, 132, 103, 255, 145, 142, 1);
        }
        break;
    case Animal::Rabbit:
        a.e(58, 34, 88, 105, 250, 246, 233); a.e(112, 34, 142, 105, 250, 246, 233);
        a.e(66, 42, 81, 91, 245, 164, 176); a.e(119, 42, 134, 91, 245, 164, 176);
        a.e(47, 114, 150, 184, 250, 246, 233); a.e(50, 56, 150, 156, 250, 246, 233);
        a.e(35, 161, 84, 188, 250, 246, 233); a.e(116, 161, 165, 188, 250, 246, 233);
        a.e(32, 130, 59, 158, 255, 255, 255);
        if (!silhouette) { a.e(75, 91, 84, 101, 45, 48, 52, 1); a.e(116, 91, 125, 101, 45, 48, 52, 1); a.e(94, 112, 106, 121, 240, 123, 146, 1); }
        break;
    case Animal::Elephant:
        a.e(17, 48, 91, 143, 115, 177, 205); a.e(109, 48, 183, 143, 115, 177, 205);
        a.e(47, 49, 153, 153, 139, 197, 216); a.e(70, 144, 96, 184, 112, 169, 197);
        a.e(105, 144, 131, 184, 112, 169, 197); a.e(91, 104, 123, 166, 120, 181, 203);
        a.e(91, 139, 123, 180, 120, 181, 203);
        if (!silhouette) { a.e(72, 88, 82, 100, 40, 56, 65, 1); a.e(120, 88, 130, 100, 40, 56, 65, 1); a.l(98, 119, 115, 123, rgb(246, 239, 223), 4); }
        break;
    case Animal::Giraffe:
        a.e(66, 59, 129, 189, 255, 202, 68); a.e(72, 23, 139, 106, 255, 213, 91);
        a.e(73, 5, 91, 38, 255, 202, 68); a.e(119, 5, 137, 38, 255, 202, 68);
        a.e(79, 1, 87, 18, 130, 78, 47); a.e(124, 1, 132, 18, 130, 78, 47);
        a.e(52, 166, 85, 194, 239, 161, 49); a.e(110, 166, 143, 194, 239, 161, 49);
        if (!silhouette) {
            a.e(89, 30, 99, 42, 55, 49, 42, 1); a.e(115, 30, 125, 42, 55, 49, 42, 1);
            a.e(94, 52, 118, 64, 245, 149, 130, 1);
            a.e(83, 74, 104, 94, 173, 103, 46, 1); a.e(103, 115, 125, 137, 173, 103, 46, 1);
            a.e(69, 145, 91, 163, 173, 103, 46, 1);
        }
        break;
    case Animal::Monkey:
        a.e(114, 92, 185, 163, 131, 82, 53); a.e(20, 94, 81, 156, 131, 82, 53);
        a.e(50, 47, 149, 157, 166, 107, 67);
        a.e(60, 19, 140, 111, 166, 107, 67); a.e(75, 46, 125, 96, 248, 205, 151);
        a.e(67, 121, 95, 185, 152, 92, 57); a.e(105, 121, 133, 185, 152, 92, 57);
        if (!silhouette) {
            a.e(82, 62, 91, 73, 35, 41, 44, 1); a.e(109, 62, 118, 73, 35, 41, 44, 1);
            a.e(91, 79, 109, 89, 118, 69, 53, 1);
            a.l(83, 52, 94, 48, rgb(92, 59, 44), 2); a.l(107, 48, 118, 52, rgb(92, 59, 44), 2);
        }
        break;
    case Animal::Butterfly:
        a.e(24, 34, 103, 126, 248, 127, 171); a.e(97, 34, 176, 126, 140, 172, 245);
        a.e(38, 103, 102, 179, 255, 205, 91); a.e(98, 103, 162, 179, 121, 215, 166);
        a.e(89, 65, 111, 153, 87, 65, 116);
        a.l(97, 68, 78, 37, rgb(70, 60, 95), 3); a.l(103, 68, 122, 37, rgb(70, 60, 95), 3);
        if (!silhouette) {
            a.e(56, 60, 75, 81, 255, 255, 255, 1); a.e(125, 60, 144, 81, 255, 255, 255, 1);
            a.e(63, 66, 71, 74, 78, 99, 160, 1); a.e(132, 66, 140, 74, 78, 99, 160, 1);
        }
        break;
    case Animal::Crocodile:
        a.e(35, 63, 149, 137, 89, 174, 91);
        a.p({{137, 82}, {190, 101}, {137, 122}}, 84, 157, 80);
        a.e(15, 94, 119, 151, 111, 193, 96);
        a.p({{38, 106}, {21, 133}, {48, 127}}, 81, 162, 81);
        a.e(55, 137, 76, 181, 71, 139, 72); a.e(111, 137, 132, 181, 71, 139, 72);
        if (!silhouette) {
            a.e(34, 99, 44, 109, 40, 49, 43, 1); a.e(50, 91, 63, 104, 255, 222, 88, 1);
            a.e(58, 96, 64, 102, 35, 42, 35, 1);
            for (int i = 0; i < 5; ++i) a.l(70 + i * 13, 68, 78 + i * 13, 56, rgb(68, 142, 74), 3);
            for (int i = 0; i < 5; ++i) a.l(57 + i * 14, 123, 63 + i * 14, 131, rgb(245, 241, 211), 2);
        }
        break;
    case Animal::Owl:
        a.e(29, 40, 171, 175, 145, 105, 160);
        a.p({{37, 64}, {43, 8}, {85, 47}}, 145, 105, 160);
        a.p({{163, 64}, {157, 8}, {115, 47}}, 145, 105, 160);
        a.e(51, 63, 101, 120, 255, 242, 206); a.e(99, 63, 149, 120, 255, 242, 206);
        a.p({{88, 107}, {112, 107}, {100, 129}}, 250, 171, 72);
        a.e(60, 127, 100, 161, 234, 177, 93); a.e(100, 127, 140, 161, 234, 177, 93);
        if (!silhouette) { a.e(69, 78, 84, 94, 36, 42, 48, 1); a.e(116, 78, 131, 94, 36, 42, 48, 1); a.e(73, 81, 77, 85, 255, 255, 255, 1); a.e(120, 81, 124, 85, 255, 255, 255, 1); }
        break;
    case Animal::Fox:
        a.e(40, 42, 160, 166, 224, 117, 68);
        a.p({{42, 83}, {11, 18}, {80, 54}}, 211, 94, 59);
        a.p({{158, 83}, {189, 18}, {120, 54}}, 211, 94, 59);
        a.p({{46, 66}, {22, 29}, {69, 57}}, 251, 234, 219);
        a.p({{154, 66}, {178, 29}, {131, 57}}, 251, 234, 219);
        a.e(46, 119, 154, 184, 250, 241, 222);
        a.p({{85, 139}, {115, 139}, {100, 158}}, 62, 50, 47);
        a.e(63, 87, 76, 101, 45, 47, 46, 1); a.e(124, 87, 137, 101, 45, 47, 46, 1);
        if (!silhouette) { a.e(66, 89, 71, 95, 255, 255, 255, 1); a.e(127, 89, 132, 95, 255, 255, 255, 1); }
        break;
    }
}

void drawButton(HDC dc, const wchar_t* label, int x1, int y1, int x2, int y2, COLORREF fill) {
    roundedRect(dc, x1, y1, x2, y2, 20, fill, rgb(255, 255, 255), 3);
    text(dc, label, x1, y1, x2, y2 - 2, 25, rgb(255, 255, 255), true);
}

void drawGame(HDC dc) {
    drawBackground(dc);
    const Challenge& q = CHALLENGES[g_question];
    roundedRect(dc, 24, 20, 1096, 95, 28, rgb(255, 255, 255), rgb(216, 242, 214), 3);
    text(dc, L"¡Ayuda a los animales a encontrar sus sombras!", 42, 26, 835, 72, 25, rgb(57, 93, 70), true);
    std::wstring level = L"NIVEL " + std::to_wstring(g_question / 3 + 1) + L"  ·  " +
                         std::to_wstring(g_question % 3 + 1) + L" de 3";
    text(dc, level, 835, 30, 1068, 70, 20, rgb(87, 115, 80), true);

    roundedRect(dc, 365, 112, 755, 426, 30, rgb(255, 253, 239), rgb(255, 255, 255), 4);
    text(dc, L"¿Cuál es su sombra?", 382, 117, 738, 157, 25, rgb(83, 104, 77), true);
    int bob = (g_feedback == 0) ? static_cast<int>(std::sin(g_animation / 6.0) * 5) : 0;
    drawAnimal(dc, q.animal, 560, 282 + bob, 205, false);
    text(dc, q.name, 410, 382, 710, 415, 21, rgb(104, 100, 79), true);

    text(dc, L"¡ELIGE UNA SOMBRA!", 258, 442, 862, 483, 25, rgb(48, 102, 65), true);
    for (int i = 0; i < 3; ++i) {
        int x = 140 + i * 290;
        COLORREF panel = g_feedback == 1 && i == q.answer ? rgb(221, 255, 218) : rgb(255, 255, 255);
        roundedRect(dc, x, 492, x + 250, 700, 24, panel, rgb(255, 255, 255), 4);
        drawAnimal(dc, q.choices[i], x + 125, 596, 145, true);
        text(dc, std::to_wstring(i + 1), x, 660, x + 250, 691, 18, rgb(128, 145, 119), true);
    }
    for (int i = 0; i < 9; ++i)
        drawStar(dc, 478 + i * 21, 736, 9, i < g_stars ? rgb(255, 198, 55) : rgb(218, 226, 204));
    text(dc, L"Estrellas encontradas: " + std::to_wstring(g_stars), 706, 718, 1006, 752,
         18, rgb(67, 103, 68), true);

    if (g_feedback > 0) {
        roundedRect(dc, 174, 313, 946, 405, 25,
                    g_correctFeedback ? rgb(67, 166, 94) : rgb(241, 169, 93),
                    rgb(255, 255, 255), 4);
        if (g_correctFeedback) {
            const int pulse = g_feedbackTicks % 5;
            drawStar(dc, 213, 302, 17 + pulse, rgb(255, 213, 72));
            drawStar(dc, 907, 302, 17 + pulse, rgb(255, 213, 72));
            drawStar(dc, 560, 288, 20 + pulse, rgb(255, 231, 117));
            drawStar(dc, 263, 419, 12 + pulse, rgb(255, 231, 117));
            drawStar(dc, 857, 419, 12 + pulse, rgb(255, 231, 117));
        }
        text(dc, g_correctFeedback ? L"¡MUY BIEN! ¡ENCONTRASTE SU SOMBRA!" : L"¡INTÉNTALO OTRA VEZ!",
             194, 322, 926, 395, g_correctFeedback ? 25 : 29, rgb(255, 255, 255), true);
    }
}

void drawMenu(HDC dc) {
    drawBackground(dc);
    roundedRect(dc, 122, 28, 998, 207, 34, rgb(255, 255, 255), rgb(255, 255, 255), 4);
    text(dc, L"EL BOSQUE PERDIÓ", 157, 50, 963, 112, 43, rgb(48, 111, 73), true);
    text(dc, L"SUS SOMBRAS", 157, 105, 963, 164, 43, rgb(237, 156, 54), true);
    int bob = static_cast<int>(std::sin(g_animation / 6.0) * 9);
    drawAnimal(dc, Animal::Lion, 560, 370 + bob, 250, false);
    drawStar(dc, 400, 276, 20, rgb(255, 220, 79));
    drawStar(dc, 714, 304, 15, rgb(255, 234, 133));
    drawButton(dc, L"JUGAR", 345, 518, 775, 582, rgb(79, 174, 96));
    drawButton(dc, L"INSTRUCCIONES", 345, 596, 775, 653, rgb(83, 157, 207));
    drawButton(dc, L"SALIR", 345, 667, 775, 724, rgb(231, 133, 111));
}

void drawInstructions(HDC dc) {
    drawBackground(dc);
    roundedRect(dc, 145, 59, 975, 665, 38, rgb(255, 255, 247), rgb(255, 255, 255), 4);
    text(dc, L"¿CÓMO SE JUEGA?", 195, 88, 925, 145, 36, rgb(49, 119, 72), true);
    const std::array<const wchar_t*, 5> lines{{
        L"1. Mira con atención al animal de colores.",
        L"2. Elige con el mouse la silueta que se parece a él.",
        L"3. ¡Si aciertas, ganas una estrella y aparece otro animal!",
        L"4. Si te equivocas, puedes volver a intentarlo sin perder estrellas.",
        L"5. ¡Completa los 3 niveles y devuelve la magia al bosque!"
    }};
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        text(dc, lines[i], 210, 185 + i * 64, 910, 234 + i * 64,
             20, rgb(77, 95, 72), true);
    text(dc, L"¡Hay 9 animales esperando tu ayuda!", 225, 526, 895, 568,
         22, rgb(222, 146, 52), true);
    drawButton(dc, L"VOLVER", 385, 585, 735, 644, rgb(79, 174, 96));
}

void drawFinish(HDC dc) {
    drawBackground(dc);
    roundedRect(dc, 180, 90, 940, 660, 38, rgb(255, 255, 247), rgb(255, 255, 255), 4);
    text(dc, L"¡LO LOGRASTE!", 220, 121, 900, 190, 46, rgb(56, 149, 83), true);
    text(dc, L"¡El bosque recuperó sus sombras gracias a ti!", 222, 204, 898, 253,
         23, rgb(83, 102, 75), true);
    drawAnimal(dc, Animal::Lion, 390, 386, 175, false);
    drawAnimal(dc, Animal::Elephant, 565, 394, 165, false);
    drawAnimal(dc, Animal::Butterfly, 735, 360, 120, false);
    for (int i = 0; i < 9; ++i) drawStar(dc, 353 + i * 52, 496, 20, rgb(255, 196, 52));
    text(dc, L"¡Ganaste 9 estrellas!", 270, 523, 850, 565, 25, rgb(197, 131, 35), true);
    drawButton(dc, L"JUGAR OTRA VEZ", 365, 583, 755, 643, rgb(79, 174, 96));
}

void paint(HDC dc) {
    switch (g_screen) {
    case Screen::Menu: drawMenu(dc); break;
    case Screen::Instructions: drawInstructions(dc); break;
    case Screen::Game: drawGame(dc); break;
    case Screen::Finish: drawFinish(dc); break;
    }
}

void playHappySound() {
    MessageBeep(MB_ICONASTERISK);
    Beep(784, 85);
    Beep(1047, 110);
}

void startGame() {
    g_question = 0;
    g_stars = 0;
    g_feedback = 0;
    g_screen = Screen::Game;
}

void handleClick(int x, int y) {
    if (g_screen == Screen::Menu) {
        if (x >= 345 && x <= 775 && y >= 518 && y <= 582) startGame();
        else if (x >= 345 && x <= 775 && y >= 596 && y <= 653) g_screen = Screen::Instructions;
        else if (x >= 345 && x <= 775 && y >= 667 && y <= 724) PostMessageW(g_window, WM_CLOSE, 0, 0);
    } else if (g_screen == Screen::Instructions) {
        if (y >= 570 && y <= 660) g_screen = Screen::Menu;
    } else if (g_screen == Screen::Finish) {
        if (y >= 570 && y <= 655) { startGame(); }
    } else if (g_screen == Screen::Game && g_feedback == 0 && y >= 492 && y <= 705) {
        int choice = -1;
        if (x >= 140 && x <= 390) choice = 0;
        else if (x >= 430 && x <= 680) choice = 1;
        else if (x >= 720 && x <= 970) choice = 2;
        if (choice >= 0) {
            g_feedback = 1;
            g_feedbackTicks = 0;
            g_correctFeedback = choice == CHALLENGES[g_question].answer;
            if (g_correctFeedback) {
                ++g_stars;
                playHappySound();
            } else {
                MessageBeep(MB_ICONEXCLAMATION);
            }
        }
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        SetTimer(window, TIMER_ID, TIMER_MS, nullptr);
        return 0;
    case WM_TIMER:
        ++g_animation;
        if (g_screen == Screen::Game && g_feedback > 0) {
            ++g_feedbackTicks;
            if (g_feedbackTicks >= (g_correctFeedback ? 34 : 28)) {
                g_feedback = 0;
                if (g_correctFeedback) {
                    ++g_question;
                    if (g_question >= static_cast<int>(CHALLENGES.size())) g_screen = Screen::Finish;
                }
            }
        }
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_LBUTTONUP:
        handleClick(LOWORD(lParam), HIWORD(lParam));
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC screen = BeginPaint(window, &ps);
        HDC buffer = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, WINDOW_W, WINDOW_H);
        HGDIOBJ old = SelectObject(buffer, bitmap);
        paint(buffer);
        BitBlt(screen, 0, 0, WINDOW_W, WINDOW_H, buffer, 0, 0, SRCCOPY);
        SelectObject(buffer, old);
        DeleteObject(bitmap);
        DeleteDC(buffer);
        EndPaint(window, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window, TIMER_ID);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const wchar_t CLASS_NAME[] = L"BosquePerdioSusSombras";
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassW(&wc)) return 1;

    RECT bounds{0, 0, WINDOW_W, WINDOW_H};
    AdjustWindowRect(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_window = CreateWindowExW(0, CLASS_NAME, L"EL BOSQUE PERDIÓ SUS SOMBRAS",
                               WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left,
                               bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (!g_window) return 1;
    ShowWindow(g_window, showCommand);
    UpdateWindow(g_window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
