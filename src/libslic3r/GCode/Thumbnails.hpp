#ifndef slic3r_GCodeThumbnails_hpp_
#define slic3r_GCodeThumbnails_hpp_

#include "../Point.hpp"
#include "../PrintConfig.hpp"
#include "ThumbnailData.hpp"
#if ENABLE_BINARIZED_GCODE
#include "GCode/GCodeBinarizer.hpp"
#endif // ENABLE_BINARIZED_GCODE

#include <vector>
#include <memory>
#include <string_view>

#include <boost/beast/core/detail/base64.hpp>

namespace Slic3r::GCodeThumbnails {

struct CompressedImageBuffer
{
    void       *data { nullptr };
    size_t      size { 0 };
    virtual ~CompressedImageBuffer() {}
    virtual std::string_view tag() const = 0;
};

std::unique_ptr<CompressedImageBuffer> compress_thumbnail(const ThumbnailData &data, GCodeThumbnailsFormat format);

template<typename WriteToOutput, typename ThrowIfCanceledCallback>
inline void export_thumbnails_to_file(ThumbnailsGeneratorCallback &thumbnail_cb, const std::vector<Vec2d> &sizes, GCodeThumbnailsFormat format, WriteToOutput output, ThrowIfCanceledCallback throw_if_canceled)
{
    // Write thumbnails using base64 encoding
    if (thumbnail_cb != nullptr) {
        static constexpr const size_t max_row_length = 78;
        ThumbnailsList thumbnails = thumbnail_cb(ThumbnailsParams{ sizes, true, true, true, true });
        for (const ThumbnailData& data : thumbnails)
            if (data.is_valid()) {
                auto compressed = compress_thumbnail(data, format);
                if (compressed->data && compressed->size) {
                    std::string encoded;
                    encoded.resize(boost::beast::detail::base64::encoded_size(compressed->size));
                    encoded.resize(boost::beast::detail::base64::encode((void*)encoded.data(), (const void*)compressed->data, compressed->size));

                    output((boost::format("\n;\n; %s begin %dx%d %d\n") % compressed->tag() % data.width % data.height % encoded.size()).str().c_str());

                    while (encoded.size() > max_row_length) {
                        output((boost::format("; %s\n") % encoded.substr(0, max_row_length)).str().c_str());
                        encoded = encoded.substr(max_row_length);
                    }

                    if (encoded.size() > 0)
                        output((boost::format("; %s\n") % encoded).str().c_str());

                    output((boost::format("; %s end\n;\n") % compressed->tag()).str().c_str());
                }
                throw_if_canceled();
            }
    }
}

#if ENABLE_BINARIZED_GCODE
template<typename ThrowIfCanceledCallback>
inline void generate_binary_thumbnails(ThumbnailsGeneratorCallback& thumbnail_cb, std::vector<bgcode::ThumbnailBlock>& out_thumbnails,
    const std::vector<Vec2d>& sizes, GCodeThumbnailsFormat format, ThrowIfCanceledCallback throw_if_canceled)
{
    out_thumbnails.clear();
    if (thumbnail_cb != nullptr) {
        ThumbnailsList thumbnails = thumbnail_cb(ThumbnailsParams{ sizes, true, true, true, true });
        for (const ThumbnailData& data : thumbnails) {
            if (data.is_valid()) {
                auto compressed = compress_thumbnail(data, format);
                if (compressed->data != nullptr && compressed->size > 0) {
                    bgcode::ThumbnailBlock& block = out_thumbnails.emplace_back(bgcode::ThumbnailBlock());
                    block.width = (uint16_t)data.width;
                    block.height = (uint16_t)data.height;
                    switch (format) {
                    case GCodeThumbnailsFormat::PNG: { block.format = (uint16_t)bgcode::EThumbnailFormat::PNG; break; }
                    case GCodeThumbnailsFormat::JPG: { block.format = (uint16_t)bgcode::EThumbnailFormat::JPG; break; }
                    case GCodeThumbnailsFormat::QOI: { block.format = (uint16_t)bgcode::EThumbnailFormat::QOI; break; }
                    }
                    block.data.resize(compressed->size);
                    memcpy(block.data.data(), compressed->data, compressed->size);
                }
            }
        }
    }
}
#endif // ENABLE_BINARIZED_GCODE

} // namespace Slic3r::GCodeThumbnails

#endif // slic3r_GCodeThumbnails_hpp_
