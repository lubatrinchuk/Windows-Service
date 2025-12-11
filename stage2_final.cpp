#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define PIPE_BUFFER_SIZE 4096
#define SERVER_PORT 9999

bool CreatePipesForChild(HANDLE *hReadPipe, HANDLE *hWritePipe) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(hReadPipe, hWritePipe, &sa, 0)) {
        printf("[ERROR] CreatePipe failed\n");
        return false;
    }

    if (!SetHandleInformation(*hWritePipe, HANDLE_FLAG_INHERIT, 0)) {
        printf("[ERROR] SetHandleInformation failed\n");
        return false;
    }

    return true;
}

bool StartChildProcess(HANDLE hStdinRead, HANDLE hStdoutWrite, PROCESS_INFORMATION *ppi) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFOW));
    si.cb = sizeof(STARTUPINFOW);
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.hStdInput = hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES;


    WCHAR commandLine[] = L"C:\\Users\\dev\\Documents\\echo_server.exe";

    printf("[DEBUG] Starting: %ls\n", commandLine);
    if (!CreateProcessW(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, ppi)) {
        printf("[ERROR] CreateProcess failed: %ld\n", GetLastError());
        return false;
    }

    printf("[OK] Process started (PID: %ld)\n", ppi->dwProcessId);
    return true;
}

bool InitializeWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("[ERROR] WSAStartup failed\n");
        return false;
    }
    return true;
}

SOCKET CreateServerSocket(unsigned short port) {
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        printf("[ERROR] socket() failed\n");
        return INVALID_SOCKET;
    }

    struct sockaddr_in serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("[ERROR] bind() failed\n");
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    if (listen(listenSocket, 5) == SOCKET_ERROR) {
        printf("[ERROR] listen() failed\n");
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }

    printf("[OK] Server listening on port %d\n", port);
    return listenSocket;
}

SOCKET CreateClientSocket(const char *serverIp, unsigned short port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(serverIp);
    addr.sin_port = htons(port);

    if (connect(s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[ERROR] connect() failed\n");
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

typedef struct {
    HANDLE hPipe;
    SOCKET sock;
    const char *name;
} ThreadArgs;

unsigned int __stdcall PipeToSocketThread(void *arg) {
    ThreadArgs *a = (ThreadArgs *)arg;
    char buf[PIPE_BUFFER_SIZE];
    DWORD n;

    printf("[%s] started\n", a->name);
    
    while (ReadFile(a->hPipe, buf, sizeof(buf), &n, NULL)) {
        if (n == 0) {
            Sleep(50);
            continue;
        }

        int sent = 0;
        while (sent < (int)n) {
            int res = send(a->sock, buf + sent, n - sent, 0);
            if (res == SOCKET_ERROR) {
                printf("[%s] send error\n", a->name);
                return 1;
            }
            sent += res;
        }
        printf("[%s] sent %d bytes\n", a->name, n);
    }

    printf("[%s] exiting\n", a->name);
    return 0;
}

unsigned int __stdcall SocketToPipeThread(void *arg) {
    ThreadArgs *a = (ThreadArgs *)arg;
    char buf[PIPE_BUFFER_SIZE];
    int n;
    DWORD written;

    printf("[%s] started\n", a->name);

    while ((n = recv(a->sock, buf, sizeof(buf), 0)) > 0) {
        int written_total = 0;
        while (written_total < n) {
            if (!WriteFile(a->hPipe, buf + written_total, n - written_total, &written, NULL)) {
                printf("[%s] write error\n", a->name);
                return 1;
            }
            written_total += written;
        }
        printf("[%s] wrote %d bytes\n", a->name, n);
    }

    printf("[%s] exiting\n", a->name);
    return 0;
}

void RunServer() {
    printf("=== SERVER START ===\n");

    if (!InitializeWinsock()) return;

    SOCKET listenSocket = CreateServerSocket(SERVER_PORT);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    printf("[WAIT] client...\n");
    SOCKET clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        printf("[ERROR] accept failed\n");
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    printf("[OK] client connected\n");
    closesocket(listenSocket);

    HANDLE hStdinRead, hStdinWrite, hStdoutRead, hStdoutWrite;
    PROCESS_INFORMATION pi;

    if (!CreatePipesForChild(&hStdinRead, &hStdinWrite)) {
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    if (!CreatePipesForChild(&hStdoutRead, &hStdoutWrite)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    if (!StartChildProcess(hStdinRead, hStdoutWrite, &pi)) {
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        closesocket(clientSocket);
        WSACleanup();
        return;
    }

    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);

    ThreadArgs args1 = { hStdoutRead, clientSocket, "out" };
    ThreadArgs args2 = { hStdinWrite, clientSocket, "in" };

    printf("[THREADS] creating...\n");
    HANDLE t1 = (HANDLE)_beginthreadex(NULL, 0, PipeToSocketThread, &args1, 0, NULL);
    HANDLE t2 = (HANDLE)_beginthreadex(NULL, 0, SocketToPipeThread, &args2, 0, NULL);

    printf("[OK] threads created\n");

    WaitForSingleObject(pi.hProcess, INFINITE);

    printf("[OK] process ended\n");

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    closesocket(clientSocket);
    WSACleanup();

    printf("=== SERVER END ===\n");
}

void RunClient(const char* ip) {
    printf("=== CLIENT START ===\n");

    if (!InitializeWinsock()) return;

    SOCKET s = CreateClientSocket(ip, SERVER_PORT);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    printf("[OK] connected\n");
    printf("Type something:\n");

    char buf[4096];
    while (fgets(buf, sizeof(buf), stdin)) {
        int len = strlen(buf);
        
        if (send(s, buf, len, 0) == SOCKET_ERROR) {
            printf("[ERROR] send\n");
            break;
        }

        Sleep(100);
        int n = recv(s, buf, sizeof(buf)-1, 0);
        if (n <= 0) break;

        buf[n] = 0;
        printf("%s", buf);
    }

    closesocket(s);
    WSACleanup();
    printf("=== CLIENT END ===\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-s") == 0) {
        RunServer();
    } else if (argc > 2 && strcmp(argv[1], "-c") == 0) {
        RunClient(argv[2]);
    } else {
        printf("Usage: my.exe -s  or  my.exe -c <ip>\n");
    }
    return 0;
}
