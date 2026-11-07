; Potato EQ - Windows installer (Inno Setup 6)
; Built by GitHub Actions:  ISCC /DAppVer=x.y.z installer\windows\PotatoEQ.iss
; Expects the built plugin in dist\ (see .github/workflows/build.yml).

#ifndef AppVer
  #define AppVer "0.0.0"
#endif

[Setup]
AppId={{6F3C2A8E-4B7D-4E1A-9C55-2D8B7E1F0A93}
AppName=Potato EQ
AppVersion={#AppVer}
AppVerName=Potato EQ {#AppVer}
AppPublisher=Taroy Audio
DefaultDirName={autopf}\Taroy Audio\Potato EQ
DefaultGroupName=Potato EQ
DisableProgramGroupPage=yes
OutputDir=..\..\installers
OutputBaseFilename=PotatoEQ-{#AppVer}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Potato EQ
UninstallDisplayIcon={app}\Potato EQ.exe
CloseApplications=yes

[Types]
Name: "full"; Description: "VST3 plugin + standalone app"
Name: "plugin"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin  (FL Studio, Ableton, Reaper, Studio One, Cubase, Bitwig...)"; Types: full plugin custom; Flags: fixed
Name: "standalone"; Description: "Standalone app (use Potato EQ without a DAW)"; Types: full

[Files]
Source: "..\..\dist\Potato EQ.vst3\*"; DestDir: "{commoncf64}\VST3\Potato EQ.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\dist\Potato EQ.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\..\INSTALL.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Potato EQ"; Filename: "{app}\Potato EQ.exe"; Components: standalone
Name: "{group}\Uninstall Potato EQ"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Potato EQ.vst3"

[Messages]
FinishedLabel=Potato EQ is installed.%n%nThe VST3 plugin is in C:\Program Files\Common Files\VST3. In FL Studio, open Options > Manage plugins and click "Find installed plugins" (other DAWs rescan on start-up).
