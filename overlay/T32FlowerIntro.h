// Terminal32 additions by 32e4, distributed under GPL-3.0-or-later.
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include <math.h>

typedef struct {
    CGFloat radius, angle, width, height, across, along, light;
    CGPoint text;
} T32Particle;

@interface T32FlowerIntro : UIView
@property (nonatomic, copy) void (^completion)(void);
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) NSMutableData *particleData;
@property (nonatomic) CFTimeInterval started;
@property (nonatomic) CFTimeInterval elapsed;
@property (nonatomic) CGSize preparedSize;
- (void)start;
@end

static CGFloat T32Ease(CGFloat start, CGFloat end, CGFloat value) {
    CGFloat t = MIN(1, MAX(0, (value - start) / (end - start)));
    return t * t * (3 - 2 * t);
}

@implementation T32FlowerIntro

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.backgroundColor = UIColor.blackColor;
        self.opaque = YES;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        self.isAccessibilityElement = YES;
        self.accessibilityLabel = @"Flower animation, made by 32e4";
    }
    return self;
}

- (void)prepareParticles {
    CGSize size = self.bounds.size;
    if (size.width <= 0 || size.height <= 0) return;
    self.preparedSize = size;
    CGFloat fontSize = MIN(42, size.width / 8.5);
    NSDictionary *attributes = @{
        NSFontAttributeName: [UIFont monospacedSystemFontOfSize:fontSize weight:UIFontWeightMedium],
        NSForegroundColorAttributeName: UIColor.whiteColor
    };
    NSString *credit = @"made by 32e4";
    CGSize textSize = [credit sizeWithAttributes:attributes];
    CGSize bitmapSize = CGSizeMake(ceil(textSize.width) + 8, ceil(textSize.height) + 8);
    UIGraphicsBeginImageContextWithOptions(bitmapSize, NO, 1);
    [credit drawAtPoint:CGPointMake(4, 4) withAttributes:attributes];
    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    CGImageRef cgImage = image.CGImage;
    size_t w = CGImageGetWidth(cgImage), h = CGImageGetHeight(cgImage);
    NSMutableData *pixels = [NSMutableData dataWithLength:w * h * 4];
    CGColorSpaceRef colors = CGColorSpaceCreateDeviceRGB();
    CGContextRef bitmap = CGBitmapContextCreate(pixels.mutableBytes, w, h, 8, w * 4,
                                               colors, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(colors);
    if (!bitmap) return;
    CGContextDrawImage(bitmap, CGRectMake(0, 0, w, h), cgImage);
    CGContextRelease(bitmap);
    const unsigned char *bytes = pixels.bytes;
    NSMutableArray<NSValue *> *targets = [NSMutableArray array];
    for (size_t y = 0; y < h; y += 2) {
        for (size_t x = 0; x < w; x += 2) {
            if (bytes[(y * w + x) * 4 + 3] > 120) {
                CGPoint point = CGPointMake(x + (size.width - w) / 2,
                                           y + (size.height - h) / 2);
                [targets addObject:[NSValue valueWithCGPoint:point]];
            }
        }
    }
    if (targets.count == 0) return;
    const NSUInteger count = 4600;
    self.particleData = [NSMutableData dataWithLength:sizeof(T32Particle) * count];
    T32Particle *particles = self.particleData.mutableBytes;
    uint32_t seed = 324;
    for (NSUInteger i = 0; i < count; i++) {
        seed = seed * 1664525u + 1013904223u;
        CGFloat u = (seed & 65535) / 65535.0;
        seed = seed * 1664525u + 1013904223u;
        CGFloat v = (seed & 65535) / 32767.5 - 1;
        NSUInteger layer = i % 4;
        NSUInteger petal = (i / 4) % 7;
        particles[i] = (T32Particle){
            .radius = 0.12 + u * (0.4 + layer * 0.23),
            .angle = petal * 2 * M_PI / 7 + layer * 2.39996,
            .width = (0.2 + layer * 0.14) * pow(sin(M_PI * u), 0.65),
            .height = 0.6 - layer * 0.17 + sin(M_PI * u) * 0.35 - u * u * 0.45 + v * v * 0.17,
            .across = v, .along = u, .light = (seed & 255) / 255.0,
            .text = targets[(i * 37) % targets.count].CGPointValue
        };
    }
}

- (void)start {
    if (UIAccessibilityIsReduceMotionEnabled()) {
        self.elapsed = 5.5;
        [self prepareParticles];
        [self setNeedsDisplay];
        __weak typeof(self) weakSelf = self;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
            [weakSelf finish];
        });
        return;
    }
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

- (void)tick:(CADisplayLink *)link {
    if (self.started == 0) self.started = link.timestamp;
    self.elapsed = link.timestamp - self.started;
    if (self.elapsed >= 6.8) {
        [self finish];
        return;
    }
    [self setNeedsDisplay];
}

- (void)finish {
    [self.displayLink invalidate];
    self.displayLink = nil;
    void (^completion)(void) = self.completion;
    self.completion = nil;
    [self removeFromSuperview];
    if (completion) completion();
}

- (void)drawRect:(CGRect)rect {
    if (!CGSizeEqualToSize(self.preparedSize, self.bounds.size)) [self prepareParticles];
    CGContextRef context = UIGraphicsGetCurrentContext();
    CGContextSetFillColorWithColor(context, UIColor.blackColor.CGColor);
    CGContextFillRect(context, self.bounds);
    CGFloat t = self.elapsed;
    CGFloat growth = 0.01 + T32Ease(0, 2.3, t) * 0.99;
    CGFloat morph = T32Ease(3.1, 5.0, t);
    CGFloat fade = 1 - T32Ease(6.1, 6.8, t);
    CGFloat scale = MIN(self.bounds.size.width * 0.35, self.bounds.size.height * 0.26);
    CGFloat turn = t * 0.22;
    const T32Particle *particles = self.particleData.bytes;
    NSUInteger count = self.particleData.length / sizeof(T32Particle);
    for (NSUInteger i = 0; i < count; i++) {
        T32Particle p = particles[i];
        CGFloat angle = p.angle + turn;
        CGFloat x = cos(angle) * p.radius - sin(angle) * p.across * p.width;
        CGFloat y = sin(angle) * p.radius + cos(angle) * p.across * p.width;
        CGFloat flowerX = self.bounds.size.width / 2 + x * scale * growth;
        CGFloat flowerY = self.bounds.size.height / 2 - (y * 0.78 - p.height * 0.62) * scale * growth;
        CGFloat px = flowerX + (p.text.x - flowerX) * morph;
        CGFloat py = flowerY + (p.text.y - flowerY) * morph;
        CGFloat light = 0.45 + p.light * 0.55;
        CGFloat red = 0.9 + morph * 0.1;
        CGFloat green = 0.15 + light * 0.4 + morph * (0.85 - light * 0.4);
        CGFloat blue = 0.45 + light * 0.35 + morph * (0.55 - light * 0.35);
        CGContextSetRGBFillColor(context, red, green, blue, fade * (0.65 + light * 0.35));
        CGFloat dot = 0.8 + p.light * 0.8;
        CGContextFillEllipseInRect(context, CGRectMake(px, py, dot, dot));
    }
}

@end
