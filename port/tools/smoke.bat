@echo off
set ROOT=D:\Sandbox Cursor\RADIANT DEFENCE
set JAVA=%ROOT%\_tools\jdk17\jdk-17.0.20.1+1\bin\java.exe
mkdir "%ROOT%\port\build\smoke\logs" 2>nul
mkdir "%ROOT%\port\build\smoke\files" 2>nul
mkdir "%ROOT%\port\build\smoke\cache" 2>nul
"%JAVA%" -Xmx192m -Xverify:none -Djava.library.path="%ROOT%\port\build" -Dport.home="%ROOT%\port\build\smoke" -Dport.apk="%ROOT%\Radiant_Defense_v.2.3.15.Unlocked.Rus.apk" -Dport.exitms=15000 -Duser.language=ru -Duser.country=RU -cp "%ROOT%\port\build\shim.jar;%ROOT%\port\build\radiant.jar" port.Main
echo JAVA_EXIT %ERRORLEVEL%
