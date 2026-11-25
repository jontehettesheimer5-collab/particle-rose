// Simulator-only regression checks for file import and chat tool execution.
#pragma once
#if TARGET_OS_SIMULATOR

@interface T32MockGroqProtocol : NSURLProtocol
@end

@implementation T32MockGroqProtocol
+ (BOOL)canInitWithRequest:(NSURLRequest *)request { return [request.URL.host isEqualToString:@"api.groq.com"]; }
+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request { return request; }
- (void)startLoading {
    NSData *body = self.request.HTTPBody;
    if (!body && self.request.HTTPBodyStream) {
        NSInputStream *stream = self.request.HTTPBodyStream;
        [stream open];
        NSMutableData *buffer = [NSMutableData data];
        uint8_t bytes[4096];
        NSInteger count;
        while ((count = [stream read:bytes maxLength:sizeof(bytes)]) > 0) [buffer appendBytes:bytes length:count];
        [stream close];
        body = buffer;
    }
    NSDictionary *request = [NSJSONSerialization JSONObjectWithData:body options:0 error:nil];
    NSArray *messages = request[@"messages"];
    NSDictionary *last = messages.lastObject;
    NSDictionary *message;
    if ([last[@"content"] isKindOfClass:NSArray.class]) {
        BOOL vision = [request[@"model"] isEqual:@"qwen/qwen3.8-27b"];
        BOOL image = NO;
        for (NSDictionary *part in last[@"content"]) if ([part[@"type"] isEqual:@"image_url"]) image = YES;
        message = @{@"role":@"assistant", @"content":vision && image ? @"Vision payload accepted" : @"Invalid image payload"};
    } else if ([last[@"role"] isEqual:@"tool"]) {
        message = @{ @"role": @"assistant", @"content": @"Tool completed." };
    } else {
        NSString *text = [last[@"content"] lowercaseString];
        NSString *tool = [text containsString:@"execute"] ? @"run_selected_file" : @"choose_file";
        message = @{ @"role": @"assistant", @"content": NSNull.null,
            @"tool_calls": @[@{ @"id": NSUUID.UUID.UUIDString, @"type": @"function",
                @"function": @{ @"name": tool, @"arguments": @"{}" } }] };
    }
    NSData *data = [NSJSONSerialization dataWithJSONObject:@{ @"choices": @[@{ @"message": message }] } options:0 error:nil];
    NSHTTPURLResponse *response = [[NSHTTPURLResponse alloc] initWithURL:self.request.URL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type": @"application/json"}];
    [self.client URLProtocol:self didReceiveResponse:response cacheStoragePolicy:NSURLCacheStorageNotAllowed];
    [self.client URLProtocol:self didLoadData:data];
    [self.client URLProtocolDidFinishLoading:self];
}
- (void)stopLoading {}
@end

static void T32SmokeWrite(NSDictionary *result) {
    NSData *data = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    [data writeToURL:[documents URLByAppendingPathComponent:@"smoke-result.json"] atomically:YES];
}

static BOOL T32HasResponder(UIView *view) {
    if (view.isFirstResponder) return YES;
    for (UIView *child in view.subviews) if (T32HasResponder(child)) return YES;
    return NO;
}

static void T32StartSmoke(T32ChatController *chat) {
    if (![NSProcessInfo.processInfo.arguments containsObject:@"--t32-smoke"]) return;
    [chat loadViewIfNeeded];
    NSURLSessionConfiguration *configuration = NSURLSessionConfiguration.ephemeralSessionConfiguration;
    configuration.protocolClasses = @[T32MockGroqProtocol.class];
    chat.client.session = [NSURLSession sessionWithConfiguration:configuration];
    __block NSUInteger ticks = 0;
    __block NSUInteger phase = 0;
    __block BOOL earlierChecks = NO;
    __block NSArray *scriptMessages = @[];
    [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer *timer) {
        ticks++;
        if (ticks > 160) {
            [timer invalidate];
            NSMutableArray *texts = [NSMutableArray array];
            for (NSDictionary *row in chat.rows) [texts addObject:row[@"text"]];
            T32SmokeWrite(@{@"passed": @NO, @"phase": @(phase), @"messages": texts});
            return;
        }
        if (phase == 0 && chat.view.window) {
            if (T32HasResponder(chat.view.window) || T32HasResponder(chat.terminalController.view)) {
                T32SmokeWrite(@{@"passed":@NO, @"error":@"Keyboard focused on launch"}); [timer invalidate]; return;
            }
            chat.input.text = @"Please locate my script";
            [chat send];
            phase = 1;
        } else if (phase == 1 && [chat.presentedViewController isKindOfClass:UIDocumentPickerViewController.class]) {
            UIDocumentPickerViewController *picker = (UIDocumentPickerViewController *)chat.presentedViewController;
            NSURL *fixture = [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:@"hello ' flower (1).sh"];
            [@"#!/bin/sh\nprintf 'T32_IMPORT_OK\\n'\n" writeToURL:fixture atomically:YES encoding:NSUTF8StringEncoding error:nil];
            [picker dismissViewControllerAnimated:NO completion:^{ [chat documentPicker:picker didPickDocumentsAtURLs:@[fixture]]; }];
            phase = 2;
        } else if (phase == 2 && chat.store.selected && !chat.busy) {
            chat.input.text = @"ok execute the file";
            [chat send];
            phase = 3;
        } else if (phase == 3 && !chat.busy && !chat.running) {
            BOOL passed = NO;
            NSMutableArray *texts = [NSMutableArray array];
            for (NSDictionary *row in chat.rows) {
                [texts addObject:row[@"text"]];
                if ([row[@"text"] containsString:@"Exit code 0"] && [row[@"text"] containsString:@"T32_IMPORT_OK"]) passed = YES;
            }
            NSMutableDictionary *original = chat.activeThread;
            scriptMessages = [texts copy];
            NSUInteger oldCount = chat.rows.count;
            [chat newChat];
            BOOL threadsWork = chat.rows.count == 0 && chat.history.count == 0 && chat.threads.count == 2;
            [chat activateThread:original];
            threadsWork = threadsWork && chat.rows.count == oldCount;
            passed = passed && threadsWork;
            earlierChecks = passed;
            [chat newChat];
            UIGraphicsBeginImageContextWithOptions(CGSizeMake(32, 32), YES, 1);
            [UIColor.systemPinkColor setFill]; UIRectFill(CGRectMake(0, 0, 32, 32));
            UIImage *image = UIGraphicsGetImageFromCurrentImageContext(); UIGraphicsEndImageContext();
            NSURL *fixture = [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:@"flower-test.png"];
            [UIImagePNGRepresentation(image) writeToURL:fixture atomically:YES];
            [chat documentPicker:nil didPickDocumentsAtURLs:@[fixture]];
            phase = 4;
        } else if (phase == 4 && !chat.busy && chat.pendingImages.count) {
            chat.input.text = @"Describe the attached image"; [chat send]; phase = 5;
        } else if (phase == 5 && !chat.busy) {
            BOOL imagePassed = [chat.rows.lastObject[@"text"] isEqual:@"Vision payload accepted"];
            BOOL imageStored = NO;
            for (NSDictionary *row in chat.rows) if ([row[@"images"] count]) imageStored = YES;
            [timer invalidate];
            T32SmokeWrite(@{@"passed":@(earlierChecks && imagePassed && imageStored), @"phase":@(phase), @"messages":chat.rows, @"scriptMessages":scriptMessages,
                @"checks":@[@"Keyboard unfocused at startup", @"Groq tools (mocked)", @"Native file picker", @"File import and quoted script execution", @"Output and exit code", @"Separate chat histories", @"Image import, message and vision payload (mocked)"],
                @"deviceChecksRemaining":@[@"Keychain with KSign provisioning", @"Physical Photos and iCloud providers"]});
        }
    }];
}
#endif
