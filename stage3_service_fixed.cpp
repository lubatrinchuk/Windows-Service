#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "advapi32.lib")

#define SERVICE_NAME "RoboShell"

SERVICE_STATUS g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = NULL;

void Log(const char* fmt, ...) {
    FILE* f = fopen("C:\\Windows\\Temp\\roboshell.log", "a");
    if (f) {
        va_list args;
        va_start(args, fmt);
        vfprintf(f, fmt, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

void WINAPI ServiceCtrlHandler(DWORD code) {
    switch (code) {
    case SERVICE_CONTROL_STOP:
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        SetEvent(g_ServiceStopEvent);
        return;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerA(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    Sleep(1000);

    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    Log("Service RoboShell is RUNNING");

    // ∆дЄм остановки
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
    CloseHandle(g_ServiceStopEvent);
    Log("Service stopped");
}

void InstallService() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    SC_HANDLE svc = CreateServiceA(scm, SERVICE_NAME, SERVICE_NAME,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);

    if (svc) {
        printf("Service INSTALLED\n");
        CloseServiceHandle(svc);
    }
    else {
        printf("CreateService failed (%d)\n", GetLastError());
    }
    CloseServiceHandle(scm);
}

void StartService() {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return;

    SC_HANDLE svc = OpenServiceA(scm, SERVICE_NAME, SERVICE_START);
    if (svc) {
        if (StartService(svc, 0, NULL)) {
            printf("Service STARTED\n");
        }
        else {
            printf("StartService failed (%d)\n", GetLastError());
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

void RemoveService() {
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) return;

    SC_HANDLE svc = OpenServiceA(scm, SERVICE_NAME, DELETE);
    if (svc) {
        if (DeleteService(svc)) {
            printf("Service REMOVED\n");
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (!strcmp(argv[1], "-install")) InstallService();
        else if (!strcmp(argv[1], "-start")) StartService();
        else if (!strcmp(argv[1], "-remove")) RemoveService();
        else printf("Usage: -install, -start, -remove\n");
        return 0;
    }

    SERVICE_TABLE_ENTRYA table[] = {
        {SERVICE_NAME, ServiceMain},
        {NULL, NULL}
    };

    StartServiceCtrlDispatcherA(table);
    return 0;
}
