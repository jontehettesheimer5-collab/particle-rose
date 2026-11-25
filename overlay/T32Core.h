// Terminal32 additions by 32e4, GPL-3.0-or-later.
#pragma once
#import <UIKit/UIKit.h>
#import <Security/Security.h>
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

static NSMutableDictionary *T32KeyQuery(void) {
    return [@{(__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
              (__bridge id)kSecAttrService: NSBundle.mainBundle.bundleIdentifier,
              (__bridge id)kSecAttrAccount: @"groq-api-key"} mutableCopy];
}

static NSString *T32ReadKey(void) {
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
            if (![relative isKindOfClass:NSString.class] || [relative containsString:@".."] || relative.isAbsolutePath) continue;
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
@property (nonatomic, strong) NSURLSessionDataTask *task;
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
    NSString *model = [NSUserDefaults.standardUserDefaults stringForKey:@"T32GroqModel"] ?: @"openai/gpt-oss-120b";
    NSDictionary *body = @{ @"model": model, @"messages": messages, @"tools": tools, @"parallel_tool_calls": @NO,
                           @"max_completion_tokens": @2048, @"temperature": @0.3 };
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:@"https://api.groq.com/openai/v1/chat/completions"]];
    request.HTTPMethod = @"POST";
    request.timeoutInterval = 60;
    [request setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
    [request setValue:[@"Bearer " stringByAppendingString:key] forHTTPHeaderField:@"Authorization"];
    request.HTTPBody = [NSJSONSerialization dataWithJSONObject:body options:0 error:nil];
    self.task = [NSURLSession.sharedSession dataTaskWithRequest:request completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
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
