

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace byte_track {

// Measures a frame rate over two horizons: a rolling window meant to be logged
// periodically while running, and the whole-run average worth quoting after.
//
// The caller supplies the clock reading, so the meter is deterministic and does
// not care which clock it is driven from.
//
// The first frame only starts the clock; it is not counted. Over n+1 frames
// there are n intervals, so counting every frame and dividing by the elapsed
// time would overstate the rate -- badly so over short runs. Skipping the first
// makes frames/elapsed exact instead.
class RateMeter
{
public:
    void count(std::chrono::steady_clock::time_point now)
    {
        if (!started_) {
            started_ = true;
            start_ = window_start_ = now;
            return;
        }
        ++frames_;
        ++window_frames_;
        last_ = now;
    }

    bool started() const { return started_; }
    uint64_t frames() const { return frames_; }

    bool window_due(std::chrono::steady_clock::time_point now,
                    std::chrono::steady_clock::duration interval) const
    {
        return started_ && now - window_start_ >= interval;
    }

    // "12.30 fps (61 frames / 5.0s)", and opens a fresh window.
    std::string take_window(std::chrono::steady_clock::time_point now)
    {
        // Meters that share a log line do not necessarily start on the same
        // frame, so this can be asked for a window that never opened. Say so
        // rather than dividing by a garbage interval.
        if (!started_) return "no frames yet";
        std::string s = format(window_frames_, now - window_start_);
        window_frames_ = 0;
        window_start_ = now;
        return s;
    }

    // The whole-run average -- the number worth quoting. Spans first frame to
    // last frame, not to whenever this is called: measuring up to shutdown
    // would fold the idle gap after the final frame into the average and
    // understate the rate.
    std::string summary() const
    {
        if (!started_ || frames_ == 0) return "too few frames to measure a rate";
        return format(frames_, last_ - start_);
    }

private:
    static std::string format(uint64_t n, std::chrono::steady_clock::duration d)
    {
        const double s = std::chrono::duration_cast<std::chrono::duration<double>>(d).count();
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%.2f fps (%llu frames / %.1fs)",
                      s > 0.0 ? static_cast<double>(n) / s : 0.0,
                      static_cast<unsigned long long>(n), s);
        return buf;
    }

    bool started_ = false;
    std::chrono::steady_clock::time_point start_{};
    std::chrono::steady_clock::time_point last_{};
    std::chrono::steady_clock::time_point window_start_{};
    uint64_t frames_ = 0;
    uint64_t window_frames_ = 0;
};

// How long the per-frame work itself takes. Without this a frame rate is
// ambiguous: a steady 30 fps might mean the pipeline is saturated, or it might
// mean the camera is simply configured for 30 and there is headroom to spare.
// Mean vs. max separates the typical frame from the occasional slow one.
class LatencyMeter
{
public:
    void add(std::chrono::steady_clock::duration d)
    {
        total_ += d;
        window_total_ += d;
        if (d > max_) max_ = d;
        if (d > window_max_) window_max_ = d;
        ++n_;
        ++window_n_;
    }

    uint64_t samples() const { return n_; }

    // "mean 8.10 ms, max 24.30 ms", and opens a fresh window.
    std::string take_window()
    {
        std::string s = format(window_total_, window_max_, window_n_);
        window_total_ = std::chrono::steady_clock::duration::zero();
        window_max_ = std::chrono::steady_clock::duration::zero();
        window_n_ = 0;
        return s;
    }

    std::string summary() const { return format(total_, max_, n_); }

private:
    static std::string format(std::chrono::steady_clock::duration total,
                              std::chrono::steady_clock::duration max, uint64_t n)
    {
        if (n == 0) return "no frames";
        char buf[64];
        std::snprintf(buf, sizeof(buf), "mean %.2f ms, max %.2f ms",
                      ms(total) / static_cast<double>(n), ms(max));
        return buf;
    }
    static double ms(std::chrono::steady_clock::duration d)
    {
        return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(d).count();
    }

    std::chrono::steady_clock::duration total_ = std::chrono::steady_clock::duration::zero();
    std::chrono::steady_clock::duration max_ = std::chrono::steady_clock::duration::zero();
    std::chrono::steady_clock::duration window_total_ = std::chrono::steady_clock::duration::zero();
    std::chrono::steady_clock::duration window_max_ = std::chrono::steady_clock::duration::zero();
    uint64_t n_ = 0;
    uint64_t window_n_ = 0;
};

// Times a scope and hands the duration to a LatencyMeter on the way out, so a
// function with several early returns is measured on every path.
class ScopedLatency
{
public:
    explicit ScopedLatency(LatencyMeter &meter)
        : meter_(meter), start_(std::chrono::steady_clock::now()) {}
    ~ScopedLatency() { meter_.add(std::chrono::steady_clock::now() - start_); }
    ScopedLatency(const ScopedLatency &) = delete;
    ScopedLatency &operator=(const ScopedLatency &) = delete;

private:
    LatencyMeter &meter_;
    std::chrono::steady_clock::time_point start_;
};

}  // namespace byte_track
