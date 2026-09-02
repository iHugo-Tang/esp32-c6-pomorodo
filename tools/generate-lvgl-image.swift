#!/usr/bin/env swift

import CoreGraphics
import Foundation
import ImageIO
import UniformTypeIdentifiers

private func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data("\(message)\n".utf8))
    exit(1)
}

guard CommandLine.arguments.count == 6,
      let size = Int(CommandLine.arguments[4]), size > 0 else {
    fail("usage: generate-lvgl-image.swift INPUT OUTPUT_PNG OUTPUT_C SIZE SYMBOL")
}

let inputURL = URL(fileURLWithPath: CommandLine.arguments[1])
let pngURL = URL(fileURLWithPath: CommandLine.arguments[2])
let cURL = URL(fileURLWithPath: CommandLine.arguments[3])
let symbol = CommandLine.arguments[5]

guard let source = CGImageSourceCreateWithURL(inputURL as CFURL, nil),
      let image = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
    fail("could not decode \(inputURL.path)")
}

let bytesPerRow = size * 4
var rgba = [UInt8](repeating: 0, count: bytesPerRow * size)
let rendered = rgba.withUnsafeMutableBytes { storage -> CGImage? in
    guard let context = CGContext(
        data: storage.baseAddress,
        width: size,
        height: size,
        bitsPerComponent: 8,
        bytesPerRow: bytesPerRow,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue |
            CGBitmapInfo.byteOrder32Big.rawValue
    ) else {
        return nil
    }
    context.interpolationQuality = .none
    context.draw(image, in: CGRect(x: 0, y: 0, width: size, height: size))
    return context.makeImage()
}

guard let rendered,
      let destination = CGImageDestinationCreateWithURL(
          pngURL as CFURL, UTType.png.identifier as CFString, 1, nil
      ) else {
    fail("could not create resized image")
}
CGImageDestinationAddImage(destination, rendered, nil)
guard CGImageDestinationFinalize(destination) else {
    fail("could not write \(pngURL.path)")
}

var rgb565 = [UInt8]()
var alpha = [UInt8]()
rgb565.reserveCapacity(size * size * 2)
alpha.reserveCapacity(size * size)
for offset in stride(from: 0, to: rgba.count, by: 4) {
    let a = rgba[offset + 3]
    let r: UInt8
    let g: UInt8
    let b: UInt8
    if a == 0 {
        r = 0
        g = 0
        b = 0
    } else {
        r = UInt8(min(255, Int(rgba[offset]) * 255 / Int(a)))
        g = UInt8(min(255, Int(rgba[offset + 1]) * 255 / Int(a)))
        b = UInt8(min(255, Int(rgba[offset + 2]) * 255 / Int(a)))
    }
    let packed = (UInt16(r >> 3) << 11) |
        (UInt16(g >> 2) << 5) |
        UInt16(b >> 3)
    rgb565.append(UInt8(packed & 0xff))
    rgb565.append(UInt8(packed >> 8))
    alpha.append(a)
}

let bytes = rgb565 + alpha
let rows = stride(from: 0, to: bytes.count, by: 16).map { start in
    let end = min(start + 16, bytes.count)
    return "    " + bytes[start..<end]
        .map { String(format: "0x%02x", $0) }
        .joined(separator: ", ") + ","
}.joined(separator: "\n")

let sourceCode = """
#include "lvgl.h"

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST
uint8_t \(symbol)_map[] = {
\(rows)
};

const lv_image_dsc_t \(symbol) = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565A8,
        .flags = 0,
        .w = \(size),
        .h = \(size),
        .stride = \(size * 2),
    },
    .data_size = sizeof(\(symbol)_map),
    .data = \(symbol)_map,
};
"""

do {
    try sourceCode.write(to: cURL, atomically: true, encoding: .utf8)
} catch {
    fail("could not write \(cURL.path): \(error)")
}
