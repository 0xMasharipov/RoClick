#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\build\stage"
#endif
[Setup]
AppId={{30612788-1F58-44DA-8C1A-BC32CE6E8029}
AppName=RoClick
AppVersion={#AppVersion}
AppPublisher=RoClick
AppPublisherURL=https://github.com/0xMasharipov/RoClick
DefaultDirName={localappdata}\Programs\RoClick
DefaultGroupName=RoClick
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64os
ArchitecturesInstallIn64BitMode=x64os
MinVersion=10.0.19045
OutputDir=..\dist
OutputBaseFilename=RoClick-Setup
SetupIconFile=..\assets\roclick.ico
UninstallDisplayIcon={app}\RoClick.exe
LicenseFile=..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#AppVersion}.0

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\RoClick"; Filename: "{app}\RoClick.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\RoClick.exe"; Description: "Open RoClick"; Flags: nowait postinstall skipifsilent
