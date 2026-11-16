; Installer di AudioNaut (ex JLoop) per Windows (Inno Setup 6). Compilato da GitHub Actions:
;   ISCC.exe /DMyAppVersion=0.59.0 /DSourceExe=...\AudioNaut.exe ci\windows\audionaut.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceExe
  #define SourceExe "..\..\Builds\VisualStudio2022\x64\Release\App\AudioNaut.exe"
#endif

[Setup]
AppId={{6E3B1C2A-4F2D-4C7B-9C1E-7A5D2B8F0A11}
AppName=AudioNaut
AppVersion={#MyAppVersion}
AppVerName=AudioNaut {#MyAppVersion}
AppPublisher=joEsb
; chi aveva gia' JLoop: si aggiorna nella stessa cartella (UsePreviousAppDir), con lo stesso AppId
DefaultDirName={autopf}\AudioNaut
DefaultGroupName=AudioNaut
DisableProgramGroupPage=yes
; Si installa anche senza permessi da amministratore (solo per l'utente), oppure per tutti se si sceglie
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\dist
OutputBaseFilename=AudioNaut-Setup
SetupIconFile=audionaut.ico
UninstallDisplayIcon={app}\AudioNaut.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes

[Languages]
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"

[Tasks]
Name: "desktopicon"; Description: "Crea un'icona sul desktop"; GroupDescription: "Icone:"
Name: "assoc"; Description: "Apri i progetti .jloop con AudioNaut (doppio clic)"; GroupDescription: "File:"

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion

[InstallDelete]
; resti del vecchio nome (JLoop)
Type: files; Name: "{app}\JLoop.exe"
Type: files; Name: "{autoprograms}\JLoop.lnk"
Type: files; Name: "{autodesktop}\JLoop.lnk"

[Icons]
Name: "{autoprograms}\AudioNaut"; Filename: "{app}\AudioNaut.exe"
Name: "{autodesktop}\AudioNaut"; Filename: "{app}\AudioNaut.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.jloop"; ValueType: string; ValueName: ""; ValueData: "JLoop.Progetto"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\.dawfl"; ValueType: string; ValueName: ""; ValueData: "JLoop.Progetto"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto"; ValueType: string; ValueName: ""; ValueData: "Progetto AudioNaut"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\AudioNaut.exe,0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\JLoop.Progetto\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\AudioNaut.exe"" ""%1"""; Tasks: assoc

[Run]
Filename: "{app}\AudioNaut.exe"; Description: "Avvia AudioNaut"; Flags: nowait postinstall skipifsilent
