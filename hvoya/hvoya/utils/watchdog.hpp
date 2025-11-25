#pragma once

#include <atomic>             
#include <thread>             
#include <mutex>              
#include <condition_variable> 
#include <chrono>  


namespace hvoya {
 

    template <typename T>
    class Watchdog {
            
        private:
            T* _owner;
            bool (T::*_updateMethod)();
            
            std::atomic <bool> _needsUpdate { false };
            std::atomic <bool> _running     { false };
			std::atomic <bool> _shouldStop  { false };
			std::atomic <bool> _paused		{ false };
			// as of Nov 2025 fucking apple clang doesnt support jthread
            std::thread _thread;
            std::mutex _mutex;
            std::condition_variable _cv;
            std::chrono::milliseconds _retryDelay;

            void loop() {
                while (!_shouldStop.load (std::memory_order_acquire)) {
                    {
                        std::unique_lock <std::mutex> lock (_mutex);
                        _cv.wait (lock, [this] {
                            return (_needsUpdate.load (std::memory_order_acquire)
									&& !_paused.load (std::memory_order_acquire))
								|| _shouldStop.load (std::memory_order_acquire);
                        });
                    }
                    
                    if (_shouldStop.load (std::memory_order_acquire))
                        break;

					if (_paused.load (std::memory_order_acquire))
						continue;

					if ((_owner->*_updateMethod)()) {
						_needsUpdate.store (false, std::memory_order_release);
						if (interruptibleSleep (std::chrono::minutes (1)))
							break;
					}
                    else
						if (interruptibleSleep (_retryDelay))
							break;
                }
            }


			bool interruptibleSleep (std::chrono::milliseconds duration) {
				std::unique_lock <std::mutex> lock(_mutex);
				return _cv.wait_for (lock, duration, [this] {
					return _shouldStop.load (std::memory_order_acquire);
				});
			}

        public:
        
            Watchdog (T* owner, bool (T::*method)(), 
                    std::chrono::milliseconds retryDelay = std::chrono::milliseconds (1000))
                : _owner (owner)
                , _updateMethod (method)
                , _retryDelay (retryDelay)
            {}
            
			~Watchdog() {
				stop();
			}

            bool start() {
                bool expected = false;
                if (_running.compare_exchange_strong (expected, true)) {
                    _thread = std::thread (&Watchdog::loop, this);
                    return true;
                }
                return false;
            }

			void pause() {
				_paused.store (true, std::memory_order_release);
			}

			void resume() {
				_paused.store (false, std::memory_order_release);
				_cv.notify_one();
			}

			bool isPaused() const {
				return _paused.load (std::memory_order_acquire);
			}

            void notify() {
                _needsUpdate.store (true, std::memory_order_release);
                _cv.notify_one();
            }
            
            void stop() {
                if (_running.load()) {
					_shouldStop.store (true, std::memory_order_release);
                    _cv.notify_one();
					if (_thread.joinable())
						_thread.join();
                    _running.store (false);
                }
            }
            
            bool isRunning() const {
                return _running.load (std::memory_order_acquire);
            }
            
            Watchdog (const Watchdog&) = delete;
            Watchdog& operator= (const Watchdog&) = delete;
    };
        
        
}
