#include <SFML/Graphics.hpp>
#include "DefFont.h"

#include <windows.h>
#include <iostream>
#include <powrprof.h>
#pragma comment(lib, "powrprof.lib")


struct WinRect
{
    int x = 0, y = 0, w = 0, h = 0;

    WinRect(HWND hwnd)
    {
        get(hwnd);
    }
    WinRect() {}

    void get(HWND hwnd)
    {
        RECT rect;
        GetWindowRect(hwnd, &rect);
        x = rect.left;
        y = rect.top;
        w = rect.right - rect.left;
        h = rect.bottom - rect.top;
    }

    bool contains(int xp, int yp)
    {
        return (xp >= x && yp >= y && xp < x + w && yp < y + h);
    }
};

char percent = 0;
bool battery = 0;
bool powersave = 0;

typedef unsigned(__stdcall* ProcGet)(GUID* mode);
typedef unsigned(__stdcall* ProcSet)(GUID* mode);
ProcGet PowerGetEffectiveOverlayScheme;
ProcSet PowerSetEffectiveOverlayScheme;

char powerLevelBat = -1, powerLevelChr = -1;

GUID highPwr = { 0xded574b5, 0x45a0, 0x4f42, 0x87, 0x37, 0x46, 0x34, 0x5c, 0x09, 0xc2, 0x38 };
GUID mdhiPwr = { 0 };
GUID mdloPwr = { 0x961cc777, 0x2547, 0x4f9d, 0x81, 0x74, 0x7d, 0x86, 0x18, 0x1b, 0x8a, 0x7a };
GUID savePwr = { 0x3af9B8d9, 0x7c97, 0x431d, 0xad, 0x78, 0x34, 0xa8, 0xbf, 0xea, 0x43, 0x9f };

void printGUID(GUID guid)
{
    printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

char getPowerMode()
{
    GUID powerMode;
    PowerGetEffectiveOverlayScheme(&powerMode);
    
    if (powerMode == savePwr)
        return 0;
    if (powerMode == mdloPwr)
        return 1;
    if (powerMode == mdhiPwr)
        return 2;
    if (powerMode == highPwr)
        return 3;
    printf("unknown GUID: ");
    printGUID(powerMode);
    return -1;
}

void setPowerMode(char mode)
{
    POWERBROADCAST_SETTING ps = { GUID_POWER_SAVING_STATUS, 1, 1 };
    POWERBROADCAST_SETTING nps = { GUID_POWER_SAVING_STATUS, 1, 0 };

    if (mode)
        BroadcastSystemMessageW(BSF_SENDNOTIFYMESSAGE, 0, WM_POWERBROADCAST, PBT_POWERSETTINGCHANGE, (LPARAM)&nps);
    else
        BroadcastSystemMessageW(BSF_SENDNOTIFYMESSAGE, 0, WM_POWERBROADCAST, PBT_POWERSETTINGCHANGE, (LPARAM)&ps);

    switch (mode)
    {
    case 0:
        PowerSetEffectiveOverlayScheme(&savePwr);
        break;
    case 1:
        PowerSetEffectiveOverlayScheme(&mdloPwr);
        break;
    case 2:
        PowerSetEffectiveOverlayScheme(&mdhiPwr);
        break;
    case 3:
        PowerSetEffectiveOverlayScheme(&highPwr);
        break;
    }
}

HWND getDesktopHwnd()
{
    HWND defView = 0, workerW = 0, desktop = GetDesktopWindow();
    do
    {
        workerW = FindWindowExA(desktop, workerW, "WorkerW", NULL);
        defView = FindWindowExA(workerW, NULL, "SHELLDLL_DefView", NULL);
    } while (!defView && workerW);
    return defView;
}


void powerUpdate(POWERBROADCAST_SETTING* ps)
{
    unsigned data = ps->Data[0];
    if (ps->PowerSetting == GUID_ACDC_POWER_SOURCE)
        battery = data;
    else if (ps->PowerSetting == GUID_BATTERY_PERCENTAGE_REMAINING)
        percent = data;
    else if (ps->PowerSetting == GUID_POWER_SAVING_STATUS)
        powersave = data;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CLOSE:
    case WM_QUIT:
        PostQuitMessage(0);
        break;

    case WM_POWERBROADCAST:
        if (lParam)
            powerUpdate((POWERBROADCAST_SETTING*)lParam);
        break;  

    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}


HWND createWindow(const wchar_t* title, int width, int height)
{
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = title;
    if (!RegisterClassW(&wc))
        return 0;

    return CreateWindowExW(WS_EX_TOOLWINDOW, 
        wc.lpszClassName, wc.lpszClassName,
        WS_POPUP, 0, 0, width, height, 
        0, 0, GetModuleHandleW(NULL), 0);

}


void draw(sf::RenderWindow& window, sf::Text& text, bool click)
{
    window.clear(click ? sf::Color(0x2F2F2FFF) : sf::Color(0x000000FF));

    if (percent != 255)
    {
        text.setFillColor(sf::Color(0xFFFFFFFF));
        text.setString(" " + std::to_string(percent) + "% ");
        text.setPosition(window.getSize().x - text.getLocalBounds().width + 10, -2);
        window.draw(text);

        if (percent < 100)
        {
            switch (battery ? powerLevelBat : powerLevelChr)
            {
            case 0:
                text.setFillColor(sf::Color(0x3FFF7FFF));
                text.setString("+");
                break;

            case 1:
                text.setFillColor(sf::Color(0xFFFF3FFF));
                text.setString("=");
                break;
            case 2:
                text.setFillColor(sf::Color(0xFF7F3FFF));
                text.setString("=");
                break;
            case 3:
                text.setFillColor(sf::Color(0xFF3F3FFF));
                text.setString("!");
                break;
            }

            text.move(0, 5);
            window.draw(text);
            text.move(0, -5);

            if (!battery)
            {
                text.setFillColor(sf::Color(0xFFFF00FF));
                text.setString("^");
                window.draw(text);
            }
        }
    }
    else
    {
        text.setFillColor(sf::Color(0xFF0000FF));
        text.setString("NULL");
        text.setPosition(window.getSize().x - text.getLocalBounds().width + 10, -2);
        window.draw(text);
    }

    window.display();
}


void setWindowOrient(sf::RenderWindow& window, HWND hwnd, WinRect trayRect, char orient)
{
    WinRect rect;
    rect.w = 70;
    rect.h = 30;

    switch (orient)
    {
    case 0:
        rect.x = trayRect.x - rect.w;
        rect.y = trayRect.y + trayRect.h - rect.h;
        break;
    case 2:
        rect.x = trayRect.x - rect.w;
        rect.y = trayRect.y;
        break;

    case 1:
        rect.x = trayRect.x - rect.w + trayRect.w;
        rect.y = trayRect.y - rect.h;
        break;
    case 3:
        rect.x = trayRect.x;
        rect.y = trayRect.y - rect.h;
        break;
    }

    window.setSize(sf::Vector2u(rect.w, rect.h));
    window.setView(sf::View(sf::FloatRect(0, 0, rect.w, rect.h)));
    window.setPosition(sf::Vector2i(rect.x, rect.y));
    SetWindowPos(hwnd, HWND_TOPMOST, rect.x, rect.y, rect.w, rect.h, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

int main()
{
    HMODULE dll = LoadLibraryW(L"powrprof.dll");
    if (!dll)
        return 2;

    PowerGetEffectiveOverlayScheme = (ProcGet) GetProcAddress(dll, "PowerGetEffectiveOverlayScheme");
    PowerSetEffectiveOverlayScheme = (ProcSet) GetProcAddress(dll, "PowerSetActiveOverlayScheme");

    if (!PowerGetEffectiveOverlayScheme || !PowerSetEffectiveOverlayScheme)
        return 3;

    sf::RenderWindow window;
    sf::Text text;

    HWND taskHwnd, trayHwnd, hwnd;
    HPOWERNOTIFY powerSrc, powerPrc, powerSav;
    MSG msg = { 0 };

    WinRect taskRect, trayRect, winRect;

    char taskOrient; // 0 - bottom | 1 - left | 2 - top | 3 - right

    bool click = false, clickLast = false;
    bool clickR = false, clickLastR = false;

    bool running = true;

    // Get taskbar window handle, then get tray window handle
    taskHwnd = FindWindowW(L"Shell_TrayWnd", NULL);
    trayHwnd = FindWindowExW(taskHwnd, NULL, L"TrayNotifyWnd", NULL);

    // Create window
    hwnd = createWindow(L"BatteryDisplay++", 400, 400);
    if (!hwnd)
        return 1;
    window.create(hwnd);
    window.setFramerateLimit(10);
 

    text.setFillColor(sf::Color::White);
    text.setFont(defFont);
    text.setCharacterSize(25);

    // Get positions of taskbar for selecting the correct orientation
    // Bottom:	task.x == 0 && task.y != 0
    // Left:	tray.x == 0
    // Top:		tray.y == 0
    // Right:	task.y == 0 && task.x != 0


    // Register for power change messages
    powerSrc = RegisterPowerSettingNotification(GetCurrentProcess(), &GUID_ACDC_POWER_SOURCE, 0);
    powerPrc = RegisterPowerSettingNotification(GetCurrentProcess(), &GUID_BATTERY_PERCENTAGE_REMAINING, 0);
    powerSav = RegisterPowerSettingNotification(GetCurrentProcess(), &GUID_POWER_SAVING_STATUS, 0);
    
    powerLevelBat = getPowerMode();
    powerLevelChr = getPowerMode();

    while (running)
    {
        bool batteryTemp = battery;
        while (PeekMessageW(&msg, hwnd, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (batteryTemp != battery)
        {
            if (battery)
                setPowerMode(powerLevelBat);
            else
                setPowerMode(powerLevelChr);
        }

        if (!IsWindow(taskHwnd) || !IsWindow(trayHwnd))
        {
            taskHwnd = FindWindowW(L"Shell_TrayWnd", NULL);
            trayHwnd = FindWindowExW(taskHwnd, NULL, L"TrayNotifyWnd", NULL);
        }

        taskRect.get(taskHwnd);
        trayRect.get(trayHwnd);

        if (!taskRect.x && taskRect.y)
            taskOrient = 0;
        else if (!trayRect.x)
            taskOrient = 1;
        else if (!trayRect.y)
            taskOrient = 2;
        else if (!taskRect.y && taskRect.x)
            taskOrient = 3;
        else
            taskOrient = 255;

        setWindowOrient(window, hwnd, trayRect, taskOrient);

        winRect.get(hwnd);
        if (winRect.contains(sf::Mouse::getPosition().x, sf::Mouse::getPosition().y))
        {
            click = sf::Mouse::isButtonPressed(sf::Mouse::Left);
            clickR = sf::Mouse::isButtonPressed(sf::Mouse::Right);

            if (!click && clickLast && window.hasFocus()) // Click release
            {
                if (battery)
                {
                    powerLevelBat = (getPowerMode() + 1) % 4;
                    setPowerMode(powerLevelBat);
                }
                else
                {
                    powerLevelChr = (getPowerMode() + 1) % 4;
                    setPowerMode(powerLevelChr);
                }
            }

            if (!clickR && clickLastR && window.hasFocus()) // Click release
            {
                int close = MessageBoxW(hwnd, L"Close BatteryDisplay++?", L"Close BatteryDisplay++?", MB_YESNOCANCEL);
                if (close == IDYES)
                    running = false;
            }

            clickLast = click;
            clickLastR = clickR;
        }
        else
        {
            if (click)
                click = sf::Mouse::isButtonPressed(sf::Mouse::Left);
            clickLast = click;

            if (clickR)
                clickR = sf::Mouse::isButtonPressed(sf::Mouse::Right);
            clickLastR = clickR;
        }

        draw(window, text, click);
    }


    // Cleanup
    FreeLibrary(dll);
    DestroyWindow(hwnd);
    UnregisterPowerSettingNotification(powerSrc);
    UnregisterPowerSettingNotification(powerPrc);
    UnregisterPowerSettingNotification(powerSav);

    return 0;
}