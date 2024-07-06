// Include standard libraries
#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <thread>
// #include <bitset>
#include <fstream>
#include <queue>

#undef WINVER
#define WINVER NTDDI_WIN10_19H1

#undef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <knownfolders.h>

#include "fpng.h"

// #include "Image.hpp"

// class ThreadRunner
// {
// private:
//     /* 
//     0 : RUN
//     1 : READY
//     2 : FINISH
//     */
//     std::bitset<2> m_Flags;
//     std::thread m_Thread;
//     void (*m_Function)(void*) = nullptr;
//     void* m_Data = nullptr;

//     void m_Runner();

// public:
//     ThreadRunner() {}
//     ~ThreadRunner(){}

//     bool IsRunning() {return m_Flags.test(0);}
//     bool IsReady() {return !m_Flags.test(1);}

//     void Thread(void (*Function)(void*)) {m_Function = Function;}
//     void Run()
//     {
//         if (!m_Flags.test(0) && m_Function != nullptr)
//         {
//             m_Flags.set(0);
//             m_Thread = std::thread(m_Runner, this);
//         }
//     }
//     void Data(void * Data)
//     {
//         m_Data = Data;
//     }
//     void Stop()
//     {
//         m_Flags.reset(0);
//         if (m_Thread.joinable())
//         {
//             m_Thread.join();
//         }
//     }
//     void Exec() {m_Flags.set(1);}
//     // void Stop() {m_Flagsreset(0);}
// };

// void ThreadRunner::m_Runner()
// {
//     while (m_Flags.test(0))
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         if (!m_Flags.test(1))
//         {
//             continue;
//         }
//         m_Function(m_Data);

//         m_Flags.reset(1);
//     }
// }

struct FileQueue
{
    std::string Name;
    std::vector<uint8_t> Data;
};

static std::queue<FileQueue> ScreenshotQueue;

static std::thread WriteToFile;

static std::string UserScreenshotFolder;

static LRESULT LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

static HHOOK LLKeyboardHook;

// static std::vector<ThreadRunner> Threads(2);

static bool isPressed = false;

static bool ThreadRunning = true;

static unsigned int ClipboardType;

std::string GetDate()
{
    std::time_t t = std::time(0);   // get time now
    std::tm* now = std::localtime(&t);

    char Date[11];
    std::strftime(Date,11, "%Y-%m-%d", now);
    Date[11] = '\0';
    return std::string(Date);
}

std::string GetAvailable(const std::string &filename)
{
    static int cur_index = 0;
    std::ifstream f(filename + ".png");
    if (!f.good())
    {
        cur_index = 0;
        cur_index++;
        return filename + ".png";
    }

    int index = 0;

    if (!cur_index)
    {
        while (f.good())
        {
            f = std::ifstream(filename + " (" + std::to_string(++index) + ")" + ".png");
        }
        cur_index = index;
    }

    index = cur_index++;

    return filename + " (" + std::to_string(index) + ")" + ".png";
}

void CaptureToQueue(std::string Filename)
{
    // capture start
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    HWND hDesktopWnd = GetDesktopWindow();
    HDC hDesktopDC = GetDC(0);
    HDC hCaptureDC = CreateCompatibleDC(hDesktopDC);

    HBITMAP hCaptureBitmap = CreateCompatibleBitmap(hDesktopDC, 
                            nScreenWidth, nScreenHeight);
    SelectObject(hCaptureDC,hCaptureBitmap); 
    BitBlt(hCaptureDC,0,0,nScreenWidth,nScreenHeight,
           hDesktopDC,0,0,SRCCOPY|CAPTUREBLT); 


    BITMAP bmp; 

    // Retrieve the bitmap color format, width, and height.  
    GetObject(hCaptureBitmap, sizeof(BITMAP), (LPVOID)&bmp);


    BITMAPINFOHEADER   bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    bi.biHeight = -bmp.bmHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    int bmpComp = bmp.bmBitsPixel/8;
    std::vector<char> BMPdata(bmp.bmWidth * bmp.bmHeight * bmpComp);
    std::vector<uint8_t> PNGdata;

    GetDIBits(hDesktopDC, hCaptureBitmap, 0,
        (UINT)nScreenHeight,
        BMPdata.data(),
        (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // BGRA -> RGBA
    for (unsigned int i = 0; i < BMPdata.size(); i += 4) {
        std::swap(BMPdata[i], BMPdata[i + 2]);
    }

    // bitmap -> png
    fpng::fpng_encode_image_to_memory(BMPdata.data(),bmp.bmWidth, bmp.bmHeight, bmpComp, PNGdata);

    // alloc png
    HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, PNGdata.size()); 
    {
        LPVOID pGlobal = GlobalLock(hglbCopy);
        memcpy(pGlobal, PNGdata.data(), PNGdata.size());
        GlobalUnlock(hglbCopy); 
    }
    // alloc bitmap
    HGLOBAL hglbCopy2 = GlobalAlloc(GMEM_MOVEABLE, BMPdata.size()); 
    {
        LPVOID pGlobal = GlobalLock(hglbCopy2);
        memcpy(pGlobal, BMPdata.data(), BMPdata.size());
        GlobalUnlock(hglbCopy2); 
    }

    OpenClipboard(hDesktopWnd);
    EmptyClipboard();


    SetClipboardData(CF_BITMAP, hglbCopy2);
    SetClipboardData(ClipboardType, hglbCopy);

    CloseClipboard();

    ReleaseDC(hDesktopWnd,hDesktopDC);
    DeleteDC(hCaptureDC);
    DeleteObject(hCaptureBitmap);
    // capture end

    ScreenshotQueue.push({Filename, PNGdata});
}

void WriteThread()
{
    while(ThreadRunning)
    {
        if (ScreenshotQueue.empty())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        std::ofstream Out(ScreenshotQueue.front().Name, std::ios::binary);
        if ( Out.fail() )
        {
            std::cout << "failed here\n";
            std::cout << std::hex << (int)GetLastError() << "\n";
            Out.close();
        }
        else
        {
            Out.write((const char *)ScreenshotQueue.front().Data.data(), (int)ScreenshotQueue.front().Data.size());
            Out.close();
            ScreenshotQueue.pop();
        }


    }
}

void exit()
{
    ThreadRunning = false;
    WriteToFile.join();
    UnhookWindowsHookEx(LLKeyboardHook);
}

// Entry Point
int main (int argc, char *argv[])
{
    WriteToFile = std::thread(WriteThread);
    {
        wchar_t *path;
        SHGetKnownFolderPath(FOLDERID_Screenshots, 0, NULL, &path);
        std::wstring ws(path);
        CoTaskMemFree(path);

        UserScreenshotFolder = std::string(ws.begin(), ws.end());
    }
    std::atexit(exit);
    // SetInit();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    fpng::fpng_init();
    ClipboardType = RegisterClipboardFormatA("PNG");

    LLKeyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    MSG msg = {0};

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    //End Program
    return 0;
}

static LRESULT LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *LLKbd = (KBDLLHOOKSTRUCT *)lParam;
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_KEYUP)
        {
            switch (LLKbd->vkCode)
            {
                case VK_SNAPSHOT:
                    isPressed = false;
                    return 1;
            }
        }
        if (wParam == WM_KEYDOWN)
        {
            switch (LLKbd->vkCode)
            {
                case VK_SNAPSHOT:
                    if (!isPressed)
                    {
                        std::string Filename = GetAvailable(UserScreenshotFolder + "\\" + GetDate());
                        CaptureToQueue(Filename);
                        isPressed = true;
                    }
                    return 1;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}
