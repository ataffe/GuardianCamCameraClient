

//
// Unit tests for the frame-rate meters. The whole point of these classes is to
// produce a number that gets reported, so the arithmetic is what is tested:
//   - the first frame starts the clock and is not counted (n frames, n-1
//     intervals), which is where a naive counter overstates the rate,
//   - rolling windows reset and do not leak into the whole-run average,
//   - a meter that has never seen a frame says so instead of dividing by a
//     garbage interval,
//   - latency mean/max accumulate over the right sample set.

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "perf/frame_rate.h"

using byte_track::LatencyMeter;
using byte_track::RateMeter;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace {

// A fixed origin so every test drives the meters from the same synthetic clock.
const steady_clock::time_point kT0{};

steady_clock::time_point at_ms(int64_t ms) { return kT0 + milliseconds(ms); }

// Feeds `count` frames spaced `period_ms` apart, starting at `start_ms`.
// Returns the timestamp of the last frame fed.
steady_clock::time_point feed(RateMeter &m, int count, int64_t start_ms, int64_t period_ms)
{
    steady_clock::time_point last = at_ms(start_ms);
    for (int i = 0; i < count; ++i) {
        last = at_ms(start_ms + i * period_ms);
        m.count(last);
    }
    return last;
}

}  // namespace

TEST(RateMeterTest, ReportsNothingBeforeAnyFrame)
{
    RateMeter m;
    EXPECT_FALSE(m.started());
    EXPECT_EQ(m.frames(), 0u);
    EXPECT_EQ(m.summary(), "too few frames to measure a rate");
}

TEST(RateMeterTest, FirstFrameOnlyStartsTheClock)
{
    RateMeter m;
    m.count(kT0);
    EXPECT_TRUE(m.started());
    // One frame gives no interval to measure over, so there is still no rate.
    EXPECT_EQ(m.frames(), 0u);
    EXPECT_EQ(m.summary(), "too few frames to measure a rate");
}

TEST(RateMeterTest, ExactRateForEvenlySpacedFrames)
{
    RateMeter m;
    // 31 frames at 33.333ms apart => 30 intervals spanning exactly 1s => 30 fps.
    // Counting all 31 over 1s would report 31 fps; the first frame is excluded
    // precisely so this comes out at 30.
    steady_clock::time_point last = kT0;
    for (int i = 0; i <= 30; ++i) {
        last = kT0 + std::chrono::nanoseconds(i * 1000000000LL / 30);
        m.count(last);
    }
    EXPECT_EQ(m.frames(), 30u);
    EXPECT_EQ(m.summary(), "30.00 fps (30 frames / 1.0s)");
}

TEST(RateMeterTest, WindowIsDueOnlyAfterTheIntervalElapses)
{
    RateMeter m;
    m.count(kT0);
    EXPECT_FALSE(m.window_due(at_ms(4999), seconds(5)));
    EXPECT_TRUE(m.window_due(at_ms(5000), seconds(5)));
}

TEST(RateMeterTest, WindowNeverDueBeforeTheFirstFrame)
{
    RateMeter m;
    // Guards against the default-constructed window_start_ (the clock epoch)
    // making every window look overdue by decades.
    EXPECT_FALSE(m.window_due(at_ms(100000), seconds(5)));
    EXPECT_EQ(m.take_window(at_ms(100000)), "no frames yet");
}

TEST(RateMeterTest, TakingAWindowResetsItWithoutDisturbingTheRunAverage)
{
    RateMeter m;
    // Clock starts at t=0, then 50 frames over the next 5s => 10 fps.
    m.count(kT0);
    feed(m, 50, /*start_ms=*/100, /*period_ms=*/100);
    EXPECT_EQ(m.take_window(at_ms(5000)), "10.00 fps (50 frames / 5.0s)");

    // A second window of 25 frames over the following 5s => 5 fps, and the
    // first window's frames must not be counted again.
    feed(m, 25, /*start_ms=*/5200, /*period_ms=*/200);
    EXPECT_EQ(m.take_window(at_ms(10000)), "5.00 fps (25 frames / 5.0s)");

    // The run average still spans everything: 75 frames from the first frame
    // (t=0) to the last (t=10s), regardless of when the summary is asked for.
    EXPECT_EQ(m.frames(), 75u);
    EXPECT_EQ(m.summary(), "7.50 fps (75 frames / 10.0s)");
}

TEST(RateMeterTest, RunAverageIgnoresIdleTimeAfterTheLastFrame)
{
    RateMeter m;
    m.count(kT0);
    feed(m, 20, /*start_ms=*/100, /*period_ms=*/100);
    // Last frame lands at t=2.0s. The camera then sits idle for a minute before
    // shutdown; the average must still be 20 frames over 2s, not over 62s.
    EXPECT_EQ(m.summary(), "10.00 fps (20 frames / 2.0s)");
}

TEST(RateMeterTest, WindowWithNoFramesReportsZeroRatherThanStale)
{
    RateMeter m;
    m.count(kT0);
    feed(m, 10, /*start_ms=*/100, /*period_ms=*/100);
    EXPECT_EQ(m.take_window(at_ms(5000)), "2.00 fps (10 frames / 5.0s)");
    // Camera stalled: no frames at all in the next window.
    EXPECT_EQ(m.take_window(at_ms(10000)), "0.00 fps (0 frames / 5.0s)");
}

TEST(LatencyMeterTest, ReportsNothingBeforeAnySample)
{
    LatencyMeter m;
    EXPECT_EQ(m.samples(), 0u);
    EXPECT_EQ(m.summary(), "no frames");
}

TEST(LatencyMeterTest, MeanAndMaxOverAllSamples)
{
    LatencyMeter m;
    m.add(milliseconds(10));
    m.add(milliseconds(20));
    m.add(milliseconds(30));
    EXPECT_EQ(m.samples(), 3u);
    EXPECT_EQ(m.summary(), "mean 20.00 ms, max 30.00 ms");
}

TEST(LatencyMeterTest, WindowResetsButTheRunTotalDoesNot)
{
    LatencyMeter m;
    m.add(milliseconds(10));
    m.add(milliseconds(50));
    EXPECT_EQ(m.take_window(), "mean 30.00 ms, max 50.00 ms");

    // Fresh window: the earlier 50ms spike must not carry over as the max.
    m.add(milliseconds(4));
    m.add(milliseconds(6));
    EXPECT_EQ(m.take_window(), "mean 5.00 ms, max 6.00 ms");

    // ...but it is still the max over the whole run.
    EXPECT_EQ(m.summary(), "mean 17.50 ms, max 50.00 ms");
}

TEST(LatencyMeterTest, EmptyWindowReportsNoFrames)
{
    LatencyMeter m;
    m.add(milliseconds(10));
    EXPECT_EQ(m.take_window(), "mean 10.00 ms, max 10.00 ms");
    EXPECT_EQ(m.take_window(), "no frames");
    // The run summary is unaffected by the empty window.
    EXPECT_EQ(m.summary(), "mean 10.00 ms, max 10.00 ms");
}

TEST(ScopedLatencyTest, RecordsOnEveryPathOutOfAScope)
{
    LatencyMeter m;
    // Including the early-return path, which is why the callback uses an RAII
    // timer rather than a measurement at the end of the function.
    const auto body = [&m](bool bail) {
        byte_track::ScopedLatency t(m);
        if (bail) return;
    };
    body(true);
    body(false);
    EXPECT_EQ(m.samples(), 2u);
}
