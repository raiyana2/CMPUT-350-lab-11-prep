#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : mSize(size) {}

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        const size_t invalidID = std::numeric_limits<size_t>::max();
        if (rectSize.x < 0 || rectSize.y < 0 || rectSize.x > mSize.x || rectSize.y > mSize.y)
            return {false, invalidID};

        float x = mX;
        float y = mY;
        float rowHeight = mRowHeight;
        if (x + rectSize.x > mSize.x) {
            x = 0;
            y += rowHeight;
            rowHeight = 0;
        }
        if (y + rectSize.y > mSize.y)
            return {false, invalidID};

        mBounds.push_back({{x, y}, rectSize});
        mX = x + rectSize.x;
        mY = y;
        mRowHeight = std::max(rowHeight, rectSize.y);
        return {true, mBounds.size() - 1};
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const { return mBounds.at(rectID); }

private:
    sf::Vector2f mSize;
    float mX = 0;
    float mY = 0;
    float mRowHeight = 0;
    std::vector<sf::FloatRect> mBounds;
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) {
    const sf::Vector2u size = img.getSize();
    if (size.x == 0 || size.y == 0)
        return {};

    sf::Image padded(sf::Vector2u{size.x + 2 * pixels, size.y + 2 * pixels});
    for (unsigned int y = 0; y < padded.getSize().y; ++y) {
        for (unsigned int x = 0; x < padded.getSize().x; ++x) {
            unsigned int sourceX = x < pixels ? 0 : std::min(x - pixels, size.x - 1);
            unsigned int sourceY = y < pixels ? 0 : std::min(y - pixels, size.y - 1);
            padded.setPixel({x, y}, img.getPixel({sourceX, sourceY}));
        }
    }
    return padded;
}

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image image;
        if (!image.loadFromFile(filepath))
            throw std::runtime_error("Failed to load image: " + filepath);

        sf::Image padded = padImage(image, static_cast<uint>(mPadding));
        auto [success, id] = mPacker.pack(sf::Vector2f(padded.getSize()));
        if (!success)
            throw std::runtime_error("Texture atlas is full: " + filepath);

        sf::FloatRect bounds = mPacker.getBounds(id);
        mTexture.update(padded, sf::Vector2u(bounds.position));
        return id;
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect bounds = mPacker.getBounds(id);
        sf::Vector2f atlasSize(mTexture.getSize());
        float padding = static_cast<float>(mPadding);
        return {{(bounds.position.x + padding) / atlasSize.x,
                 1.f - (bounds.position.y + bounds.size.y - padding) / atlasSize.y},
                {(bounds.size.x - 2 * padding) / atlasSize.x,
                 (bounds.size.y - 2 * padding) / atlasSize.y}};
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        sf::Vector2f size = mPacker.getBounds(id).size;
        float padding = static_cast<float>(mPadding);
        return sf::Vector2u(size - sf::Vector2f{2 * padding, 2 * padding});
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
