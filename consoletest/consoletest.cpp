#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <iostream>
#include <thread>

HHOOK hKeyHook = NULL;
bool disableWinKey = false;
bool autostartEnabled = false;
const LPCWSTR appName = L"WinKeyDisablerConsole";

bool winKeyPressed = false;
bool spacePressedDuringWin = false;

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam ) {
  if ( nCode == HC_ACTION && disableWinKey ) {
    KBDLLHOOKSTRUCT* pKey = ( KBDLLHOOKSTRUCT* ) lParam;

    bool isWinKey = ( pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN );
    bool isKeyDown = ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN );
    bool isKeyUp = ( wParam == WM_KEYUP || wParam == WM_SYSKEYUP );

    if ( isWinKey ) {
      if ( isKeyDown ) {
        if ( !winKeyPressed ) {
          winKeyPressed = true;
          puts( "Blocked Win key down" );
          return 1;
        }
      } else if ( isKeyUp ) {
        winKeyPressed = false;
        puts( "Allowed Win key up" );
      }
    } else { // not Win key
      if ( winKeyPressed ) {
        INPUT inputs[2] = {};

        // Resend Win key down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LWIN;
        inputs[0].ki.dwFlags = 0;

        SendInput( 1, inputs, sizeof( INPUT ) );
        puts( "Resent Win key down" );
      }
    }
  }
  return CallNextHookEx( hKeyHook, nCode, wParam, lParam );
}

// Check for fullscreen app using SHQueryUserNotificationState
bool IsFullscreenApp() {
  QUERY_USER_NOTIFICATION_STATE notificationState;
  if (SHQueryUserNotificationState(&notificationState) == S_OK) {
    switch (notificationState) {
    case QUNS_BUSY:
    case QUNS_RUNNING_D3D_FULL_SCREEN:
    case QUNS_PRESENTATION_MODE:
    //case QUNS_APP:
      std::cout << "[LOG] Fullscreen detected (Notification State: " << notificationState << ")\n";
      return true;
    default:
      return false;
    }
  }
  return false;
}

// Toggle autostart via registry
void ToggleAutostart() {
  HKEY hKey;
  const LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
    if (autostartEnabled) {
      RegDeleteValue(hKey, appName);
      autostartEnabled = false;
      std::cout << "[LOG] Autostart disabled\n";
    } else {
      wchar_t exePath[MAX_PATH];
      GetModuleFileName(NULL, exePath, MAX_PATH);
      RegSetValueEx(hKey, appName, 0, REG_SZ, (BYTE*)exePath, wcslen(exePath) + 1);
      autostartEnabled = true;
      std::cout << "[LOG] Autostart enabled\n";
    }
    RegCloseKey(hKey);
  }
}

// Console control handler for graceful exit
BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
    std::cout << "[LOG] Exiting...\n";
    if (hKeyHook) UnhookWindowsHookEx(hKeyHook);
    ExitProcess(0);
  }
  return TRUE;
}

// Thread for fullscreen detection
void FullscreenDetectionThread() {
  while (true) {
    bool currentFullscreen = IsFullscreenApp();

    if (currentFullscreen && !disableWinKey) {
      disableWinKey = true;
      std::cout << "[LOG] Windows key disabled\n";
    } else if (!currentFullscreen && disableWinKey) {
      disableWinKey = false;
      std::cout << "[LOG] Windows key enabled\n";
    }

    Sleep(1000);  // Poll every second
  }
}

int main() {
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);

  std::cout << "[LOG] WinKey Disabler Console Started\n";
  std::cout << "[LOG] Press Ctrl+C to exit\n";

  // Set the keyboard hook
  hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
  if (!hKeyHook) {
    std::cerr << "[ERROR] Failed to set keyboard hook\n";
    return 1;
  }

  // Start the fullscreen detection thread
  std::thread detectionThread(FullscreenDetectionThread);
  detectionThread.detach();

  // Message loop to keep the application running
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return 0;
}
