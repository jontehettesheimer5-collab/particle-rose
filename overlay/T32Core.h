// Terminal32 additions by 32e4, GPL-3.0-or-later.
#pragma once
#import <UIKit/UIKit.h>
#import <Security/Security.h>
#import <ImageIO/ImageIO.h>
#import "TerminalViewController.h"
#import "T32Mount.h"

static NSString *T32Quote(NSString *text) {
    return [NSString stringWithFormat:@"'%@'", [text stringByReplacingOccurrencesOfString:@"'" withString:@"'\\''"]];
}

static NSURL *T32StateURL(void) {
    NSURL *support = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject;
    NSURL *directory = [support URLByAppendingPathComponent:@"Terminal32" isDirectory:YES];
    [NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:nil];
    return directory;
}

static UIImage *T32ImageForRecord(NSDictionary *record) {
    if (![record[@"relative"] isKindOfClass:NSString.class]) return nil;
    NSURL *url = [T32ImportsURL() URLByAppendingPathComponent:record[@"relative"]];
    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!source) return nil;
    NSDictionary *options = @{(__bridge id)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
                              (__bridge id)kCGImageSourceThumbnailMaxPixelSize: @1600,
                              (__bridge id)kCGImageSourceCreateThumbnailWithTransform: @YES};
    CGImageRef thumbnail = CGImageSourceCreateThumbnailAtIndex(source, 0, (__bridge CFDictionaryRef)options);
    CFRelease(source);
    if (!thumbnail) return nil;
    UIImage *image = [UIImage imageWithCGImage:thumbnail];
    CGImageRelease(thumbnail);
    return image;
}

static id T32MessageContent(NSDictionary *row) {
    NSMutableArray *parts = [NSMutableArray arrayWithObject:@{@"type": @"text", @"text": row[@"text"] ?: @""}];
    for (NSDictionary *record in row[@"images"]) {
        UIImage *image = T32ImageForRecord(record);
        if (!image) continue;
        NSData *jpeg = UIImageJPEGRepresentation(image, 0.75);
        if (!jpeg) continue;
        NSString *url = [@"data:image/jpeg;base64," stringByAppendingString:[jpeg base64EncodedStringWithOptions:0]];
        [parts addObject:@{@"type": @"image_url", @"image_url": @{@"url": url}}];
    }
    return parts.count > 1 ? parts : (row[@"text"] ?: @"");
}

static NSMutableDictionary *T32KeyQuery(void) {
    return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
              (__bridge id)kSecAttrService: NSBundle.mainBundle.bundleIdentifier,
              (__bridge id)kSecAttrAccount: @"groq-api-key"} mutableCopy];
}

static NSString *T32ReadKey(void) {
#if TARGET_OS_SIMULATOR
    // Unsigned CI simulator apps cannot access Keychain. Mock this boundary only
    // for the explicit smoke-test launch; device builds always use Keychain.
    if ([NSProcessInfo.processInfo.arguments containsObject:@"--t32-smoke"]) return @"simulator-test-key";
#endif
    NSMutableDictionary *query = T32KeyQuery();
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;
    CFTypeRef result = NULL;
    if (SecItemCopyMatching((__bridge CFDictionaryRef)query, &result) != errSecSuccess) return @"";
    NSData *data = CFBridgingRelease(result);
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
}

static OSStatus T32SaveKey(NSString *key) {
    NSMutableDictionary *query = T32KeyQuery();
    NSData *data = [key dataUsingEncoding:NSUTF8StringEncoding];
    if (key.length == 0) {
        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
        return status == errSecItemNotFound ? errSecSuccess : status;
    }
    NSDictionary *update = @{(__bridge id)kSecValueData: data};
    OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);
    if (status != errSecItemNotFound) return status;
    query[(__bridge id)kSecValueData] = data;
    query[(__bridge id)kSecAttrAccessible] = (__bridge id)kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
    return SecItemAdd((__bridge CFDictionaryRef)query, NULL);
}

@interface T32FileStore : NSObject
@property (nonatomic, strong) NSMutableArray<NSDictionary *> *files;
@property (nonatomic, strong) NSDictionary *selected;
- (void)save;
- (NSDictionary *)importURL:(NSURL *)url error:(NSError **)error;
- (NSString *)selectedText:(NSError **)error;
@end

@implementation T32FileStore
- (instancetype)init {
    if ((self = [super init])) {
        NSData *data = [NSData dataWithContentsOfURL:[T32StateURL() URLByAppendingPathComponent:@"files.json"]];
        NSArray *saved = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        self.files = [NSMutableArray array];
        for (id item in saved) {
            if (![item isKindOfClass:NSDictionary.class]) continue;
            NSString *relative = item[@"relative"];
            if (![relative isKindOfClass:NSString.class] || [relative.pathComponents containsObject:@".."] || relative.isAbsolutePath) continue;
            if ([NSFileManager.defaultManager fileExistsAtPath:[T32ImportsURL() URLByAppendingPathComponent:relative].path]) [self.files addObject:item];
        }
        self.selected = self.files.lastObject;
    }
    return self;
}
- (void)save {
    NSData *data = [NSJSONSerialization dataWithJSONObject:self.files options:0 error:nil];
    [data writeToURL:[T32StateURL() URLByAppendingPathComponent:@"files.json"] options:NSDataWritingAtomic error:nil];
}
- (NSDictionary *)importURL:(NSURL *)url error:(NSError **)error {
    __block NSDictionary *record;
    __block NSError *failure;
    BOOL scoped = [url startAccessingSecurityScopedResource];
    NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
    [coordinator coordinateReadingItemAtURL:url options:0 error:&failure byAccessor:^(NSURL *readURL) {
        NSNumber *directory, *size;
        [readURL getResourceValue:&directory forKey:NSURLIsDirectoryKey error:nil];
        [readURL getResourceValue:&size forKey:NSURLFileSizeKey error:nil];
        if (directory.boolValue || size.unsignedLongLongValue > 20 * 1024 * 1024) {
            failure = [NSError errorWithDomain:@"Terminal32" code:1 userInfo:@{NSLocalizedDescriptionKey:@"Choose a file up to 20 MB."}];
            return;
        }
        NSString *name = readURL.lastPathComponent;
        if ([name rangeOfCharacterFromSet:NSCharacterSet.controlCharacterSet].location != NSNotFound) {
            failure = [NSError errorWithDomain:@"Terminal32" code:2 userInfo:@{NSLocalizedDescriptionKey:@"This filename contains unsupported control characters."}];
            return;
        }
        NSString *identifier = NSUUID.UUID.UUIDString;
        NSURL *folder = [T32ImportsURL() URLByAppendingPathComponent:identifier isDirectory:YES];
        if (![NSFileManager.defaultManager createDirectoryAtURL:folder withIntermediateDirectories:YES attributes:nil error:&failure]) return;
        NSURL *destination = [folder URLByAppendingPathComponent:name];
        if (![NSFileManager.defaultManager copyItemAtURL:readURL toURL:destination error:&failure]) return;
        NSString *relative = [identifier stringByAppendingPathComponent:name];
        record = @{ @"name": name, @"relative": relative, @"path": [@"/imports" stringByAppendingPathComponent:relative], @"size": size ?: @0 };
    }];
    if (scoped) [url stopAccessingSecurityScopedResource];
    if (failure && error) *error = failure;
    return record;
}
- (NSString *)selectedText:(NSError **)error {
    if (!self.selected) return @"No file selected.";
    NSURL *url = [T32ImportsURL() URLByAppendingPathComponent:self.selected[@"relative"]];
    NSFileHandle *handle = [NSFileHandle fileHandleForReadingFromURL:url error:error];
    if (!handle) return nil;
    NSData *data = [handle readDataOfLength:32768];
    [handle closeFile];
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!text) return @"The selected file is binary or is not UTF-8 text.";
    return data.length == 32768 ? [text stringByAppendingString:@"\n[Preview limited to 32 KB]"] : text;
}
@end

@interface T32GroqClient : NSObject
@property (nonatomic) BOOL toolsEnabled;
@property (nonatomic) BOOL scriptToolEnabled;
@property (nonatomic, strong) NSURLSessionDataTask *task;
@property (nonatomic, strong) NSURLSession *session;
- (void)replyTo:(NSArray *)messages completion:(void (^)(NSDictionary *, NSString *))completion;
@end

@implementation T32GroqClient
- (void)replyTo:(NSArray *)messages completion:(void (^)(NSDictionary *, NSString *))completion {
    NSString *key = T32ReadKey();
    if (!key.length) { completion(nil, @"Add your Groq API key in Settings first."); return; }
    NSDictionary *(^tool)(NSString *, NSString *, NSDictionary *) = ^NSDictionary *(NSString *name, NSString *description, NSDictionary *properties) {
        return @{ @"type": @"function", @"function": @{ @"name": name, @"description": description,
            @"parameters": @{ @"type": @"object", @"properties": properties, @"required": @[], @"additionalProperties": @NO } } };
    };
    NSArray *tools = @[
        tool(@"choose_file", @"Open the native iPhone/iPad file picker so the user can select a file. Wait for selection.", @{}),
        tool(@"list_files", @"List files the user has imported into this app.", @{}),
        tool(@"read_selected_file", @"Read up to 32 KB of the selected file when the user asks to inspect or explain its contents.", @{}),
        tool(@"run_selected_file", @"Execute the selected script locally only when the user's current message explicitly requests execution. The result contains actual output and an exit code.", @{ @"arguments": @{ @"type": @"array", @"items": @{ @"type": @"string" }, @"description": @"Optional script arguments." } })
    ];
    // Groq's vision model accepts three images across the entire request.
    NSMutableArray *prepared = [NSMutableArray array];
    NSUInteger remainingImages = 3;
    BOOL hasImages = NO;
    for (NSDictionary *message in messages.reverseObjectEnumerator) {
        NSMutableDictionary *copy = [message mutableCopy];
        if ([message[@"content"] isKindOfClass:NSArray.class]) {
            NSMutableArray *parts = [NSMutableArray array];
            for (NSDictionary *part in [message[@"content"] reverseObjectEnumerator]) {
                if ([part[@"type"] isEqual:@"image_url"]) {
                    if (!remainingImages) continue;
                    remainingImages--;
                    hasImages = YES;
                }
                [parts insertObject:part atIndex:0];
            }
            copy[@"content"] = parts;
        }
        [prepared insertObject:copy atIndex:0];
    }
    NSString *model = hasImages ? @"qwen/qwen3.8-27b" : ([NSUserDefaults.standardUserDefaults stringForKey:@"T32GroqModel"] ?: @"openai/gpt-oss-120b");
    NSMutableDictionary *body = [@{ @"model": model, @"messages": prepared,
                           @"max_completion_tokens": @2048, @"temperature": @0.3 } mutableCopy];
    if (self.toolsEnabled) {
        body[@"tools"] = self.scriptToolEnabled ? tools : [tools subarrayWithRange:NSMakeRange(0, tools.count - 1)];
        body[@"parallel_tool_calls"] = @NO;
    }
    if ([model hasPrefix:@"qwen/"]) body[@"reasoning_effort"] = @"none";
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://api.groq.com/openai/v1/chat/completions"]];
    request.HTTPMethod = @"POST";
    request.timeoutInterval = 60;
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [request setValue:[@"Bearer " stringByAppendingString:key] forHTTPHeaderField:@"Authorization"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    NSURLSession *session = self.session ?: NSURLSession.sharedSession;
    self.task = [session dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        NSDictionary *json = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        NSInteger status = [(NSHTTPURLResponse *)response statusCode];
        NSString *failure;
        NSDictionary *message;
        if (error) failure = error.localizedDescription;
        else if (status < 200 || status >= 300) {
            NSString *detail = [json[@"error"] isKindOfClass:NSDictionary.class] ? json[@"error"][@"message"] : nil;
            failure = [NSString stringWithFormat:@"Groq returned HTTP %ld. %@", (long)status, detail ?: @"Check your key, model and account limits."];
        } else {
            NSArray *choices = json[@"choices"];
            if ([choices isKindOfClass:NSArray.class] && choices.count) message = choices.firstObject[@"message"];
            if (![message isKindOfClass:NSDictionary.class]) failure = @"Groq returned an unreadable response. Try again.";
        }
        dispatch_async(dispatch_get_main_queue(), ^{ completion(message, failure); });
    }];
    [self.task resume];
}
@end
