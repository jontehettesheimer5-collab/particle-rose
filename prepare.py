"""Apply Terminal32 additions to the pinned iSH source checkout."""
from pathlib import Path
import plistlib
import shutil
import sys

source = Path(sys.argv[1])
overlay = Path(__file__).parent / 'overlay'
scene_path = source / 'app/SceneDelegate.m'
scene = scene_path.read_text()
scene = scene.replace('#import "AboutViewController.h"', '#import "AboutViewController.h"\n#import "T32FlowerIntro.h"')
scene = scene.replace('@property NSString *terminalUUID;', '@property NSString *terminalUUID;\n@property (nonatomic, strong) UIWindow *introWindow;')
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
scene_path.write_text(scene)
shutil.copy2(overlay / 'T32FlowerIntro.h', source / 'app/T32FlowerIntro.h')

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
info['UIRequiresFullScreen'] = True
info['UIApplicationSceneManifest']['UIApplicationSupportsMultipleScenes'] = False
info_path.write_bytes(plistlib.dumps(info))

# Favor the terminal's dark theme while preserving the user's settings controls.
prefs_path = source / 'app/UserPreferences.m'
prefs = prefs_path.read_text().replace('kPreferenceColorSchemeKey: @(ColorSchemeMatchSystem)', 'kPreferenceColorSchemeKey: @(ColorSchemeAlwaysDark)')
prefs_path.write_text(prefs)
print('Terminal32 source overlay applied.')
