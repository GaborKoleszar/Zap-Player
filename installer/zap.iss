; Inno Setup script for Zap. Build with:  pwsh scripts/build-installer.ps1
;
; Installs for the current user only (no administrator prompt), adds a Start menu entry and
; an uninstaller, and always registers Zap as a player for common video types, so Windows
; offers it under "Open with" and in Settings > Default apps.
; Windows does not let installers change the default app; the user picks Zap once in
; "Open with > Choose another app > Always".

#define AppName "Zap"
#define AppExe "Zap.exe"
#define DistDir "..\build\release\dist"
; The version comes from the built exe, which gets it from CMakeLists.txt.
#define AppVersion GetStringFileInfo(DistDir + "\" + AppExe, "ProductVersion")
#define ProgId "Zap.Video"

[Setup]
AppId={{6F1B0E0C-5B1E-4D0B-9C7E-2A5D2E3F7A41}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Zap
VersionInfoVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\{#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
WizardStyle=modern
; Zap and the bundled libmpv are GPL software: the licence is shown and installed with them.
LicenseFile=..\LICENSE
SetupIconFile=..\win\zap.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
OutputDir=..\build\installer
OutputBaseFilename=ZapSetup-{#AppVersion}
CloseApplications=yes
RestartApplications=no
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#DistDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#DistDir}\libmpv-2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; The file type Zap handles, and how to open it.
Root: HKA; Subkey: "Software\Classes\{#ProgId}"; ValueType: string; ValueData: "Video file"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\{#ProgId}\DefaultIcon"; ValueType: string; ValueData: "{app}\{#AppExe},0"
Root: HKA; Subkey: "Software\Classes\{#ProgId}\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""

; Lets Explorer list Zap under "Open with" even for types not named below.
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\shell\open\command"; ValueType: string; ValueData: """{app}\{#AppExe}"" ""%1"""

; Makes Zap show up in Settings > Apps > Default apps.
Root: HKA; Subkey: "Software\Zap\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Zap\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "A small, fast video player."
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "{#AppName}"; ValueData: "Software\Zap\Capabilities"; Flags: uninsdeletevalue

; For every video extension: offer Zap for it, without taking over the default.
#dim Exts[16] {".mp4", ".mkv", ".webm", ".mov", ".avi", ".m4v", ".wmv", ".flv", ".mpg", ".mpeg", ".ts", ".m2ts", ".mts", ".3gp", ".ogv", ".vob"}
#define i 0
#sub EmitExt
  #define E Exts[i]
Root: HKA; Subkey: "Software\Classes\{#E}\OpenWithProgids"; ValueType: string; ValueName: "{#ProgId}"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\SupportedTypes"; ValueType: string; ValueName: "{#E}"; ValueData: ""
Root: HKA; Subkey: "Software\Zap\Capabilities\FileAssociations"; ValueType: string; ValueName: "{#E}"; ValueData: "{#ProgId}"
#endsub
#for {i = 0; i < DimOf(Exts); i++} EmitExt

[Run]
Filename: "{app}\{#AppExe}"; Description: "Start Zap"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Compiled-shader cache. Settings and resume positions in %APPDATA%\Zap are left in place.
Type: filesandordirs; Name: "{localappdata}\Zap\cache"
Type: dirifempty; Name: "{localappdata}\Zap"
