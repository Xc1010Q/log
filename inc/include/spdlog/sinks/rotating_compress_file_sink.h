// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

<<<<<<< HEAD
#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <mutex>
#include <string>
=======
//
// Rotating file sink based on size, with optional compression of rotated
// (archived) log files using zlib (gzip) or zstd.
//
// - Active log file:   logs/app.log
// - Rotated archives:  logs/app.1.log[.gz|.zst] ... app.N.log[.gz|.zst]
//
// Rotation happens when the active file exceeds max_size. Archives occupy the
// fixed sequence slots 1..max_files and a slot is atomically replaced when the
// sequence wraps. Compression runs on a dedicated background thread. Automatic
// rotation never waits for compression: while all compression slots are
// occupied, records remain in the active file and it may temporarily exceed
// max_size. Rotation is retried on the next record.
//
// Requires C++11. The zlib and zstd backends are optional and independent.
// Define SPDLOG_ROTATING_COMPRESS_ZLIB and/or SPDLOG_ROTATING_COMPRESS_ZSTD
// before including this header to compile the corresponding backend, then
// link with -lz and/or -lzstd. Selecting a backend that was not compiled in
// throws spdlog_ex at construction. Backends are explicit rather than inferred
// from header availability, because finding a header does not arrange the
// corresponding linker dependency.
// These backend macros must have identical values in every translation unit
// that instantiates this sink.
//
// Do not share one active base_filename between processes or between multiple
// sink instances. Archive names avoid accidental replacement, but active-file
// rotation and retention are intentionally coordinated only inside one sink.
// file_event_handlers must not call back into the same sink.
//

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/details/synchronous_factory.h>
#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <condition_variable>
#include <exception>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#ifndef _WIN32
#    include <cerrno>
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

#if defined(SPDLOG_ROTATING_COMPRESS_ZLIB) && defined(SPDLOG_ROTATING_COMPRESS_NO_ZLIB)
#    error "SPDLOG_ROTATING_COMPRESS_ZLIB conflicts with SPDLOG_ROTATING_COMPRESS_NO_ZLIB"
#endif

#if defined(SPDLOG_ROTATING_COMPRESS_ZSTD) && defined(SPDLOG_ROTATING_COMPRESS_NO_ZSTD)
#    error "SPDLOG_ROTATING_COMPRESS_ZSTD conflicts with SPDLOG_ROTATING_COMPRESS_NO_ZSTD"
#endif

#ifdef SPDLOG_ROTATING_COMPRESS_ZLIB
#    include <zlib.h>
#endif
#ifdef SPDLOG_ROTATING_COMPRESS_ZSTD
#    include <zstd.h>
#    if ZSTD_VERSION_NUMBER < 10400
#        error "rotating_compress_file_sink requires zstd 1.4.0 or newer"
#    endif
#endif

#ifdef _WIN32
#    include <windows.h>
#else
#    include <dirent.h>
#endif
>>>>>>> 9459495b80f6cdc1926b5669612df7259e7e7ed3

namespace spdlog {
namespace sinks {

<<<<<<< HEAD
//
// Rotating file sink based on size.
// The oldest rotated plain file is gzip-compressed into a timestamped
// archive and kept on disk forever (archives are never deleted).
// max_files controls how many plain rotated files are kept.
//
// Naming (base = "app.log", max_files = 3):
//   app.log                  current active file
//   app.1.log                most recent rotated file
//   app.2.log                ...
//   app_20260827_153012.gz  compressed archive (kept forever)
//   app_20260827_160503.gz  ...
=======
enum class rotation_compress_type {
    none,  // keep rotated files uncompressed
    zlib,  // gzip format, appends ".gz"
    zstd   // zstd format, appends ".zst"
};

namespace rotating_compress_details {

inline filename_t dir_name(const filename_t &fname) {
    const auto pos = fname.find_last_of(details::os::folder_seps_filename);
    if (pos == filename_t::npos) {
#ifdef _WIN32
        if (fname.size() >= 2 && fname[1] == SPDLOG_FILENAME_T(':')) {
            return fname.substr(0, 2) + SPDLOG_FILENAME_T(".");
        }
#endif
        return SPDLOG_FILENAME_T(".");
    }
    if (pos == 0) {
        return fname.substr(0, 1);
    }
#ifdef _WIN32
    if (pos == 2 && fname.size() > 2 && fname[1] == SPDLOG_FILENAME_T(':')) {
        return fname.substr(0, 3);
    }
#endif
    return fname.substr(0, pos);
}

inline filename_t base_name(const filename_t &fname) {
    const auto pos = fname.find_last_of(details::os::folder_seps_filename);
#ifdef _WIN32
    if (pos == filename_t::npos && fname.size() >= 2 && fname[1] == SPDLOG_FILENAME_T(':')) {
        return fname.substr(2);
    }
#endif
    return pos == filename_t::npos ? fname : fname.substr(pos + 1);
}

inline filename_t join_path(const filename_t &dir, const filename_t &name) {
    if (dir.empty()) {
        return name;
    }
    if (dir.find_last_of(details::os::folder_seps_filename) == dir.size() - 1) {
        return dir + name;
    }
#ifdef _WIN32
    return dir + SPDLOG_FILENAME_T("\\") + name;
#else
    return dir + SPDLOG_FILENAME_T("/") + name;
#endif
}

// List the entry names of a directory. Returns false on a directory I/O error.
inline bool list_dir(const filename_t &dir, std::vector<filename_t> &names) {
    names.clear();
#ifdef _WIN32
#ifdef SPDLOG_WCHAR_FILENAMES
    WIN32_FIND_DATAW ffd;
    HANDLE h = FindFirstFileW(join_path(dir, SPDLOG_FILENAME_T("*")).c_str(), &ffd);
#else
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(join_path(dir, SPDLOG_FILENAME_T("*")).c_str(), &ffd);
#endif
#ifdef SPDLOG_WCHAR_FILENAMES
    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            names.push_back(ffd.cFileName);
        }
    } while (FindNextFileW(h, &ffd) != 0);
#else
    if (h == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            names.push_back(ffd.cFileName);
        }
    } while (FindNextFileA(h, &ffd) != 0);
#endif
    const bool ok = GetLastError() == ERROR_NO_MORE_FILES;
    FindClose(h);
    return ok;
#else
    DIR *d = opendir(dir.c_str());
    if (d == nullptr) {
        return false;
    }
    errno = 0;
    while (dirent *entry = readdir(d)) {
        names.push_back(entry->d_name);
    }
    const bool ok = errno == 0;
    closedir(d);
    return ok;
#endif
}

inline bool is_regular_file(const filename_t &path) {
#ifdef _WIN32
#    ifdef SPDLOG_WCHAR_FILENAMES
    const DWORD attributes = GetFileAttributesW(path.c_str());
#    else
    const DWORD attributes = GetFileAttributesA(path.c_str());
#    endif
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
    struct stat info;
    return ::stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
#endif
}

inline FILE *open_binary_file(const filename_t &path, const filename_t &mode) {
    FILE *file = nullptr;
    if (details::os::fopen_s(&file, path, mode)) {
        return nullptr;
    }
    return file;
}

inline bool sync_file_path(const filename_t &path) {
    FILE *file = open_binary_file(path, SPDLOG_FILENAME_T("rb+"));
    if (file == nullptr) {
        return false;
    }
    const bool ok = details::os::fsync(file);
    return std::fclose(file) == 0 && ok;
}

inline bool sync_parent_directory(const filename_t &path) {
#ifdef _WIN32
    // move_no_replace uses MOVEFILE_WRITE_THROUGH on Windows.
    (void)path;
    return true;
#else
    const filename_t dir = dir_name(path);
#    ifdef O_DIRECTORY
    const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
#    else
    const int fd = ::open(dir.c_str(), O_RDONLY);
#    endif
    if (fd == -1) {
        return false;
    }
    const bool ok = ::fsync(fd) == 0;
    ::close(fd);
    return ok;
#endif
}

// Move a file without replacing an existing destination. On POSIX, link()
// gives us the required no-replace operation; on Windows MoveFileEx without
// MOVEFILE_REPLACE_EXISTING has the same behavior.
inline bool move_no_replace(const filename_t &src, const filename_t &dst) {
    if (details::os::path_exists(dst)) {
        return false;
    }
#ifdef _WIN32
#ifdef SPDLOG_WCHAR_FILENAMES
    return MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    return MoveFileExA(src.c_str(), dst.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#endif
#else
    if (::link(src.c_str(), dst.c_str()) != 0) {
        return false;
    }
    if (details::os::remove(src) == 0) {
        return true;
    }
    // link() succeeded but unlinking the source did not. Roll the destination
    // back so callers never observe a failed "move" that also created a file.
    (void)details::os::remove(dst);
    return false;
#endif
}

// Atomically replace a fixed archive slot with the active file. The source and
// destination are always in the same directory, so POSIX rename() is atomic;
// MoveFileEx provides the equivalent replace operation on Windows.
inline bool move_replace(const filename_t &src, const filename_t &dst) {
#ifdef _WIN32
#    ifdef SPDLOG_WCHAR_FILENAMES
    return MoveFileExW(src.c_str(), dst.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#    else
    return MoveFileExA(src.c_str(), dst.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#    endif
#else
    return ::rename(src.c_str(), dst.c_str()) == 0;
#endif
}

inline bool publish_compressed_file(const filename_t &src,
                                    const filename_t &tmp,
                                    const filename_t &dst,
                                    const char **failure_stage) {
    // Never replace an existing archive. Refusing a collision is safer than
    // silently overwriting a valid archive from another process.
    if (details::os::path_exists(dst)) {
        *failure_stage = "compressed destination already exists";
        details::os::remove_if_exists(tmp);
        return false;
    }

    if (!sync_file_path(tmp)) {
        *failure_stage = "syncing compressed temporary failed";
        details::os::remove_if_exists(tmp);
        return false;
    }
    if (!move_no_replace(tmp, dst)) {
        *failure_stage = "publishing compressed destination failed";
        details::os::remove_if_exists(tmp);
        return false;
    }

    // Persist the compressed data and its directory entry before deleting the
    // only uncompressed source copy.
    if (!sync_parent_directory(dst)) {
        *failure_stage = "syncing published destination directory failed";
        return false;
    }

    // Delete the uncompressed source only after the compressed destination is
    // safely published. If this fails, both copies remain recoverable.
    if (details::os::remove(src) != 0 && details::os::path_exists(src)) {
        *failure_stage = "deleting uncompressed source failed";
        return false;
    }
    if (!sync_parent_directory(dst)) {
        *failure_stage = "syncing source deletion failed";
        return false;
    }
    return true;
}

#ifdef SPDLOG_ROTATING_COMPRESS_ZLIB
inline gzFile open_gzip_file(const filename_t &path, const std::string &mode) {
#ifdef _WIN32
#ifdef SPDLOG_WCHAR_FILENAMES
    return gzopen_w(path.c_str(), mode.c_str());
#else
    return gzopen(path.c_str(), mode.c_str());
#endif
#else
    return gzopen(path.c_str(), mode.c_str());
#endif
}

// Compress src file into a gzip file (src + ".gz"), then remove src.
// The compressed data is first written to a temporary name that does not
// match the archive naming pattern, so the retention pruning never counts
// both the source and a half-written compressed file.
// Returns true on success.
inline bool gzip_compress_file(const filename_t &src, int level, const char **failure_stage) {
    const filename_t dst = src + SPDLOG_FILENAME_T(".gz");
    const filename_t tmp = dst + SPDLOG_FILENAME_T(".tmp");
    std::string mode = "wb";
    if (level != -1) {
        mode += static_cast<char>('0' + level);
    }
    // Finish all potentially allocating work before acquiring C resources.
    FILE *in = open_binary_file(src, SPDLOG_FILENAME_T("rb"));
    if (in == nullptr) {
        *failure_stage = "opening uncompressed source failed";
        return false;
    }
    gzFile out = open_gzip_file(tmp, mode);
    if (out == nullptr) {
        *failure_stage = "opening gzip temporary failed";
        std::fclose(in);
        return false;
    }
    char buf[64 * 1024];
    bool ok = true;
    for (;;) {
        const std::size_t n = std::fread(buf, 1, sizeof(buf), in);
        if (n > 0 && gzwrite(out, buf, static_cast<unsigned>(n)) != static_cast<int>(n)) {
            ok = false;
            break;
        }
        if (n < sizeof(buf)) {
            ok = ok && std::ferror(in) == 0;
            break;
        }
    }
    ok = std::fclose(in) == 0 && ok;
    ok = gzclose(out) == Z_OK && ok;
    if (!ok) {
        *failure_stage = "gzip stream or source I/O failed";
        details::os::remove_if_exists(tmp);
        return false;
    }
    return publish_compressed_file(src, tmp, dst, failure_stage);
}
#endif  // SPDLOG_ROTATING_COMPRESS_ZLIB

#ifdef SPDLOG_ROTATING_COMPRESS_ZSTD
// Compress src file into a zstd file (src + ".zst"), then remove src.
// The compressed data is first written to a temporary name that does not
// match the archive naming pattern, so the retention pruning never counts
// both the source and a half-written compressed file.
// Returns true on success.
inline bool zstd_compress_file(const filename_t &src, int level, const char **failure_stage) {
    std::vector<char> in_buf(ZSTD_CStreamInSize());
    std::vector<char> out_buf(ZSTD_CStreamOutSize());
    const filename_t dst = src + SPDLOG_FILENAME_T(".zst");
    const filename_t tmp = dst + SPDLOG_FILENAME_T(".tmp");
    ZSTD_CCtx *cctx = ZSTD_createCCtx();
    if (cctx == nullptr) {
        *failure_stage = "creating zstd context failed";
        return false;
    }
    const std::size_t parameter_result = ZSTD_CCtx_setParameter(
        cctx, ZSTD_c_compressionLevel, level == -1 ? ZSTD_CLEVEL_DEFAULT : level);
    if (ZSTD_isError(parameter_result)) {
        *failure_stage = "setting zstd compression level failed";
        ZSTD_freeCCtx(cctx);
        return false;
    }
    FILE *in = open_binary_file(src, SPDLOG_FILENAME_T("rb"));
    if (in == nullptr) {
        *failure_stage = "opening uncompressed source failed";
        ZSTD_freeCCtx(cctx);
        return false;
    }
    FILE *out = open_binary_file(tmp, SPDLOG_FILENAME_T("wb"));
    if (out == nullptr) {
        *failure_stage = "opening zstd temporary failed";
        std::fclose(in);
        ZSTD_freeCCtx(cctx);
        return false;
    }

    bool ok = true;
    for (;;) {
        const std::size_t read_n = std::fread(in_buf.data(), 1, in_buf.size(), in);
        const bool last = read_n < in_buf.size();
        ZSTD_inBuffer input{in_buf.data(), read_n, 0};
        const ZSTD_EndDirective mode = last ? ZSTD_e_end : ZSTD_e_continue;
        std::size_t remaining;
        do {
            ZSTD_outBuffer output{out_buf.data(), out_buf.size(), 0};
            remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
            if (ZSTD_isError(remaining)) {
                ok = false;
                break;
            }
            if (output.pos > 0 &&
                std::fwrite(out_buf.data(), 1, output.pos, out) != output.pos) {
                ok = false;
                break;
            }
        } while (input.pos < input.size || (last && remaining > 0));
        if (!ok) {
            break;
        }
        if (last) {
            ok = std::ferror(in) == 0;
            break;
        }
    }
    ok = std::fclose(in) == 0 && ok;
    ok = std::fflush(out) == 0 && ok;
    ok = std::fclose(out) == 0 && ok;
    ZSTD_freeCCtx(cctx);
    if (!ok) {
        *failure_stage = "zstd stream or source I/O failed";
        details::os::remove_if_exists(tmp);
        return false;
    }
    return publish_compressed_file(src, tmp, dst, failure_stage);
}
#endif  // SPDLOG_ROTATING_COMPRESS_ZSTD

}  // namespace rotating_compress_details

//
// Rotating file sink based on size with optional archive compression.
// Rotated files are queued for compression on a background thread owned by
// the sink; the queue is fully drained before the sink is destroyed.
>>>>>>> 9459495b80f6cdc1926b5669612df7259e7e7ed3
//
template <typename Mutex>
class rotating_compress_file_sink final : public base_sink<Mutex> {
public:
<<<<<<< HEAD
    static constexpr size_t MaxFiles = 200000;

    rotating_compress_file_sink(filename_t base_filename,
                                 std::size_t max_size,
                                 std::size_t max_files,
                                 bool rotate_on_open = false,
                                 const file_event_handlers &event_handlers = {});

    static filename_t calc_filename(const filename_t &filename, std::size_t index);
    filename_t filename();
    void rotate_now();
    void set_max_size(std::size_t max_size);
    std::size_t get_max_size();
    void set_max_files(std::size_t max_files);
    std::size_t get_max_files();

protected:
    void sink_it_(const details::log_msg &msg) override;
    void flush_() override;

private:
    // Rotate files:
    // app.log           -> app.1.log
    // app.1.log         -> app.2.log
    // ...
    // app.(N-1).log     -> app_<timestamp>.gz  (compressed, kept forever)
    void rotate_();

    // build a fixed-format archive filename from base_filename_:
    // e.g. base "app.log" => "app_20260827_153012.gz"
    filename_t make_archive_filename_();

    // delete the target if exists, and rename the src file to target.
    // return true on success, false otherwise.
    bool rename_file_(const filename_t &src_filename, const filename_t &target_filename);

    // gzip-compress src_filename into target_filename (typically *.gz).
    // returns true on success.
    bool compress_file_(const filename_t &src_filename, const filename_t &target_filename);
=======
    rotating_compress_file_sink(filename_t base_filename,
                                std::size_t max_size,
                                std::size_t max_files,
                                rotation_compress_type compress_type = rotation_compress_type::none,
                                int compression_level = -1,
                                bool rotate_on_open = false,
                                const file_event_handlers &event_handlers = {})
        : base_filename_(std::move(base_filename)),
          max_size_(max_size),
          max_files_(max_files),
          compress_type_(compress_type),
          compression_level_(compression_level),
          file_helper_(event_handlers) {
        if (max_size == 0) {
            throw_spdlog_ex("rotating_compress_file_sink: max_size must be > 0");
        }
        if (max_files == 0) {
            throw_spdlog_ex("rotating_compress_file_sink: max_files must be > 0");
        }
        if (compress_type != rotation_compress_type::none &&
            compress_type != rotation_compress_type::zlib &&
            compress_type != rotation_compress_type::zstd) {
            throw_spdlog_ex("rotating_compress_file_sink: invalid compression type");
        }
#ifndef SPDLOG_ROTATING_COMPRESS_ZLIB
        if (compress_type == rotation_compress_type::zlib) {
            throw_spdlog_ex(
                "rotating_compress_file_sink: zlib support not compiled in "
                "(define SPDLOG_ROTATING_COMPRESS_ZLIB and link zlib)");
        }
#endif
#ifndef SPDLOG_ROTATING_COMPRESS_ZSTD
        if (compress_type == rotation_compress_type::zstd) {
            throw_spdlog_ex(
                "rotating_compress_file_sink: zstd support not compiled in "
                "(define SPDLOG_ROTATING_COMPRESS_ZSTD and link zstd)");
        }
#endif
#ifdef SPDLOG_ROTATING_COMPRESS_ZLIB
        if (compress_type == rotation_compress_type::zlib &&
            (compression_level < -1 || compression_level > 9)) {
            throw_spdlog_ex(
                "rotating_compress_file_sink: zlib compression level must be -1 or 0..9");
        }
#endif
#ifdef SPDLOG_ROTATING_COMPRESS_ZSTD
        if (compress_type == rotation_compress_type::zstd && compression_level != -1 &&
            (compression_level < ZSTD_minCLevel() || compression_level > ZSTD_maxCLevel())) {
            throw_spdlog_ex(
                "rotating_compress_file_sink: zstd compression level is outside backend bounds");
        }
#endif
        file_helper_.open(base_filename_);
        current_size_ = file_helper_.size();
        initialize_next_sequence_();
        if (rotate_on_open && current_size_ > 0) {
            rotate_();
        }
        if (compress_type_ != rotation_compress_type::none) {
            compress_thread_ = std::thread([this] { compress_worker_(); });
        }
    }

    ~rotating_compress_file_sink() override {
        {
            std::lock_guard<std::mutex> lock(compress_mutex_);
            compress_stop_ = true;
        }
        compress_cv_.notify_all();
        if (compress_thread_.joinable()) {
            compress_thread_.join();  // drains pending compressions
        }
        try {
            prune_old_files_();  // retry a transient final maintenance failure once
        } catch (const std::exception &exception) {
            std::fprintf(stderr,
                         "[spdlog] rotating_compress_file_sink: final prune failed: %s\n",
                         exception.what());
        } catch (...) {
            std::fprintf(stderr,
                         "[spdlog] rotating_compress_file_sink: final prune failed\n");
        }
    }

    rotating_compress_file_sink(const rotating_compress_file_sink &) = delete;
    rotating_compress_file_sink &operator=(const rotating_compress_file_sink &) = delete;

    filename_t filename() {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        return file_helper_.filename();
    }

    void rotate_now() {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        (void)rotate_();
    }

    void set_max_size(std::size_t max_size) {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        if (max_size == 0) {
            throw_spdlog_ex("rotating_compress_file_sink: max_size must be > 0");
        }
        max_size_ = max_size;
    }

    std::size_t get_max_size() {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        return max_size_;
    }

    void set_max_files(std::size_t max_files) {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        if (max_files == 0) {
            throw_spdlog_ex("rotating_compress_file_sink: max_files must be > 0");
        }
        std::lock_guard<std::mutex> maintenance_lock(maintenance_mutex_);
        max_files_ = max_files;
        try {
            prune_old_files_unlocked_();
            initialize_next_sequence_unlocked_();
        } catch (...) {
            maintenance_error_count_.fetch_add(1, std::memory_order_relaxed);
            throw;
        }
    }

    std::size_t get_max_files() {
        std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
        std::lock_guard<std::mutex> maintenance_lock(maintenance_mutex_);
        return max_files_;
    }

    std::size_t compression_error_count() const noexcept {
        return compression_error_count_.load(std::memory_order_relaxed);
    }

    std::size_t maintenance_error_count() const noexcept {
        return maintenance_error_count_.load(std::memory_order_relaxed);
    }

    // Number of automatic rotations deferred because every compression slot
    // was occupied. Deferral is lossless: the record is appended to the active
    // file, which may temporarily grow beyond max_size_.
    std::size_t deferred_rotation_count() const noexcept {
        return deferred_rotation_count_.load(std::memory_order_relaxed);
    }

    void wait_for_compression() {
        if (compress_type_ == rotation_compress_type::none) {
            return;
        }
        std::unique_lock<std::mutex> lock(compress_mutex_);
        compress_done_cv_.wait(lock, [this] {
            return compress_stop_ ||
                   (compress_queue_.empty() && compress_in_progress_.empty());
        });
        if (compress_stop_) {
            throw_spdlog_ex("rotating_compress_file_sink: compression worker stopped early");
        }
    }

protected:
    void sink_it_(const details::log_msg &msg) override {
        if (!file_helper_.is_open()) {
            throw_spdlog_ex("rotating_compress_file_sink: active log file is closed");
        }
        memory_buf_t formatted;
        base_sink<Mutex>::formatter_->format(msg, formatted);
        auto new_size = current_size_ + formatted.size();
        filename_t rotated_archive;
        if (new_size > max_size_) {
            // Match spdlog's rotating sink behavior: flushing detects buffered
            // I/O errors, while the real-size check avoids archiving an empty
            // file when a single record itself exceeds max_size_.
            file_helper_.flush();
            if (file_helper_.size() > 0) {
                // Compression backpressure must never stall the caller. If all
                // compression slots are occupied, keep appending to the active
                // file and retry rotation on the next record.
                rotated_archive = rotate_(false, true);
                new_size = current_size_ + formatted.size();
            }
        }
        file_helper_.write(formatted);
        current_size_ = new_size;
        if (!rotated_archive.empty()) {
            // Retention failure is still reported, but only after the message
            // that triggered rotation has reached the new active file.
            prune_old_files_(rotated_archive);
        }
    }

    void flush_() override {
        if (!file_helper_.is_open()) {
            throw_spdlog_ex("rotating_compress_file_sink: active log file is closed");
        }
        file_helper_.flush();
    }

private:
    // Rotate files:
    // app.log -> app.SEQUENCE.log, queued for compression. SEQUENCE is a
    // fixed slot in [1, max_files_], shared by raw/gzip/zstd representations.
    filename_t rotate_(bool prune_after_rotation = true,
                       bool defer_if_compression_full = false) {
        const filename_t archive_name = calc_archive_filename_(next_sequence_);
        const filename_t stale_gzip = archive_name + SPDLOG_FILENAME_T(".gz");
        const filename_t stale_zstd = archive_name + SPDLOG_FILENAME_T(".zst");
        const filename_t stale_gzip_tmp = stale_gzip + SPDLOG_FILENAME_T(".tmp");
        const filename_t stale_zstd_tmp = stale_zstd + SPDLOG_FILENAME_T(".tmp");
        bool archive_reserved = false;

        try {
            if (compress_type_ != rotation_compress_type::none) {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                if (compress_stop_) {
                    if (defer_if_compression_full) {
                        deferred_rotation_count_.fetch_add(1, std::memory_order_relaxed);
                        return filename_t();
                    }
                    throw_spdlog_ex("rotating_compress_file_sink: compression worker is stopping");
                }
                const std::size_t pending =
                    compress_queue_.size() + (compress_in_progress_.empty() ? 0U : 1U) +
                    compress_reserved_.size();
                const bool target_pending = archive_is_pending_unlocked_(archive_name);
                if ((pending >= max_files_ || target_pending) && defer_if_compression_full) {
                    deferred_rotation_count_.fetch_add(1, std::memory_order_relaxed);
                    return filename_t();
                }
                if (target_pending) {
                    throw_spdlog_ex(
                        "rotating_compress_file_sink: target archive slot is still compressing");
                }
                // Reserve the archive before closing/renaming the active file.
                // This closes the gap in which the worker could otherwise prune
                // a renamed file before it is inserted into the queue.
                compress_reserved_.insert(archive_name);
                archive_reserved = true;
            }

            // rotate_now() also comes through this path, so flush here even
            // though the automatic size-based path already flushed above.
            file_helper_.flush();
            file_helper_.sync();
            file_helper_.close();
            if (!rotating_compress_details::move_replace(base_filename_, archive_name)) {
                const std::string rename_error =
                    "rotating_compress_file_sink: failed replacing archive slot " +
                    details::os::filename_to_str(base_filename_) + " to " +
                    details::os::filename_to_str(archive_name);
                try {
                    // The original file was not renamed. Reopen in append mode;
                    // reopening with truncate=true would destroy existing logs.
                    file_helper_.reopen(false);
                    current_size_ = file_helper_.size();
                } catch (const std::exception &exception) {
                    throw_spdlog_ex(rename_error + "; failed reopening source: " + exception.what());
                }
                throw_spdlog_ex(rename_error);
            }
            advance_next_sequence_();
            // Reopen immediately after committing the move. No allocation or
            // maintenance work is allowed to strand the sink in a closed state.
            file_helper_.reopen(false);
            current_size_ = file_helper_.size();

            if (!rotating_compress_details::sync_parent_directory(archive_name)) {
                maintenance_error_count_.fetch_add(1, std::memory_order_relaxed);
                std::fprintf(stderr,
                             "[spdlog] rotating_compress_file_sink: failed to sync archive directory\n");
            }

            // The raw slot was replaced atomically. Remove stale representations
            // of that same logical slot before scheduling its new compression.
            const filename_t stale_paths[] = {stale_gzip, stale_zstd, stale_gzip_tmp,
                                              stale_zstd_tmp};
            bool stale_cleanup_ok = true;
            for (const filename_t &path : stale_paths) {
                if (details::os::remove_if_exists(path) != 0 &&
                    details::os::path_exists(path)) {
                    stale_cleanup_ok = false;
                }
            }
            if (!stale_cleanup_ok) {
                maintenance_error_count_.fetch_add(1, std::memory_order_relaxed);
                std::fprintf(stderr,
                             "[spdlog] rotating_compress_file_sink: failed cleaning stale archive slot\n");
            }

            if (compress_type_ != rotation_compress_type::none && stale_cleanup_ok) {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                // Keep the reservation until push succeeds. If allocation
                // fails, the catch path still knows that it owns this archive.
                compress_queue_.push(archive_name);
                compress_reserved_.erase(archive_name);
                archive_reserved = false;
                compress_cv_.notify_one();
            } else if (archive_reserved) {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                compress_reserved_.erase(archive_name);
                archive_reserved = false;
            }
            if (prune_after_rotation) {
                prune_old_files_(archive_name);
            }
            return archive_name;
        } catch (...) {
            const std::exception_ptr original_exception = std::current_exception();
            if (archive_reserved) {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                compress_reserved_.erase(archive_name);
            }
            try {
                if (!file_helper_.is_open()) {
                    file_helper_.reopen(false);
                }
                current_size_ = file_helper_.size();
            } catch (const std::exception &recovery_error) {
                throw_spdlog_ex(
                    std::string("rotating_compress_file_sink: rotation failed and active file ") +
                    "could not be recovered: " + recovery_error.what());
            }
            std::rethrow_exception(original_exception);
        }
        return filename_t();
    }

    filename_t calc_archive_filename_(std::size_t sequence) const {
        filename_t basename, ext;
        std::tie(basename, ext) = details::file_helper::split_by_extension(base_filename_);
        std::basic_ostringstream<filename_t::value_type> seq;
        seq.imbue(std::locale::classic());
        seq << sequence;
        return basename + SPDLOG_FILENAME_T(".") + seq.str() + ext;
    }

    void advance_next_sequence_() noexcept {
        next_sequence_ = next_sequence_ >= max_files_ ? 1U : next_sequence_ + 1U;
    }

    bool archive_is_pending_unlocked_(const filename_t &archive_name) const {
        if (compress_in_progress_ == archive_name ||
            compress_reserved_.count(archive_name) != 0) {
            return true;
        }
        std::queue<filename_t> queue_copy = compress_queue_;
        while (!queue_copy.empty()) {
            if (queue_copy.front() == archive_name) {
                return true;
            }
            queue_copy.pop();
        }
        return false;
    }

    struct archive_group {
        std::string source_name;
        std::vector<filename_t> paths;
    };

    struct temporary_file {
        std::size_t sequence;
        std::string source_name;
        filename_t path;
    };

    // maintenance_mutex_ is needed even for the _st sink because compression
    // always introduces an internal worker thread.
    void prune_old_files_(const filename_t &protected_archive = filename_t()) {
        std::lock_guard<std::mutex> maintenance_lock(maintenance_mutex_);
        try {
            prune_old_files_unlocked_(protected_archive);
        } catch (...) {
            maintenance_error_count_.fetch_add(1, std::memory_order_relaxed);
            throw;
        }
    }

    void prune_old_files_unlocked_(const filename_t &protected_archive = filename_t()) {
        std::set<std::string> pending;
        {
            std::lock_guard<std::mutex> lock(compress_mutex_);
            std::queue<filename_t> q = compress_queue_;
            while (!q.empty()) {
                pending.insert(details::os::filename_to_str(
                    rotating_compress_details::base_name(q.front())));
                q.pop();
            }
            if (!compress_in_progress_.empty()) {
                pending.insert(details::os::filename_to_str(
                    rotating_compress_details::base_name(compress_in_progress_)));
            }
            for (const filename_t &reserved : compress_reserved_) {
                pending.insert(details::os::filename_to_str(
                    rotating_compress_details::base_name(reserved)));
            }
        }
        if (!protected_archive.empty()) {
            pending.insert(details::os::filename_to_str(
                rotating_compress_details::base_name(protected_archive)));
        }

        std::map<std::size_t, archive_group> groups;
        std::vector<temporary_file> temporary_files;
        scan_archive_files_unlocked_(groups, temporary_files);

        for (const temporary_file &temporary : temporary_files) {
            const auto group_position = groups.find(temporary.sequence);
            if (group_position == groups.end() && pending.count(temporary.source_name) == 0) {
                // Compression is never resumed after restart. A tmp without a
                // raw or published representation is therefore crash debris.
                if (details::os::remove_if_exists(temporary.path) != 0 &&
                    details::os::path_exists(temporary.path)) {
                    throw_spdlog_ex(
                        "rotating_compress_file_sink: failed deleting orphan temporary " +
                        details::os::filename_to_str(temporary.path));
                }
                continue;
            }
            archive_group &group = groups[temporary.sequence];
            if (group.source_name.empty()) {
                group.source_name = temporary.source_name;
            }
            group.paths.push_back(temporary.path);
        }

        // Fixed slots 1..max_files_ are the complete retention policy. Slots
        // outside that range can only be leftovers after reducing max_files.
        for (const auto &item : groups) {
            if (item.first <= max_files_ || pending.count(item.second.source_name) != 0) {
                continue;
            }
            for (const filename_t &path : item.second.paths) {
                if (details::os::remove_if_exists(path) != 0 &&
                    details::os::path_exists(path)) {
                    throw_spdlog_ex("rotating_compress_file_sink: failed deleting archive slot " +
                                    details::os::filename_to_str(path));
                }
            }
        }
    }

    void initialize_next_sequence_() {
        std::lock_guard<std::mutex> maintenance_lock(maintenance_mutex_);
        initialize_next_sequence_unlocked_();
    }

    void initialize_next_sequence_unlocked_() {
        std::map<std::size_t, archive_group> groups;
        std::vector<temporary_file> temporary_files;
        scan_archive_files_unlocked_(groups, temporary_files);
        (void)temporary_files;  // tmp-only files never occupy a sequence slot

        std::set<std::size_t> occupied;
        for (const auto &item : groups) {
            if (item.first >= 1U && item.first <= max_files_) {
                occupied.insert(item.first);
            }
        }
        if (occupied.empty() || occupied.size() >= max_files_) {
            next_sequence_ = 1U;
            return;
        }
        const std::size_t highest = *occupied.rbegin();
        if (highest < max_files_) {
            next_sequence_ = highest + 1U;
            return;
        }
        for (std::size_t sequence = 1U; sequence <= max_files_; ++sequence) {
            if (occupied.count(sequence) == 0) {
                next_sequence_ = sequence;
                return;
            }
        }
        next_sequence_ = 1U;
    }

    void scan_archive_files_unlocked_(std::map<std::size_t, archive_group> &groups,
                                      std::vector<temporary_file> &temporary_files) const {
        filename_t basename, ext;
        std::tie(basename, ext) = details::file_helper::split_by_extension(base_filename_);
        const std::string prefix = details::os::filename_to_str(
                                       rotating_compress_details::base_name(basename)) + ".";
        const std::string extension = details::os::filename_to_str(ext);
        const filename_t dir = rotating_compress_details::dir_name(base_filename_);

        std::vector<filename_t> entries;
        if (!rotating_compress_details::list_dir(dir, entries)) {
            throw_spdlog_ex("rotating_compress_file_sink: failed listing directory " +
                            details::os::filename_to_str(dir));
        }
        for (const filename_t &entry : entries) {
            const std::string name = details::os::filename_to_str(entry);
            std::string source_name;
            std::size_t sequence = 0;
            const bool complete_archive =
                archive_source_name_(name, prefix, extension, source_name, sequence);
            const bool temporary_archive =
                !complete_archive &&
                archive_temporary_source_name_(name, prefix, extension, source_name, sequence);
            if (!complete_archive && !temporary_archive) {
                continue;
            }
            const filename_t path = rotating_compress_details::join_path(dir, entry);
            if (!rotating_compress_details::is_regular_file(path)) {
                continue;
            }
            if (temporary_archive) {
                temporary_files.push_back(temporary_file{sequence, source_name, path});
                continue;
            }
            archive_group &group = groups[sequence];
            group.source_name = source_name;
            group.paths.push_back(path);
        }
    }

    static bool parse_sequence_(const std::string &text, std::size_t &sequence) {
        if (text.empty() || (text.size() > 1U && text[0] == '0')) {
            return false;
        }
        std::size_t value = 0;
        for (char digit : text) {
            if (digit < '0' || digit > '9') {
                return false;
            }
            const std::size_t numeric = static_cast<std::size_t>(digit - '0');
            if (value > ((std::numeric_limits<std::size_t>::max)() - numeric) / 10U) {
                return false;
            }
            value = value * 10U + numeric;
        }
        if (value == 0U) {
            return false;
        }
        sequence = value;
        return true;
    }

    // Exact source form: app.SEQUENCE.log, without padding or time metadata.
    static bool is_archive_source_name_(const std::string &name,
                                        const std::string &prefix,
                                        const std::string &ext,
                                        std::size_t &sequence) {
        if (name.size() <= prefix.size() + ext.size() ||
            name.compare(0, prefix.size(), prefix) != 0 ||
            name.compare(name.size() - ext.size(), ext.size(), ext) != 0) {
            return false;
        }
        const std::string middle =
            name.substr(prefix.size(), name.size() - prefix.size() - ext.size());
        return parse_sequence_(middle, sequence);
    }

    // Resolve compressed and uncompressed names to the same logical source.
    // Trying the raw source form first is essential when the base filename
    // itself ends in .gz or .zst.
    static bool archive_source_name_(const std::string &name,
                                     const std::string &prefix,
                                     const std::string &ext,
                                     std::string &source_name,
                                     std::size_t &sequence) {
        if (is_archive_source_name_(name, prefix, ext, sequence)) {
            source_name = name;
            return true;
        }
        const char *suffixes[] = {".gz", ".zst"};
        for (const char *suffix : suffixes) {
            const std::string compression_suffix(suffix);
            if (name.size() <= compression_suffix.size() ||
                name.compare(name.size() - compression_suffix.size(), compression_suffix.size(),
                             compression_suffix) != 0) {
                continue;
            }
            const std::string source = name.substr(0, name.size() - compression_suffix.size());
            if (is_archive_source_name_(source, prefix, ext, sequence)) {
                source_name = source;
                return true;
            }
        }
        return false;
    }

    // A process killed during compression can leave .gz.tmp/.zst.tmp behind.
    // Associate such files with their logical source so normal retention will
    // eventually remove them; a live worker's pending source remains protected.
    static bool archive_temporary_source_name_(const std::string &name,
                                               const std::string &prefix,
                                               const std::string &ext,
                                               std::string &source_name,
                                               std::size_t &sequence) {
        const char *suffixes[] = {".gz.tmp", ".zst.tmp"};
        for (const char *suffix : suffixes) {
            const std::string temporary_suffix(suffix);
            if (name.size() <= temporary_suffix.size() ||
                name.compare(name.size() - temporary_suffix.size(), temporary_suffix.size(),
                             temporary_suffix) != 0) {
                continue;
            }
            const std::string source = name.substr(0, name.size() - temporary_suffix.size());
            if (is_archive_source_name_(source, prefix, ext, sequence)) {
                source_name = source;
                return true;
            }
        }
        return false;
    }

    void compress_worker_() noexcept {
        try {
            compress_worker_loop_();
        } catch (const std::exception &exception) {
            compression_error_count_.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                compress_stop_ = true;
                compress_in_progress_.clear();
                while (!compress_queue_.empty()) {
                    compress_queue_.pop();
                }
            }
            compress_cv_.notify_all();
            compress_done_cv_.notify_all();
            std::fprintf(stderr,
                         "[spdlog] rotating_compress_file_sink: worker stopped: %s\n",
                         exception.what());
        } catch (...) {
            compression_error_count_.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                compress_stop_ = true;
                compress_in_progress_.clear();
                while (!compress_queue_.empty()) {
                    compress_queue_.pop();
                }
            }
            compress_cv_.notify_all();
            compress_done_cv_.notify_all();
            std::fprintf(stderr,
                         "[spdlog] rotating_compress_file_sink: worker stopped\n");
        }
    }

    void compress_worker_loop_() {
        for (;;) {
            filename_t target;
            {
                std::unique_lock<std::mutex> lock(compress_mutex_);
                compress_cv_.wait(lock, [this] { return compress_stop_ || !compress_queue_.empty(); });
                if (compress_queue_.empty()) {
                    return;  // stop requested and queue drained
                }
                target = std::move(compress_queue_.front());
                compress_queue_.pop();
                compress_in_progress_ = target;
            }

            bool ok = false;
            std::string failure_reason;
            for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
                try {
                    const char *failure_stage = nullptr;
#ifdef SPDLOG_ROTATING_COMPRESS_ZLIB
                    if (compress_type_ == rotation_compress_type::zlib) {
                        ok = rotating_compress_details::gzip_compress_file(
                            target, compression_level_, &failure_stage);
                    }
#endif
#ifdef SPDLOG_ROTATING_COMPRESS_ZSTD
                    if (compress_type_ == rotation_compress_type::zstd) {
                        ok = rotating_compress_details::zstd_compress_file(
                            target, compression_level_, &failure_stage);
                    }
#endif
                    if (!ok) {
                        failure_reason = failure_stage == nullptr
                                             ? "compression returned failure"
                                             : failure_stage;
                    }
                } catch (const std::exception &exception) {
                    failure_reason = exception.what();
                } catch (...) {
                    failure_reason = "unknown compression exception";
                }
                if (!ok && attempt < 2) {
                    details::os::sleep_for_millis(10);
                }
            }

            {
                std::lock_guard<std::mutex> lock(compress_mutex_);
                compress_in_progress_.clear();
            }
            compress_done_cv_.notify_all();
            if (!ok) {
                compression_error_count_.fetch_add(1, std::memory_order_relaxed);
                std::fprintf(stderr,
                             "[spdlog] rotating_compress_file_sink: failed to compress %s: %s\n",
                             details::os::filename_to_str(target).c_str(), failure_reason.c_str());
            }
            // compression freed a pending slot: re-enforce max_files_ on the
            // settled archives (prune_old_files_ skips files still owned by
            // this worker, so the limit can only be enforced from here once
            // the queue drains)
            try {
                // Never take the base sink mutex here. maintenance_mutex_
                // already protects every state item used by pruning, and the
                // compression worker must remain independent of producers.
                prune_old_files_();
            } catch (const std::exception &exception) {
                std::fprintf(stderr,
                             "[spdlog] rotating_compress_file_sink: failed pruning archives: %s\n",
                             exception.what());
            } catch (...) {
                std::fprintf(stderr,
                             "[spdlog] rotating_compress_file_sink: failed pruning archives\n");
            }
        }
    }
>>>>>>> 9459495b80f6cdc1926b5669612df7259e7e7ed3

    filename_t base_filename_;
    std::size_t max_size_;
    std::size_t max_files_;
<<<<<<< HEAD
    std::size_t current_size_;
    details::file_helper file_helper_;
=======
    rotation_compress_type compress_type_;
    int compression_level_;
    std::size_t current_size_{0};
    std::size_t next_sequence_{1};
    details::file_helper file_helper_;

    std::atomic<std::size_t> compression_error_count_{0};
    std::atomic<std::size_t> maintenance_error_count_{0};
    std::atomic<std::size_t> deferred_rotation_count_{0};

    // Serializes retention work and max_files_ access against the internal
    // worker even when Mutex is details::null_mutex (_st variant).
    std::mutex maintenance_mutex_;
    std::thread compress_thread_;
    std::mutex compress_mutex_;
    std::condition_variable compress_cv_;
    std::condition_variable compress_done_cv_;
    std::queue<filename_t> compress_queue_;
    filename_t compress_in_progress_;
    std::set<filename_t> compress_reserved_;
    bool compress_stop_{false};
>>>>>>> 9459495b80f6cdc1926b5669612df7259e7e7ed3
};

using rotating_compress_file_sink_mt = rotating_compress_file_sink<std::mutex>;
using rotating_compress_file_sink_st = rotating_compress_file_sink<details::null_mutex>;

}  // namespace sinks

//
// factory functions
//
template <typename Factory = spdlog::synchronous_factory>
<<<<<<< HEAD
std::shared_ptr<logger> rotating_compress_logger_mt(const std::string &logger_name,
                                                      const filename_t &filename,
                                                      size_t max_file_size,
                                                      size_t max_files,
                                                      bool rotate_on_open = false,
                                                      const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::rotating_compress_file_sink_mt>(
        logger_name, filename, max_file_size, max_files, rotate_on_open, event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
std::shared_ptr<logger> rotating_compress_logger_st(const std::string &logger_name,
                                                      const filename_t &filename,
                                                      size_t max_file_size,
                                                      size_t max_files,
                                                      bool rotate_on_open = false,
                                                      const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::rotating_compress_file_sink_st>(
        logger_name, filename, max_file_size, max_files, rotate_on_open, event_handlers);
}

}  // namespace spdlog

#ifdef SPDLOG_HEADER_ONLY
#include "rotating_compress_file_sink-inl.h"
#endif
=======
std::shared_ptr<logger> rotating_compress_logger_mt(
    const std::string &logger_name,
    const filename_t &filename,
    size_t max_file_size,
    size_t max_files,
    sinks::rotation_compress_type compress_type = sinks::rotation_compress_type::none,
    int compression_level = -1,
    bool rotate_on_open = false,
    const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::rotating_compress_file_sink_mt>(
        logger_name, filename, max_file_size, max_files, compress_type, compression_level,
        rotate_on_open, event_handlers);
}

template <typename Factory = spdlog::synchronous_factory>
std::shared_ptr<logger> rotating_compress_logger_st(
    const std::string &logger_name,
    const filename_t &filename,
    size_t max_file_size,
    size_t max_files,
    sinks::rotation_compress_type compress_type = sinks::rotation_compress_type::none,
    int compression_level = -1,
    bool rotate_on_open = false,
    const file_event_handlers &event_handlers = {}) {
    return Factory::template create<sinks::rotating_compress_file_sink_st>(
        logger_name, filename, max_file_size, max_files, compress_type, compression_level,
        rotate_on_open, event_handlers);
}
}  // namespace spdlog
>>>>>>> 9459495b80f6cdc1926b5669612df7259e7e7ed3
