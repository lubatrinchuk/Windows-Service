#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PIPE_BUFFER_SIZE 4096

bool CreatePipesForChild(HANDLE *hReadPipe, HANDLE *hWritePipe) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(hReadPipe, hWritePipe, &sa, 0)) {
        printf("[ERROR] CreatePipe failed: %ld\n", GetLastError());
        return false;
    }

    if (!SetHandleInformation(*hWritePipe, HANDLE_FLAG_INHERIT, 0)) {
        printf("[ERROR] SetHandleInformation failed: %ld\n", GetLastError());
        return false;
    }

    return true;
}

bool StartChildProcess(HANDLE hStdinRead, HANDLE hStdoutWrite, PROCESS_INFORMATION *ppi) {
    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(STARTUPINFOW));
    si.cb = sizeof(STARTUPINFOW);
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    WCHAR commandLine[] = L"cmd.exe";

    printf("[STAGE1] Creating child process: cmd.exe\n");
    if (!CreateProcessW(NULL, commandLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, ppi)) {
        printf("[ERROR] CreateProcess failed: %ld\n", GetLastError());
        return false;
    }

    printf("[OK] Child process created with PID: %ld\n", ppi->dwProcessId);
    return true;
}

int main() {
    printf("=== STAGE 1: Pipes + Child Process ===\n");

    HANDLE hStdinRead, hStdinWrite;
    HANDLE hStdoutRead, hStdoutWrite;
    PROCESS_INFORMATION pi;
    char buffer[PIPE_BUFFER_SIZE];
    DWORD bytesRead, bytesWritten;

    // Create pipes for stdin (parent -> child)
    if (!CreatePipesForChild(&hStdinRead, &hStdinWrite)) {
        printf("[ERROR] Failed to create stdin pipes\n");
        return 1;
    }
    printf("[OK] stdin pipes created\n");

    // Create pipes for stdout (child -> parent)
    if (!CreatePipesForChild(&hStdoutRead, &hStdoutWrite)) {
        printf("[ERROR] Failed to create stdout pipes\n");
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        return 1;
    }
    printf("[OK] stdout pipes created\n");

    // Start child process
    if (!StartChildProcess(hStdinRead, hStdoutWrite, &pi)) {
        printf("[ERROR] Failed to start child process\n");
        CloseHandle(hStdinRead);
        CloseHandle(hStdinWrite);
        CloseHandle(hStdoutRead);
        CloseHandle(hStdoutWrite);
        return 1;
    }

    // Close ONLY the child's write ends that parent doesn't use
    CloseHandle(hStdoutWrite);  // Child has this, parent doesn't write to stdout
    // DO NOT CLOSE hStdinRead - child needs it to read our commands!

    printf("[OK] Parent-child pipes connected\n");
    printf("Type commands (exit to quit):\n\n");

    // Simple interactive loop
    while (true) {
        printf("> ");
        fflush(stdout);

        // Read from user
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;
        }

        int cmdLen = strlen(buffer);

        // Send command to child via pipe
        if (!WriteFile(hStdinWrite, buffer, cmdLen, &bytesWritten, NULL)) {
            printf("[ERROR] WriteFile failed: %ld\n", GetLastError());
            break;
        }

        printf("[DEBUG] Sent %d bytes\n", bytesWritten);

        // Give child time to process and output
        Sleep(300);

        // Read response from child via pipe
        if (!ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
            printf("[ERROR] ReadFile failed: %ld\n", GetLastError());
            break;
        }

        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            printf("%s", buffer);
        } else {
            printf("[INFO] No output from child\n");
        }
    }

    printf("\n[STAGE1] Closing...\n");
    FlushFileBuffers(hStdinWrite);
    CloseHandle(hStdinWrite);
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutRead);
    
    // Give process time to finish
    Sleep(500);
    
    TerminateProcess(pi.hProcess, 0);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("[OK] Done\n");
    return 0;
}