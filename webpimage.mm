#include "webpimage.h"

#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

QImage loadWebpImage(const QString &filePath)
{
    @autoreleasepool {
        NSURL *url = [NSURL fileURLWithPath:filePath.toNSString()];
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, nullptr);
        if (!source)
            return {};
        CGImageRef cgImage = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
        CFRelease(source);
        if (!cgImage)
            return {};

        const size_t width = CGImageGetWidth(cgImage);
        const size_t height = CGImageGetHeight(cgImage);
        if (width == 0 || height == 0) {
            CGImageRelease(cgImage);
            return {};
        }

        const CGImageAlphaInfo alpha = CGImageGetAlphaInfo(cgImage);
        const bool hasAlpha = !(alpha == kCGImageAlphaNone
                                || alpha == kCGImageAlphaNoneSkipFirst
                                || alpha == kCGImageAlphaNoneSkipLast);
        // CG draws into the buffer as R,G,B,A (premultiplied-last) — match that
        // byte order with RGBA formats; Qt converts/un-premultiplies afterwards.
        const QImage::Format fmt = hasAlpha ? QImage::Format_RGBA8888_Premultiplied
                                            : QImage::Format_RGBX8888;
        QImage image(int(width), int(height), fmt);
        image.fill(hasAlpha ? Qt::transparent : Qt::black);

        CGColorSpaceRef colorSpace = CGImageGetColorSpace(cgImage);
        CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
        CGContextRef ctx = CGBitmapContextCreate(
            image.bits(), width, height, 8, image.bytesPerLine(),
            colorSpace ? colorSpace : rgb,
            hasAlpha ? kCGImageAlphaPremultipliedLast : kCGImageAlphaNoneSkipLast);
        CGColorSpaceRelease(rgb);
        if (!ctx) {
            CGImageRelease(cgImage);
            return {};
        }
        CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), cgImage);
        CGContextRelease(ctx);
        CGImageRelease(cgImage);
        image = image.convertToFormat(hasAlpha ? QImage::Format_ARGB32
                                               : QImage::Format_RGB32);
        return image;
    }
}
