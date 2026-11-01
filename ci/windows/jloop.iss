; Installer di JLoop per Windows (Inno Setup 6). Compilato da GitHub Actions:
;   ISCC.exe /DMyAppVersion=0.59.0 /DSourceExe=...\JLoop.exe ci\windows\jloop.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\..\Builds\VisualStudio2022\x64\Release\App\JLoop.exe"
#endif

[Setup]
AppId={{6E3B1C2A-4F2D-4C7B-9C1E-7A5D2B8F0A11}
AppName=JLoop
AppVersion={#MyAppVersion}
AppVerName=JLoop {#MyAppVersion}
AppPublisher=joEsb
DefaultDirName={autopf}\JLoop
DefaultGroupName=JLoop
DisableProgramGroupPage=yes
; Si installa anche senza permessi da amministratore (solo per l'utente), oppure per tutti se si sceglie
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\dist
OutputBaseFilename=JLoop-Setup
SetupIconFile=jloop.ico
UninstallDisplayIcon={app}\JLoop.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop"; GroupDescription: "Icone:"
Name: "assoc"; Description: "Apri i progetti .jloop con JLoop (doppio clic)"; GroupDescription: "File:"

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\JLoop"; Filename: "{app}\JLoop.exe"
Name: "{autodesktop}\JLoop"; Filename: "{app}\JLoop.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.jloop"; ValueType: string; ValueName: ""; ValueData: "JLoop.Progetto"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.dawfl"; ValueType: string; ValueName: ""; ValueData: "JLoop.Progetto"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto"; ValueType: string; ValueName: ""; ValueData: "Progetto JLoop"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\JLoop.exe,0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\JLoop.exe"" ""%1"""; Tasks: assoc

[Run]
Filename: "{app}\JLoop.exe"; Description: "Avvia JLoop"; Flags: nowait postinstall skipifsilent
