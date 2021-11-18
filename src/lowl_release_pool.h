#ifndef LOWL_RELEASE_POOL_H
#define LOWL_RELEASE_POOL_H

#include "lowl_typedef.h"
#include "lowl_timer.h"
#include "lowl_logger.h"

#include <mutex>
#include <vector>
#include <memory>
#include <unordered_set>
#include <algorithm>

namespace Lowl {

    class ReleasePool {

    private:
        std::unique_ptr<Timer> timer;
        std::vector<std::shared_ptr<void>> pool;
        std::mutex mutex;

        void release_callback() {
            std::lock_guard<std::mutex> lock(mutex);
            pool.erase(
                    std::remove_if(
                            pool.begin(), pool.end(),
                            [](std::shared_ptr<void> &object) {
                                return object.use_count() <= 1;
                            }
                    ),
                    pool.end()
            );
        }

    public:
        template<typename T>
        void add(const std::shared_ptr<T> &object) {
            if (object == nullptr) {
                return;
            }
            std::lock_guard<std::mutex> lock(mutex);
            if (std::find(pool.begin(), pool.end(), object) != pool.end()) {
                return;
            }
            pool.template emplace_back<>(object);
        }

        ReleasePool() {
            timer = std::make_unique<Timer>();
            pool = std::vector<std::shared_ptr<void>>();
            timer->start_interval(std::bind(&ReleasePool::release_callback, this), std::chrono::seconds(10));
        }

        ~ReleasePool() {}
    };
}

#endif