# LOTR BFME II Network fixer

Injects selected application with a DLL wich hooks gethostbyname() wsock2 function. The hook intercepts the IP list and removes all entries except the wanted one.
This way the application will only know about one network inreface. This solution will not work if the application uses other means to get list of IP addresses.

Initially this project was developed to fix LAN multiplayer in LOTR BFME 2 game, however it can be repurposed for any other game or app which uses the same IP detection method.

# Usage For LOTR BFME II and RotWK addon

1. Download NetworkFixerLauncher.x86.exe and NetworkFixer.x86.dll
2. Place both files in the same directory
3. Run NetworkFixerLauncher.x86.exe as administrator
4. Select desired IP address
5. Run the game
6. The Launcher will automatically inject the game process. Make sure you see no errors in the launcher's log.

# Advanced usage

Run NetworkFixerLauncher[.x86].exe --help to see available arguments.
   
