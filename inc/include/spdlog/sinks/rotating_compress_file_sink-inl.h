// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/rotating_compress_file_sink.h>
#endif

#include <spdlog/common.h>

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/fmt/fmt.h>

#include <cerrno>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include <zlib.h>

namespace spdlog {
namespace sinks {

template <typename Mutex>
SPDLOG_INLINE rotating_compress_file_sink<Mutex>::rotating_compress_file_sink(
    filename_t base_filename,
    std::size_t max_size,
    std::size_t max_files,
    bool rotate_on_open,
    const file_event_handlers &event_handlers)
    : base_filename_(std::move(base_filename)),
      max_size_(max_size),
      max_files_(max_files),
      file_helper_{event_handlers} {
    if (max_size == 0) {
        throw_spdlog_ex("rotating_compress sink constructor: max_size arg cannot be zero");
    }

    if (max_files > MaxFiles) {
        throw_spdlog_ex("rotating_compress sink constructor: max_files arg cannot exceed MaxFiles");
    }

    file_helper_.open(calc_filename(base_filename_, 0));
    current_size_ = file_helper_.size();  // expensive. called only once
    if (rotate_on_open && current_size_ > 0) {
        rotate_();
        current_size_ = 0;
    }
}

// calc filename according to index and file extension if exists.
// e.g. calc_filename("logs/app.log", 3) => "logs/app.3.log".
template <typename Mutex>
SPDLOG_INLINE filename_t rotating_compress_file_sink<Mutex>::calc_filename(const filename_t &filename,
                                                                              std::size_t index) {
    if (index == 0U) {
        return filename;
    }

    filename_t basename;
    filename_t ext;
    std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
    return fmt_lib::format(SPDLOG_FMT_STRING(SPDLOG_FILENAME_T("{}.{}{}")), basename, index, ext);
}

template <typename Mutex>
SPDLOG_INLINE filename_t rotating_compress_file_sink<Mutex>::filename() {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    return file_helper_.filename();
}

template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::rotate_now() {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    rotate_();
}

template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::set_max_size(std::size_t max_size) {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    if (max_size == 0) {
        throw_spdlog_ex("rotating_compress sink set_max_size: max_size arg cannot be zero");
    }
    max_size_ = max_size;
}

template <typename Mutex>
SPDLOG_INLINE std::size_t rotating_compress_file_sink<Mutex>::get_max_size() {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    return max_size_;
}

template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::set_max_files(std::size_t max_files) {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    if (max_files > MaxFiles) {
        throw_spdlog_ex("rotating_compress sink set_max_files: max_files arg cannot exceed MaxFiles");
    }
    max_files_ = max_files;
}

template <typename Mutex>
SPDLOG_INLINE std::size_t rotating_compress_file_sink<Mutex>::get_max_files() {
    std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
    return max_files_;
}

template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::sink_it_(const details::log_msg &msg) {
    memory_buf_t formatted;
    base_sink<Mutex>::formatter_->format(msg, formatted);
    auto new_size = current_size_ + formatted.size();

    // rotate if the new estimated file size exceeds max size.
    // rotate only if the real size > 0 to better deal with full disk (see issue #2261).
    if (new_size > max_size_) {
        file_helper_.flush();
        if (file_helper_.size() > 0) {
            rotate_();
            new_size = formatted.size();
        }
    }
    file_helper_.write(formatted);
    current_size_ = new_size;
}

template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::flush_() {
    file_helper_.flush();
}

// Rotate files:
// app.log           -> app.1.log
// app.1.log         -> app.2.log
// ...
// app.(N-1).log     -> app_<timestamp>.gz  (compressed, kept forever)
// Compressed archives are never deleted.
template <typename Mutex>
SPDLOG_INLINE void rotating_compress_file_sink<Mutex>::rotate_() {
    using details::os::filename_to_str;
    using details::os::path_exists;

    file_helper_.close();

    // shift existing rotated files upward by one index.
    // The file at index (max_files_ - 1) is compressed into a timestamped
    // archive and kept on disk forever.
    for (auto i = max_files_; i > 0; --i) {
        filename_t src = calc_filename(base_filename_, i - 1);
        if (!path_exists(src)) {
            continue;
        }

        if (i == max_files_) {
            // compress the oldest plain log into a fixed-format archive, kept forever.
            filename_t target = make_archive_filename_();
            if (!compress_file_(src, target)) {
                file_helper_.reopen(true);
                current_size_ = 0;
                throw_spdlog_ex("rotating_compress_file_sink: failed compressing " +
                                    filename_to_str(src) + " to " + filename_to_str(target),
                                errno);
            }
            (void)details::os::remove(src);
        } else {
            filename_t target = calc_filename(base_filename_, i);
            if (!rename_file_(src, target)) {
                // workaround for a windows issue where very high rotation rates can
                // cause rename to fail with permission denied (because of antivirus?).
                details::os::sleep_for_millis(100);
                if (!rename_file_(src, target)) {
                    file_helper_.reopen(
                        true);  // truncate the log file anyway to prevent it to grow beyond its limit!
                    current_size_ = 0;
                    throw_spdlog_ex("rotating_compress_file_sink: failed renaming " +
                                        filename_to_str(src) + " to " + filename_to_str(target),
                                    errno);
                }
            }
        }
    }

    file_helper_.reopen(true);
}

// build a fixed-format archive filename from base_filename_:
// e.g. base "app.log" => "app_20260827_153012.gz"
template <typename Mutex>
SPDLOG_INLINE filename_t rotating_compress_file_sink<Mutex>::make_archive_filename_() {
    filename_t basename;
    filename_t ext;
    std::tie(basename, ext) = details::file_helper::split_by_extension(base_filename_);

    std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", &tm_buf);

    return fmt_lib::format(SPDLOG_FMT_STRING(SPDLOG_FILENAME_T("{}_{}.gz")), basename, ts);
}

// delete the target if exists, and rename the src file to target.
// return true on success, false otherwise.
template <typename Mutex>
SPDLOG_INLINE bool rotating_compress_file_sink<Mutex>::rename_file_(const filename_t &src_filename,
                                                                      const filename_t &target_filename) {
    (void)details::os::remove(target_filename);
    return details::os::rename(src_filename, target_filename) == 0;
}

// gzip-compress src_filename into target_filename using zlib.
// returns true on success.
template <typename Mutex>
SPDLOG_INLINE bool rotating_compress_file_sink<Mutex>::compress_file_(const filename_t &src_filename,
                                                                        const filename_t &target_filename) {
    using details::os::filename_to_str;

    FILE *in = nullptr;
    gzFile out = nullptr;

#if defined(SPDLOG_WCHAR_FILENAMES) && defined(_WIN32)
    // filename_t is std::wstring: use wide-char Windows APIs.
    if (_wfopen_s(&in, src_filename.c_str(), L"rb") != 0 || in == nullptr) {
        return false;
    }
    out = gzopen_w(target_filename.c_str(), "wb");
#else
    // filename_t is std::string: use standard char-based APIs.
    in = std::fopen(filename_to_str(src_filename).c_str(), "rb");
    if (in == nullptr) {
        return false;
    }
    out = gzopen(filename_to_str(target_filename).c_str(), "wb");
#endif

    if (out == nullptr) {
        std::fclose(in);
        return false;
    }

    constexpr std::size_t kBufSize = 16 * 1024;
    std::vector<unsigned char> buffer(kBufSize);
    bool ok = true;

    while (true) {
        std::size_t n = std::fread(buffer.data(), 1, kBufSize, in);
        if (n == 0) {
            break;
        }
        if (gzwrite(out, buffer.data(), static_cast<unsigned>(n)) != static_cast<int>(n)) {
            ok = false;
            break;
        }
    }

    std::fclose(in);
    gzclose(out);

    if (!ok) {
        (void)details::os::remove(target_filename);
    }
    return ok;
}

}  // namespace sinks
}  // namespace spdlog
