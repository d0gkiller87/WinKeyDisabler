#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <filesystem>
#include <sstream>
#include <thread>
#include "resource.h"

// Debug logging macro
#ifdef _DEBUG
#define DEBUG_LOG( str ) OutputDebugStringA( "[WinKeyDisabler] " ); OutputDebugStringA( str ); OutputDebugStringA( "\n" )
#else
#define DEBUG_LOG( str )
#endif

// Constants
const UINT WM_TRAYICON = WM_USER + 1;
const UINT IDM_AUTOSTART = 1002;
const UINT IDM_BUSY = 1003;
const UINT IDM_D3D = 1004;
const UINT IDM_PRESENTATION = 1005;
const LPCWSTR APP_NAME = L"WinKeyDisabler";
const LPCWSTR WINDOW_CLASS = L"WinKeyDisablerClass";

// Global variables
HWND g_hwnd = NULL;
HHOOK g_keyHook = NULL;
NOTIFYICONDATA g_nid = {};
bool g_disableWinKey = false;
bool g_autostartEnabled = false;
HICON g_hIcon = NULL;

// Configuration flags
struct Config {
  bool detectBusy = true;
  bool detectD3D = true;
  bool detectPresentation = true;
} g_config;

// Get configuration file path
std::wstring GetConfigPath() {
  wchar_t exePath[MAX_PATH];
  GetModuleFileName( NULL, exePath, MAX_PATH );
  std::wstring path( exePath );
  size_t pos = path.find_last_of( L"\\/" );
  return path.substr( 0, pos + 1 ) + L"WinKeyDisabler_config.ini";
}

// Load configuration from INI file
void LoadConfig() {
  std::wstring configPath = GetConfigPath();
  g_config.detectBusy = GetPrivateProfileInt( L"Settings", L"DetectBusy", 1, configPath.c_str() ) != 0;
  g_config.detectD3D = GetPrivateProfileInt( L"Settings", L"DetectD3D", 1, configPath.c_str() ) != 0;
  g_config.detectPresentation = GetPrivateProfileInt( L"Settings", L"DetectPresentation", 1, configPath.c_str() ) != 0;
  DEBUG_LOG( "Configuration loaded" );
}

// Save configuration to INI file
void SaveConfig() {
  std::wstring configPath = GetConfigPath();
  WritePrivateProfileString(
    L"Settings", L"DetectBusy",
    g_config.detectBusy ? L"1" : L"0",
    configPath.c_str()
  );
  WritePrivateProfileString(
    L"Settings", L"DetectD3D",
    g_config.detectD3D ? L"1" : L"0",
    configPath.c_str()
  );
  WritePrivateProfileString(
    L"Settings", L"DetectPresentation",
    g_config.detectPresentation ? L"1" : L"0",
    configPath.c_str()
  );
  DEBUG_LOG( "Configuration saved" );
}

bool winKeyPressed = false;

LRESULT CALLBACK LowLevelKeyboardProc( int nCode, WPARAM wParam, LPARAM lParam ) {
  if ( nCode == HC_ACTION && g_disableWinKey ) {
    KBDLLHOOKSTRUCT* pKey = ( KBDLLHOOKSTRUCT* ) lParam;

    bool isWinKey = ( pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN );
    bool isKeyDown = ( wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN );
    bool isKeyUp = ( wParam == WM_KEYUP || wParam == WM_SYSKEYUP );
     
    if ( isWinKey ) {
      if ( isKeyDown ) {
        if ( !winKeyPressed ) {
          winKeyPressed = true;
          DEBUG_LOG( "Blocked Win key down" );
          return 1;
        }
      } else if ( isKeyUp ) {
        winKeyPressed = false;
        DEBUG_LOG( "Allowed Win key up" );
      }
    } else { // not Win key
      if ( winKeyPressed ) {
        INPUT inputs[1] = {};

        // Resend Win key down
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = VK_LWIN;
        inputs[0].ki.dwFlags = 0;

        SendInput( 1, inputs, sizeof( INPUT ) );
        DEBUG_LOG( "Resent Win key down" );
      }
    }
  }
  return CallNextHookEx( g_keyHook, nCode, wParam, lParam );
}

// Check for fullscreen app
bool IsFullscreenApp() {
  // https://stackoverflow.com/a/58755620/9182265
  QUERY_USER_NOTIFICATION_STATE notificationState;
  if ( SUCCEEDED( SHQueryUserNotificationState( &notificationState ) ) ) {
    switch ( notificationState ) {
      case QUNS_BUSY:
        return g_config.detectBusy;
      case QUNS_RUNNING_D3D_FULL_SCREEN:
        return g_config.detectD3D;
      case QUNS_PRESENTATION_MODE:
        return g_config.detectPresentation;
      default:
        return false;
    }
  }
  return false;
}

// Initialize tray icon
void InitTrayIcon( HWND hwnd, HINSTANCE hInstance ) {
  g_hIcon = LoadIcon( hInstance, MAKEINTRESOURCE( IDI_TRAYICON ) );
  if ( !g_hIcon ) {
    DEBUG_LOG( "Failed to load custom icon, using default" );
    g_hIcon = LoadIcon( NULL, IDI_APPLICATION );
  }

  g_nid.cbSize = sizeof( NOTIFYICONDATA );
  g_nid.hWnd = hwnd;
  g_nid.uID = 1;
  g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  g_nid.uCallbackMessage = WM_TRAYICON;
  g_nid.hIcon = g_hIcon;
  wcscpy_s( g_nid.szTip, L"WinKey Disabler" );
  Shell_NotifyIcon( NIM_ADD, &g_nid );
}

// Show context menu
void ShowContextMenu( HWND hwnd, POINT pt ) {
  HMENU hMenu = CreatePopupMenu();

  // Check autostart status
  HKEY hKey;
  if (
    RegOpenKeyEx(
      HKEY_CURRENT_USER,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      0,
      KEY_READ,
      &hKey
    ) == ERROR_SUCCESS
  ) {
    wchar_t value[MAX_PATH];
    DWORD valueSize = sizeof( value );
    g_autostartEnabled = ( RegQueryValueEx( hKey, APP_NAME, 0, NULL,
                           ( LPBYTE ) value, &valueSize ) == ERROR_SUCCESS );
    RegCloseKey( hKey );
  }

  // Add menu items
  InsertMenu( hMenu, -1, MF_BYPOSITION | ( g_autostartEnabled ? MF_CHECKED : MF_UNCHECKED ),
              IDM_AUTOSTART, L"Run at Startup" );

  // Add separator
  InsertMenu( hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );

  // Add detection mode toggles
  InsertMenu( hMenu, -1, MF_BYPOSITION | ( g_config.detectBusy ? MF_CHECKED : MF_UNCHECKED ),
              IDM_BUSY, L"Lock General Fullscreen" );
  InsertMenu( hMenu, -1, MF_BYPOSITION | ( g_config.detectD3D ? MF_CHECKED : MF_UNCHECKED ),
              IDM_D3D, L"Lock D3D Fullscreen" );
  InsertMenu( hMenu, -1, MF_BYPOSITION | ( g_config.detectPresentation ? MF_CHECKED : MF_UNCHECKED ),
              IDM_PRESENTATION, L"Lock Presentation Mode" );

  // Add separator and exit
  InsertMenu( hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL );
  InsertMenu( hMenu, -1, MF_BYPOSITION, IDM_EXIT, L"Exit" );

  SetForegroundWindow( hwnd );
  TrackPopupMenu( hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                  pt.x, pt.y, 0, hwnd, NULL );
  DestroyMenu( hMenu );
}

// Fullscreen detection thread
void FullscreenDetectionThread() {
  while ( true ) {
    bool currentFullscreen = IsFullscreenApp();

    if ( currentFullscreen && !g_disableWinKey ) {
      g_disableWinKey = true;
      DEBUG_LOG( "Windows key disabled" );
    } else if ( !currentFullscreen && g_disableWinKey ) {
      g_disableWinKey = false;
      DEBUG_LOG( "Windows key enabled" );
    }

    Sleep( 1000 );  // Poll every second
  }
}

// Toggle autostart via registry
void ToggleAutostart() {
  HKEY hKey;
  const LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
  if ( RegOpenKeyEx( HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey ) == ERROR_SUCCESS ) {
    if ( g_autostartEnabled ) {
      RegDeleteValue( hKey, APP_NAME );
      g_autostartEnabled = false;
      DEBUG_LOG( "Autostart disabled" );
    } else {
      wchar_t exePath[MAX_PATH];
      GetModuleFileName( NULL, exePath, MAX_PATH );
      RegSetValueEx(
        hKey,
        APP_NAME,
        0,
        REG_SZ,
        ( BYTE* ) exePath,
        ( wcslen( exePath ) + 1 ) * sizeof( wchar_t )
      );
      g_autostartEnabled = true;
      DEBUG_LOG( "Autostart enabled" );
    }
    RegCloseKey( hKey );
  }
}

// Cleanup resources
void Cleanup() {
  if ( g_keyHook ) {
    UnhookWindowsHookEx( g_keyHook );
    g_keyHook = NULL;
  }
  Shell_NotifyIcon( NIM_DELETE, &g_nid );
  if ( g_hIcon ) {
    DestroyIcon( g_hIcon );
    g_hIcon = NULL;
  }
  SaveConfig();  // Save configuration before exit
  DEBUG_LOG( "Application cleanup complete" );
}

// Window procedure
LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam ) {
  switch ( msg ) {
  case WM_TRAYICON:
    if ( lParam == WM_RBUTTONUP ) {
      POINT pt;
      GetCursorPos( &pt );
      ShowContextMenu( hwnd, pt );
    }
    break;

  case WM_COMMAND:
    switch ( LOWORD( wParam ) ) {
    case IDM_EXIT:
      DestroyWindow( hwnd );
      break;
    case IDM_AUTOSTART:
      ToggleAutostart();
      break;
    case IDM_BUSY:
      g_config.detectBusy = !g_config.detectBusy;
      SaveConfig();
      break;
    case IDM_D3D:
      g_config.detectD3D = !g_config.detectD3D;
      SaveConfig();
      break;
    case IDM_PRESENTATION:
      g_config.detectPresentation = !g_config.detectPresentation;
      SaveConfig();
      break;
    }
    break;

  case WM_DESTROY:
    Cleanup();
    PostQuitMessage( 0 );
    break;

  default:
    return DefWindowProc( hwnd, msg, wParam, lParam );
  }
  return 0;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
  // Load configuration
  if ( std::filesystem::exists( GetConfigPath() ) ) {
    LoadConfig();
  } else {
    SaveConfig();
  }

  // Register window class
  WNDCLASSEX wc = {};
  wc.cbSize = sizeof( WNDCLASSEX );
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = WINDOW_CLASS;
  RegisterClassEx( &wc );

  // Create hidden window
  g_hwnd = CreateWindowEx(
    0, WINDOW_CLASS, APP_NAME, 0,
    0, 0, 0, 0, NULL, NULL, hInstance, NULL
  );

  if ( !g_hwnd ) {
    DEBUG_LOG( "Failed to create window" );
    return 1;
  }

  // Initialize keyboard hook
  g_keyHook = SetWindowsHookEx( WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0 );
  if ( !g_keyHook ) {
    DEBUG_LOG( "Failed to set keyboard hook" );
    return 1;
  }

  // Initialize tray icon
  InitTrayIcon( g_hwnd, hInstance );
  DEBUG_LOG( "Application started" );

  // Start fullscreen detection thread
  std::thread detectionThread( FullscreenDetectionThread );
  detectionThread.detach();

  // Message loop
  MSG msg;
  while ( GetMessage( &msg, NULL, 0, 0 ) ) {
    TranslateMessage( &msg );
    DispatchMessage( &msg );
  }

  return 0;
}
