#include <iostream>
#include <windows.h>
#include <vector>
#include <string>
#include <chrono>
#include <TCHAR.h>
#include <pdh.h>

using namespace std;

static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;

string getTime() {
    auto start = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(start);
    return std::asctime(std::localtime(&time));
}

void initCPUMeasurement() {
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    PdhAddEnglishCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
}

double getCPUUsage() {
    PDH_FMT_COUNTERVALUE counterVal;

    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    return counterVal.doubleValue;
}

DWORDLONG getMem() {
    MEMORYSTATUSEX memInfo{};
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    DWORDLONG physMemUsed = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    return physMemUsed;
}

int main()
{
    initCPUMeasurement();

    SECURITY_ATTRIBUTES sa = { sizeof(sa), 0, TRUE };

    HANDLE h = CreateFileA("GPUMonitoring.txt",
                FILE_APPEND_DATA,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                &sa,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

    PROCESS_INFORMATION pi;
    STARTUPINFOA si{};
    BOOL ret = FALSE;
    DWORD flags = CREATE_NO_WINDOW;

    si.cb = sizeof(STARTUPINFO);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = NULL;
    si.hStdError = h;
    si.hStdOutput = h;

    vector<string> cmd;
    LPCSTR cmd1 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=temperature.gpu --format=csv,noheader";
    LPCSTR cmd2 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=power.draw --format=csv,noheader";
    LPCSTR cmd3 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=utilization.gpu --format=csv,noheader";
    LPCSTR cmd4 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=memory.used --format=csv,noheader";
    LPCSTR cmd5 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=clocks.gr --format=csv,noheader";
    LPCSTR cmd6 = "C:\\Windows\\System32\\nvidia-smi.exe --query-gpu=pstate --format=csv,noheader";

    cmd.push_back(cmd1);
    cmd.push_back(cmd2);
    cmd.push_back(cmd3);
    cmd.push_back(cmd4);
    cmd.push_back(cmd5);
    cmd.push_back(cmd6);

    string header = "CPU, Mem, GPUTemp, Power, Use, Memory, Clock, pstate, Timestamp\n";
    WriteFile(h, header.c_str(), header.size(), NULL, NULL);
    
    while (true) {

        string firstMetrics = to_string(getCPUUsage()) + "," + to_string(getMem()) + ",";

        WriteFile(h, firstMetrics.c_str(), firstMetrics.size(), NULL, NULL);

        for (int i = 0; i < cmd.size(); i++) {
            CreateProcessA(NULL, (LPSTR)cmd[i].c_str(), NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi);
            WaitForSingleObject(pi.hProcess, INFINITE);

            if (i < cmd.size() - 1)
                WriteFile(h, ",", 1, NULL, NULL);
            else {
                string timeStr = "," + getTime();
                WriteFile(h, timeStr.c_str(), timeStr.size(), NULL, NULL);
            }
        }

        WriteFile(h, "\n", 1, NULL, NULL);

        Sleep(10000);
    }
    
    if (ret)
    {
        CloseHandle(h);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 0;
    }

}
