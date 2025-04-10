#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <wingdi.h>
#include <winnt.h>
#include <winuser.h>
#include <Uxtheme.h>
#include <CommCtrl.h>
#include <commdlg.h>

#pragma comment(lib, "uxtheme.lib")

HBRUSH hBackgroundBrush;
HBITMAP hBitmap;

void enable()
{
    SetThemeAppProperties(STAP_ALLOW_NONCLIENT | STAP_ALLOW_CONTROLS | STAP_ALLOW_WEBCONTENT);
}

void createManifest(const char *filePath)
{
    FILE *file = fopen(filePath, "wb");
    if(!file)
    {
        perror("Failed to open file");
        return;
    }

    const char *manifestContent =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
        " <dependency>\n"
        "   <dependentAssembly>\n"
        "     <assemblyIdentity\n"
        "       type=\"win32\"\n"
        "       name=\"Microsoft.Windows.Common-Controls\"\n"
        "       version=\"6.0.0.0\"\n"
        "       publicKeyToken=\"6595b64144ccf1df\"\n"
        "       language=\"*\"\n"
        "       processorArchitecture=\"*\"/>\n"
        "   </dependentAssembly>\n"
        " </dependency>\n"
        " <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n"
        "   <security>\n"
        "     <requestedPrivileges>\n"
        "       <requestedExecutionLevel\n"
        "         level=\"asInvoker\"\n"
        "         uiAccess=\"false\"/>\n"
        "     </requestedPrivileges>\n"
        "   </security>\n"
        " </trustInfo>\n"
        "</assembly>\n";

    fputs(manifestContent, file);

    fclose(file);
}

BOOL updateManifest(const char *output, const char *manifest)
{
    FILE *manifestFile = fopen(manifest, "rb");
    if(!manifestFile)
    {
        fprintf(stderr, "Failed to open manifest file: %s\n", manifest);
        return FALSE;
    }

    fseek(manifestFile, 0, SEEK_END);
    long fileSize = ftell(manifestFile);
    rewind(manifestFile);

    char *buffer = (char *)malloc(fileSize);
    if(!buffer)
    {
        fclose(manifestFile);
        fprintf(stderr, "Memory allocation failed for manifest content.\n");
        return FALSE;
    }

    fread(buffer, 1, fileSize, manifestFile);
    fclose(manifestFile);

    HANDLE hUpdateRes = BeginUpdateResourceA(output, FALSE);
    if(hUpdateRes == NULL)
    {
        fprintf(stderr, "Failed to open executable for update: %s\n", output);
        free(buffer);
        return FALSE;
    }

    BOOL result = UpdateResourceA(
        hUpdateRes,
        RT_MANIFEST,
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPVOID)buffer,
        (DWORD)fileSize
    );

    if(result == FALSE)
    {
        fprintf(stderr, "Failed to update manifest resource in executable: %s\n", output);
        free(buffer);
        EndUpdateResource(hUpdateRes, TRUE);
        return FALSE;
    }

    if(!EndUpdateResource(hUpdateRes, FALSE))
    {
        fprintf(stderr, "Failed to write changes to executable: %s\n", output);
        free(buffer);
        return FALSE;
    }

    free(buffer);
    return TRUE;
}

BOOL updateManifestFromMemory(const char *output, const char *manifestContent, int id)
{
    HANDLE hUpdateRes = BeginUpdateResourceA(output, FALSE);
    if(!hUpdateRes) return FALSE;

    BOOL result = UpdateResourceA(
        hUpdateRes,
        RT_MANIFEST,
        MAKEINTRESOURCEA(id),
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPVOID)manifestContent,
        (DWORD)strlen(manifestContent)
    );

    if(!result || !EndUpdateResource(hUpdateRes, FALSE))
    {
        return FALSE;
    }
    return TRUE;
}

char exePath[MAX_PATH];
HWND buildButton;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH hBrush;
    switch(uMsg)
    {
        case WM_CREATE:
            SetTimer(hwnd, 1, 500, NULL);
            break;
        case WM_COMMAND:
            if(LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED)
            {
                char msg[2048];
                sprintf(msg, "Your executable is %s, correct?", exePath);
                int q = MessageBox(hwnd, msg, "Manifest Builder", MB_YESNO | MB_ICONQUESTION);
                if(q == IDNO)
                    break;
                char manifestName[MAX_PATH];
                createManifest("temp.exe.manifest");
                if(updateManifest(exePath, "temp.exe.manifest"))
                    MessageBox(NULL, "Build successful!", "Manifest Builder", MB_OK | MB_ICONINFORMATION);
                else
                    MessageBox(NULL, "Failed to update executable.", "Error", MB_OK | MB_ICONERROR);
                remove("temp.exe.manifest");
            }
            else if(LOWORD(wParam) == 2 && HIWORD(wParam) == BN_CLICKED)
            {
                OPENFILENAMEA ofn;
                char szFile[MAX_PATH] = {0};

                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFile = szFile;
                ofn.lpstrFilter = "Application\0*.exe\0";
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrTitle = "Select your app";
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if(GetOpenFileNameA(&ofn))
                {
                    strcpy(exePath, szFile);
                }
                else
                {
                    DWORD dwError = CommDlgExtendedError();
                    if(dwError != 0)
                    {
                        char szErrorMessage[256];
                        snprintf(szErrorMessage, sizeof(szErrorMessage), "File dialog error: 0x%08X", dwError);
                        MessageBoxA(hwnd, szErrorMessage, "Error", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_PAINT:
            {
                PAINTSTRUCT ps;

                HDC hdc = BeginPaint(hwnd, &ps);
                HDC hdcMem = CreateCompatibleDC(hdc);

                FillRect(hdc, &ps.rcPaint, hBackgroundBrush);

                HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

                BITMAP bm;
                GetObject(hBitmap, sizeof(bm), &bm);
                BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

                SelectObject(hdcMem, hOldBitmap);
                DeleteDC(hdcMem);

                EndPaint(hwnd, &ps);
            }
            break;

        case WM_CTLCOLORSTATIC:
            {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(255, 0, 0));
                SetBkMode(hdcStatic, TRANSPARENT);

                if(!hBrush)
                    hBrush = CreateSolidBrush(RGB(18, 18, 18));

                return (INT_PTR)hBrush;
            }
            break;
        case WM_CTLCOLOREDIT:
            {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(255, 0, 0));
                SetBkMode(hdcStatic, TRANSPARENT);

                if(!hBrush)
                    hBrush = CreateSolidBrush(RGB(18, 18, 18));
                if(!hBackgroundBrush)
                    hBackgroundBrush = CreateSolidBrush(RGB(18, 18, 18));

                return (INT_PTR)hBrush;
            }
            break;
        case WM_CTLCOLORBTN:
            {
                HDC hdcStatic = (HDC)wParam;
                SetTextColor(hdcStatic, RGB(255, 0, 0));
                SetBkMode(hdcStatic, TRANSPARENT);

                if(!hBrush)
                    hBrush = CreateSolidBrush(RGB(18, 18, 18));
                if(!hBackgroundBrush)
                    hBackgroundBrush = CreateSolidBrush(RGB(18, 18, 18));

                return (INT_PTR)hBrush;
            }
            break;
        case WM_TIMER:
            if(strlen(exePath) == 0)
                EnableWindow(buildButton, FALSE);
            else
                EnableWindow(buildButton, TRUE);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int main(int argc, char **argv)
{
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    enable();
    const char CLASS_NAME[] = "Sample Window Class";

    WNDCLASSA wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowA(
            CLASS_NAME,
            "Manifest Builder",
            WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
            NULL,
            NULL,
            GetModuleHandle(NULL),
            NULL
            );

    if(hwnd == NULL)
        return 0;

    ShowWindow(hwnd, SW_SHOW);

    HWND credits = CreateWindowA(
            "STATIC",
            "Made by Strahinja Adamov (c) - 2024",
            WS_VISIBLE | WS_CHILD,
            3, 152, 300, 20,
            hwnd,
            NULL,
            GetModuleHandle(NULL),
            NULL
            );

    HWND title = CreateWindowA(
            "STATIC",
            "Manifest Builder 1.1",
            WS_VISIBLE | WS_CHILD,
            63, 10, 300, 20,
            hwnd,
            NULL,
            GetModuleHandle(NULL),
            NULL
            );

    buildButton= CreateWindowA(
            "BUTTON",
            "Build",
            WS_VISIBLE | WS_CHILD | BS_FLAT,
            100, 110, 100, 30, 
            hwnd,
            (HMENU)1,
            GetModuleHandle(NULL),
            NULL
            );

    HWND selectButton = CreateWindowA(
            "BUTTON",
            "Select",
            WS_VISIBLE | WS_CHILD | BS_FLAT,
            100, 60, 100, 30, 
            hwnd,
            (HMENU)2,
            GetModuleHandle(NULL),
            NULL
            );

    HFONT hFont = CreateFont(21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));

    HFONT cFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));

    HFONT titleFont = CreateFont(26, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Segoe UI"));

    SendMessage(buildButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(selectButton, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(credits, WM_SETFONT, (WPARAM)cFont, TRUE);
    SendMessage(title, WM_SETFONT, (WPARAM)titleFont, TRUE);

    EnableWindow(buildButton, FALSE);

    hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(69));
    if(!hBitmap)
    {
        MessageBox(NULL, "Failed to load bitmap!", "Error", MB_ICONERROR);
        return 0;
    }

    MSG msg = { };
    while(GetMessageA(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}
