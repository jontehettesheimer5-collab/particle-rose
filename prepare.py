"""Apply Terminal32 additions to the pinned iSH source checkout."""
from pathlib import Path
import plistlib
import shutil
import sys

source = Path(sys.argv[1])
overlay = Path(__file__).parent / 'overlay'
scene_path = source / 'app/SceneDelegate.m'
scene = scene_path.read_text()
scene = scene.replace('#import "AboutViewController.h"', '#import "AboutViewController.h"\n#import "T32FlowerIntro.h"\n#import "T32ChatController.h"')
scene = scene.replace('@property NSString *terminalUUID;', '@property NSString *terminalUUID;\n@property (nonatomic, strong) UIWindow *introWindow;\n@property (nonatomic, strong) TerminalViewController *terminalController;')
anchor = '    vc.sceneSession = session;'
assert scene.count(anchor) == 1
scene = scene.replace(anchor, '''    static BOOL introShown = NO;
    if (!introShown) {
        introShown = YES;
        self.introWindow = [[UIWindow alloc] initWithWindowScene:(UIWindowScene *)scene];
        self.introWindow.windowLevel = UIWindowLevelAlert + 1;
        UIViewController *introController = [UIViewController new];
        introController.view.backgroundColor = UIColor.blackColor;
        self.introWindow.rootViewController = introController;
        T32FlowerIntro *intro = [[T32FlowerIntro alloc] initWithFrame:self.introWindow.bounds];
        [introController.view addSubview:intro];
        __weak typeof(self) weakSelf = self;
        intro.completion = ^{
            weakSelf.introWindow.hidden = YES;
            weakSelf.introWindow = nil;
        };
        self.introWindow.hidden = NO;
        [intro start];
    }
''' + anchor)
# Retain the terminal controller explicitly because the window now hosts Chat/Terminal tabs.
scene = scene.replace('    vc.sceneSession = session;', '    self.terminalController = vc;\n    vc.sceneSession = session;')
scene = scene.replace('''    }
}

- (NSUserActivity *)stateRestorationActivityForScene:''', '''    }
    self.window.rootViewController = T32MakeWorkspace(vc);
}

- (NSUserActivity *)stateRestorationActivityForScene:''')
scene = scene.replace('TerminalViewController *vc = (TerminalViewController *) self.window.rootViewController;\n    if ([vc', 'TerminalViewController *vc = self.terminalController;\n    if ([vc')
scene = scene.replace('TerminalViewController *terminalViewController = (TerminalViewController *) self.window.rootViewController;', 'TerminalViewController *terminalViewController = self.terminalController;')
scene_path.write_text(scene)
for header in overlay.glob('T32*.h'):
    shutil.copy2(header, source / 'app' / header.name)

app_path = source / 'app/AppDelegate.m'
app = app_path.read_text().replace('#import "AppGroup.h"', '#import "AppGroup.h"\n#import "T32Mount.h"')
app = app.replace('    FsInitialize();', '    FsInitialize();\n    int importMountError = T32MountImports();\n    if (importMountError < 0) NSLog(@"Import mount failed: %d", importMountError);')
app_path.write_text(app)

terminal_header = source / 'app/Terminal.h'
terminal_header.write_text(terminal_header.read_text().replace('- (void)sendInput:(NSData *)input;', '- (void)sendInput:(NSData *)input;\n@property BOOL t32CaptureOutput;\n- (BOOL)t32ShellForeground:(int)pid;'))
terminal_path = source / 'app/Terminal.m'
terminal = terminal_path.read_text()
terminal = terminal.replace('- (int)sendOutput:(const void *)buf length:(int)len {', '''- (BOOL)t32ShellForeground:(int)pid {
#if !ISH_LINUX
    if (self.tty == NULL) return NO;
    lock(&self.tty->lock);
    BOOL idle = self.tty->fg_group == pid;
    unlock(&self.tty->lock);
    return idle;
#else
    return NO;
#endif
}

- (int)sendOutput:(const void *)buf length:(int)len {
    if (self.t32CaptureOutput && len > 0) {
        NSData *captured = [NSData dataWithBytes:buf length:len];
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter postNotificationName:@"T32TerminalOutput" object:self userInfo:@{@"data": captured}];
        });
    }''')
terminal_path.write_text(terminal)
controller_header = source / 'app/TerminalViewController.h'
controller_header.write_text(controller_header.read_text().replace('- (void)startNewSession;', '- (void)startNewSession;\n- (BOOL)t32CanRunCommand;\n- (void)t32RunCommand:(NSString *)command;'))
controller_path = source / 'app/TerminalViewController.m'
controller = controller_path.read_text()
controller = controller.replace('- (void)startNewSession {', '''- (BOOL)t32CanRunCommand {
    return self.terminal == self.sessionTerminal && self.sessionTerminal.loaded && [self.sessionTerminal t32ShellForeground:self.sessionPid];
}

- (void)t32RunCommand:(NSString *)command {
    if (![self t32CanRunCommand]) return;
    [self.sessionTerminal sendInput:[NSData dataWithBytes:"\\025" length:1]];
    [self.sessionTerminal sendInput:[command dataUsingEncoding:NSUTF8StringEncoding]];
}

- (void)startNewSession {''')
controller_path.write_text(controller)

group_path = source / 'app/AppGroup.m'
group = group_path.read_text()
old = '''    NSString *appGroup = CurrentAppGroups()[0];
    return [NSFileManager.defaultManager containerURLForSecurityApplicationGroupIdentifier:appGroup];'''
assert old in group
group = group.replace(old, '''    NSString *appGroup = CurrentAppGroups().firstObject;
    NSURL *shared = appGroup ? [NSFileManager.defaultManager containerURLForSecurityApplicationGroupIdentifier:appGroup] : nil;
    if (shared != nil) return shared;
    return [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;''')
group_path.write_text(group)

info_path = source / 'app/Info.plist'
info = plistlib.loads(info_path.read_bytes())
info['CFBundleDisplayName'] = 'Terminal32'
info['CFBundleShortVersionString'] = '2.0'
info['CFBundleVersion'] = '2'
info['LSSupportsOpeningDocumentsInPlace'] = True
info['UIRequiresFullScreen'] = True
info['UIApplicationSceneManifest']['UIApplicationSupportsMultipleScenes'] = False
info_path.write_bytes(plistlib.dumps(info))

# Favor the terminal's dark theme while preserving the user's settings controls.
prefs_path = source / 'app/UserPreferences.m'
prefs = prefs_path.read_text().replace('kPreferenceColorSchemeKey: @(ColorSchemeMatchSystem)', 'kPreferenceColorSchemeKey: @(ColorSchemeAlwaysDark)')
prefs_path.write_text(prefs)
print('Terminal32 source overlay applied.')
