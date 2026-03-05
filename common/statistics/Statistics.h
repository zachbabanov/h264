/*
* @license
* (C) zachbabanov
*
*/

#pragma once

#include <log/Logger.h>

#include <condition_variable>
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <deque>

namespace server {
    class Statistics {
    public:
        Statistics(const Statistics&) = delete;
        Statistics& operator=(const Statistics&) = delete;
        Statistics(Statistics&&) = delete;
        Statistics& operator=(Statistics&&) = delete;

        static Statistics& Instance() {
            static Statistics statisticsInstance;
            return statisticsInstance;
        }

        void SetFilePath(const std::string &path) {
            std::ofstream newFile(path, std::ios::trunc);

            if (!newFile.is_open()) {
                Logger::Instance().Warn(fmt::format("Statistics file cannot be opened/created: {}", path));
                exit(1);
            }

            {
                std::unique_lock lock(_mutex);
                _file.swap(newFile);
                _filePath = path;
            }
        }

        void Start() {
            std::unique_lock lock(_mutex);

            if (_started.load(std::memory_order_acquire)) {
                Logger::Instance().Warn("Statistics are already started");
                return;
            }

            if (!_file.is_open()) {
                _file.open(_filePath, std::ios::trunc);

                if (!_file.is_open()) {
                    Logger::Instance().Error(fmt::format("Statistics file cannot be opened/created: {}", _filePath));
                    exit(1);
                }
            }

            if (_counters.empty()) {
                Logger::Instance().Warn("Statistics counters list is empty, nothing to count and write");
                return;
            }

            _started.store(true, std::memory_order_release);

            std::vector<std::string_view> counterNames;
            counterNames.reserve(_counters.size());
            std::transform(_counters.begin(), _counters.end(), std::back_inserter(counterNames),
                           [](const auto& pair) { return pair.first; });

            try {
                _file << fmt::format("Timestamp,{}", fmt::join(counterNames, ",")) << std::endl;
            } catch (const std::exception& e) {
                Logger::Instance().Error(fmt::format("Cannot write statistics file header by path: {}", _filePath));
                _started.store(false, std::memory_order_release);
                Stop();
                return;
            }

            _writeThread = std::jthread(&Statistics::WriteLoop, this);
        }

        void RegisterCounter(const std::string &name) {
            std::unique_lock lock(_mutex);

            if (_started) {
                Logger::Instance().Warn("Cannot register counter after statistics write start");
                return;
            }

            if (_nameToIndex.find(name) != _nameToIndex.end()) {
                return;
            }

            _nameToIndex[name] = _counters.size();
            _counters.emplace_back(std::piecewise_construct, std::forward_as_tuple(name), std::forward_as_tuple(0));
        }

        void RegisterCounters(const std::vector<std::string> &countersNames) {
            if (_started) {
                Logger::Instance().Warn("Cannot register counter after statistics write start");
                return;
            }

            for (const auto &name : countersNames) {
                RegisterCounter(name);
            }
        }

        void Increment(const std::string_view& name, int64_t valueDelta = 1) {
            std::shared_lock lock(_mutex);

            auto it = _nameToIndex.find(name);

            if (it == _nameToIndex.end()) {
                return;
            }

            _counters[it->second].second.fetch_add(valueDelta, std::memory_order_relaxed);
        }

        void Set(const std::string_view& name, int64_t value) {
            std::shared_lock lock(_mutex);

            auto it = _nameToIndex.find(name);

            if (it == _nameToIndex.end()) {
                return;
            }

            _counters[it->second].second.store(value, std::memory_order_relaxed);
        }

        int64_t Get(const std::string_view& name) const {
            std::shared_lock lock(_mutex);

            auto it = _nameToIndex.find(name);

            if (it == _nameToIndex.end()) {
                return 0;
            }

            return _counters[it->second].second.load(std::memory_order_relaxed);
        }

    private:
        Statistics() = default;

        ~Statistics() {
            Stop();
        }

        void Stop() {
            std::unique_lock lock(_mutex);

            if (_writeThread.joinable()) {
                _writeThread.request_stop();
                _writeThread.join();
            }

            if (_file.is_open()) {
                _file.close();
            }
        }

        void WriteLoop(const std::stop_token &stopToken) {
            while (!stopToken.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                if (!stopToken.stop_requested()) {
                    writeStatisticsLine();
                }
            }
        }

        void writeStatisticsLine() {
            std::shared_lock lock(_mutex);

            if (!_file.is_open()) {
                Logger::Instance().Warn("Trying to write statistics line into closed file");
                return;
            }

            std::vector<std::int64_t> counterValues;
            counterValues.reserve(_counters.size());
            std::transform(_counters.begin(), _counters.end(), std::back_inserter(counterValues),
                           [](const auto& pair) { return pair.second.load(std::memory_order_relaxed); });

            try {
                _file << fmt::format("{:%Y-%m-%d %H:%M:%S},{}", std::chrono::system_clock::now(), fmt::join(counterValues, ",")) << std::endl;
            } catch (const std::exception& e) {
                Logger::Instance().Error(fmt::format("Cannot write statistics entry by path: {}, error: {}", _filePath, e.what()));
            }
        }

        std::atomic<bool> _started{false};
        mutable std::shared_mutex _mutex;
        std::jthread _writeThread;

        std::deque<std::pair<std::string_view, std::atomic<int64_t>>> _counters;
        std::unordered_map<std::string_view, size_t> _nameToIndex;

        std::string _filePath{config::fields::DEFAULT_STAT_PATH};
        std::ofstream _file;
    };
}
