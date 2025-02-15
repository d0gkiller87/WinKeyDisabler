> A utility to disable the Windows key when fullscreen is detected, while supporting combination keys (e.g., Win+Space).

---

## ✨Tray Icon Context Menu
<img src="/screenshots/main_v0.1.jpg">

## 🔧Installation
> [!NOTE]
> Download the latest builds from [GitHub Releases](https://github.com/d0gkiller87/WinKeyDisabler/releases)

## How It Works
1. The program uses `SetWindowsHookEx` to hook low-level keyboard inputs globally, without injecting DLLs into every process.
2. It calls `SHQueryUserNotificationState` every second (interval is currently hardcoded) to check if the user is in fullscreen.
3. When a Windows key down event is detected, the program blocks the input and enters the `winKeyPressed` state.
4. If any key is pressed while in the `winKeyPressed` state, the program replays the Windows key down event using `SendInput` to prevent key combinations from breaking. (`Win+D` (i.e. minimize all Windows) is disabled for now for obvious reasons)
5. When the Windows key up event is received, the `winKeyPressed` state is reset.

## Will This Get Me Banned?
If a legitimate key-remapping AHK (AutoHotkey) script doesn't get you banned, this likely won't either.  
BUT! Nothing is guaranteed! Use at your own risk!

## Credits
- [Flaticon - Windows](https://www.flaticon.com/free-icon/windows_882600) for the awesome icon.
- https://stackoverflow.com/a/58755620/9182265 for providing a realiable way to detect full screen states.
- Collabration from ChatGPT & Claude for heavy lifting 90% of the code.

## License
This project is licensed under the **MIT License**. Review [LICENSE](/LICENSE) for furthur details.
