# LOTR BFME II Network fixer

Injects selected application with a DLL wich hooks gethostbyname() wsock2 function. The hook intercepts the IP list and removes all entries except the wanted one.
This way the application will only know about one network interface. This solution will not work if the application uses other means to get list of IP addresses.

Initially this project was developed to fix LAN multiplayer in LOTR BFME 2 game, however it can be repurposed for any other game or app which uses the same IP detection method.
The problem with LOTR is that if you have multiple network adapters, the game will pick the first one. Since the order of adapters is impossible to control, game can potentially bind to wrong network interface rendering LAN multiplayer impossible or inconvenient. NetwokFixer fixes this issue by allowing the user to manually select the correct network interface.

Target application's files are not modified in any way, all changes are applied to the process memory.

# Usage For LOTR BFME II and RotWK addon

1. Download NetworkFixerLauncher.x86.exe and NetworkFixer.x86.dll
2. Place both files in the same directory. No need to place the files in the game installation directory.
3. Run NetworkFixerLauncher.x86.exe as administrator
4. Select desired IP address
5. Run the game
6. The Launcher will automatically inject the game process. Make sure you see no errors in the launcher's log.

# Advanced usage

Run NetworkFixerLauncher[.x86].exe --help to see available arguments.

Make sure to use the correct bitness! x86 version will not work with x64 target and vice versa.
