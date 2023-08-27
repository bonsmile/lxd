#include "timer.h"
#include <fmt/chrono.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iomanip>
#else
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#endif

namespace lxd {
    void sleep(int milliseconds) {
#ifdef _WIN32
        Sleep(milliseconds);
#else
        usleep(milliseconds);
#endif
    }

    uint64_t nanosecond() {
#ifdef _WIN32
        static uint64_t ticksPerSecond = 0;
        static uint64_t timeBase = 0;

        if(ticksPerSecond == 0) {
            LARGE_INTEGER li;
            QueryPerformanceFrequency(&li);
            ticksPerSecond = (uint64_t)li.QuadPart;
            QueryPerformanceCounter(&li);
            timeBase = (uint64_t)li.LowPart + 0xFFFFFFFFULL * li.HighPart;
        }

        LARGE_INTEGER li;
        QueryPerformanceCounter(&li);
        uint64_t counter = (uint64_t)li.LowPart + 0xFFFFFFFFULL * li.HighPart;
        return (counter - timeBase) * 1000ULL * 1000ULL * 1000ULL / ticksPerSecond;
#else
        static uint64_t timeBase = 0;

        struct timeval tv;
        gettimeofday(&tv, 0);

        if(timeBase == 0)
        {
            timeBase = (uint64_t)tv.tv_sec * 1000ULL * 1000ULL * 1000ULL + tv.tv_usec * 1000ULL;
        }

        return (uint64_t)tv.tv_sec * 1000ULL * 1000ULL * 1000ULL + tv.tv_usec * 1000ULL - timeBase;
#endif
	}

    float millisecond() {
        return float(nanosecond()) * 0.000001f;
    }

    double second() {
        return double(nanosecond()) * 0.000000001;
    }

    const std::string date(const DateFormat fmt) {
        time_t now;
        time(&now);

        tm local;
#ifdef _MSC_VER
        localtime_s(&local, &now);
#else
        localtime_r(&now, &local);
#endif

        if(fmt == Default)
            return fmt::format("{:%Y%m%d%H%M%S}", local);
        else
            return fmt::format("{:%Y-%m-%d %H:%M:%S}", local);
    }

#ifdef _WIN32
    // https://stackoverflow.com/a/33542189
    char* strptime(const char* s, const char* f, struct tm* tm) {
        // Isn't the C++ standard lib nice? std::get_time is defined such that its
        // format parameters are the exact same as strptime. Of course, we have to
        // create a string stream first, and imbue it with the current C locale, and
        // we also have to make sure we return the right things if it fails, or
        // if it succeeds, but this is still far simpler an implementation than any
        // of the versions in any of the C standard libraries.
        std::istringstream input(s);
        //input.imbue(std::locale(setlocale(LC_ALL, nullptr)));
        input >> std::get_time(tm, f);
        if (input.fail()) {
            return nullptr;
        }
        return (char*)(s + input.tellg());
    }
#endif
}