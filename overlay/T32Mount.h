// Terminal32 additions by 32e4, GPL-3.0-or-later.
#pragma once
#import <Foundation/Foundation.h>
#include "kernel/task.h"
#include "kernel/fs.h"
#include "fs/path.h"
#include "fs/real.h"

static NSURL *T32ImportsURL(void) {
    NSURL *documents = [NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    return [documents URLByAppendingPathComponent:@"Terminal32Imports" isDirectory:YES];
}

static int T32MountImports(void) {
    NSURL *url = T32ImportsURL();
    if (![NSFileManager.defaultManager createDirectoryAtURL:url withIntermediateDirectories:YES attributes:nil error:nil]) return -1;
    current = pid_get_task(1);
    if (!current) return -1;
    int result = generic_mkdirat(AT_PWD, "/imports", 0755);
    if (result < 0 && result != -17) return result;
    return do_mount(&realfs, url.fileSystemRepresentation, "/imports", "", 0);
}
