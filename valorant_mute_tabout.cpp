// valorant_mute_tabout.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <psapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <iostream>
#include <string>
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Ole32.lib")

bool IsValorantActive() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        WCHAR processName[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, NULL, processName, MAX_PATH)) {
            std::wstring name = processName;
            if (name.find(L"VALORANT-Win64-Shipping.exe") != std::wstring::npos) {
                CloseHandle(hProcess);
                return true;
            }
        }
        CloseHandle(hProcess);
    }
    return false;
}

DWORD GetValorantPID() {
    DWORD pid = 0;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (std::wstring(entry.szExeFile) == L"VALORANT-Win64-Shipping.exe") {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

void SetProcessMute(DWORD pid, bool mute) {
    CoInitialize(NULL);

    IMMDeviceEnumerator* pDeviceEnumerator = NULL;
    IMMDevice* pDevice = NULL;
    IAudioSessionManager2* pAudioSessionManager = NULL;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pDeviceEnumerator);

    if (SUCCEEDED(hr)) {
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(
            eRender, eMultimedia, &pDevice);
    }

    if (SUCCEEDED(hr)) {
        hr = pDevice->Activate(
            __uuidof(IAudioSessionManager2), CLSCTX_ALL,
            NULL, (void**)&pAudioSessionManager);
    }

    if (SUCCEEDED(hr)) {
        IAudioSessionEnumerator* pSessionEnumerator = NULL;
        hr = pAudioSessionManager->GetSessionEnumerator(&pSessionEnumerator);
        if (SUCCEEDED(hr)) {
            int sessionCount = 0;
            pSessionEnumerator->GetCount(&sessionCount);
            for (int i = 0; i < sessionCount; i++) {
                IAudioSessionControl* pSessionControl = NULL;
                hr = pSessionEnumerator->GetSession(i, &pSessionControl);
                if (SUCCEEDED(hr)) {
                    IAudioSessionControl2* pSessionControl2 = NULL;
                    hr = pSessionControl->QueryInterface(
                        __uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                    if (SUCCEEDED(hr)) {
                        DWORD sessionPid = 0;
                        pSessionControl2->GetProcessId(&sessionPid);
                        if (sessionPid == pid) {
                            ISimpleAudioVolume* pSimpleAudioVolume = NULL;
                            hr = pSessionControl2->QueryInterface(
                                __uuidof(ISimpleAudioVolume), (void**)&pSimpleAudioVolume);
                            if (SUCCEEDED(hr)) {
                                pSimpleAudioVolume->SetMute(mute, NULL);
                                pSimpleAudioVolume->Release();
                            }
                        }
                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
            }
            pSessionEnumerator->Release();
        }
    }

    if (pAudioSessionManager) pAudioSessionManager->Release();
    if (pDevice) pDevice->Release();
    if (pDeviceEnumerator) pDeviceEnumerator->Release();

    CoUninitialize();
}

int main() {
    DWORD pid = GetValorantPID();
    if (!pid) {
        std::cout << "Valorant is not running." << std::endl;
        return 0;
    }

    bool isMuted = false;
    while (true) {
        bool isActive = IsValorantActive();
        if (!isActive && !isMuted) {
            SetProcessMute(pid, true);
            isMuted = true;
        } else if (isActive && isMuted) {
            SetProcessMute(pid, false);
            isMuted = false;
        }
        Sleep(500);
    }
    return 0;
}