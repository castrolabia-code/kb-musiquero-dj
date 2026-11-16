; =============================================================================
;  Instalador de KB - MUSIQUERO DJ · by Castro Labs (Inno Setup 6)
;
;  Lo compila el CI (GitHub Actions) después de "cmake --install ... --component app":
;    iscc /DAppVersion=0.12.0 /DDistDir="dist\KB Musiquero DJ" installer\KBMusiqueroDJ.iss
;
;  Firma de código (preparada): con un certificado OV/EV, definir en Inno Setup un
;  "Sign Tool" llamado "kbsign" (signtool sign /fd sha256 /tr <servidor TSA> /td sha256 /f cert.pfx /p ... $f)
;  y descomentar la línea SignTool de [Setup]. El CI puede hacerlo con secretos del repo.
; =============================================================================
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef DistDir
  #define DistDir "..\dist\KB Musiquero DJ"
#endif

[Setup]
AppId={{6E1C9B5A-4B7D-4E1F-9A2C-0B4D1A000001}
AppName=KB - MUSIQUERO DJ
AppVersion={#AppVersion}
AppVerName=KB - MUSIQUERO DJ {#AppVersion}
AppPublisher=Castro Labs
AppCopyright=© Castro Labs
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\KB Musiquero DJ
DefaultGroupName=KB Musiquero DJ
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=KB-Musiquero-DJ-Setup-{#AppVersion}
SetupIconFile=..\assets\icon\icon.ico
UninstallDisplayIcon={app}\KB Musiquero DJ.exe
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
; SignTool=kbsign
; SignedUninstaller=yes

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"; LicenseFile: "EULA_ES.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "EULA_EN.txt"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "EULA_ES.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion
Source: "EULA_EN.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\KB - MUSIQUERO DJ"; Filename: "{app}\KB Musiquero DJ.exe"
Name: "{group}\Manual de usuario"; Filename: "{app}\manual\manual_es.html"; Languages: spanish
Name: "{group}\User manual"; Filename: "{app}\manual\manual_en.html"; Languages: english
Name: "{group}\{cm:UninstallProgram,KB - MUSIQUERO DJ}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\KB - MUSIQUERO DJ"; Filename: "{app}\KB Musiquero DJ.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\KB Musiquero DJ.exe"; Description: "{cm:LaunchProgram,KB - MUSIQUERO DJ}"; Flags: nowait postinstall skipifsilent

; Al desinstalar NO se borran la biblioteca, los hot cues ni la licencia del usuario
; (%APPDATA%\Castro Labs\KB Musiquero DJ): reinstalar no pierde nada.
