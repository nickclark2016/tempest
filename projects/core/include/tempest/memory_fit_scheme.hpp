#ifndef tempest_memory_fit_scheme_hpp
#define tempest_memory_fit_scheme_hpp

#include <tempest/int.hpp>
#include <tempest/memory.hpp>
#include <tempest/vector.hpp>

#include <optional>

namespace tempest::core
{
    template <typename T>
    class best_fit_scheme
    {
      public:
        explicit best_fit_scheme(const T initial_range);
        [[nodiscard]] std::optional<range<T>> allocate(const T& len);
        void release(range<T>&& rng);
        void release_all();
        void extend(const T& new_length);
        [[nodiscard]] T min_extent() const noexcept;
        [[nodiscard]] T max_extent() const noexcept;

      private:
        range<T> _full;
        vector<range<T>> _free;
    };

    template <typename T>
    inline best_fit_scheme<T>::best_fit_scheme(const T initial_range)
        : _full{range<T>{
              .start{0},
              .end{initial_range},
          }}
    {
        _free.push_back(_full);
    }

    template <typename T>
    inline std::optional<range<T>> best_fit_scheme<T>::allocate(const T& len)
    {
        struct fit
        {
            size_t index;
            range<T> range;
        };

        std::optional<fit> optimal_block = std::nullopt;
        T best_fit = len - len;

        for (size_t i = 0; i < _free.size(); ++i)
        {
            range<T> rng = _free[i];

            T range_size = rng.end - rng.start;
            best_fit += range_size;

            if (range_size < len)
            {
                continue;
            }
            else if (range_size == len)
            {
                optimal_block = fit{
                    .index{i},
                    .range{rng},
                };
                break;
            }
            else
            {
                optimal_block = ([&]() -> std::optional<fit> {
                    if (optimal_block)
                    {
                        if (range_size < (optimal_block->range.end - optimal_block->range.start))
                        {
                            return fit{
                                .index{i},
                                .range{rng},
                            };
                        }
                        else
                        {
                            return optimal_block;
                        }
                    }
                    else
                    {
                        return fit{
                            .index{i},
                            .range{rng},
                        };
                    }
                })();
            }
        }

        if (optimal_block)
        {
            auto& [index, rng] = *optimal_block;
            if (rng.end - rng.start == len)
            {
                _free.erase(_free.begin() + index);
            }
            else
            {
                _free[index].start += len;
            }
            return range<T>{
                .start{rng.start},
                .end{rng.start + len},
            };
        }

        return std::nullopt;
    }

    template <typename T>
    inline void best_fit_scheme<T>::release(range<T>&& rng)
    {
        auto free_iter = std::find_if(_free.begin(), _free.end(), [&rng](range<T>& r) { return r.start > rng.start; });
        const size_t idx = std::distance(_free.begin(), free_iter);

        if (idx > 0 && rng.start == _free[idx - 1].end)
        {
            // coalesce left
            _free[idx - 1].end = ([&]() {
                if (idx < _free.size() && rng.end == _free[idx].start)
                {
                    auto end = _free[idx].end;
                    _free.erase(_free.begin() + idx);
                    return end;
                }
                else
                {
                    return rng.end;
                }
            })();
            return;
        }
        else if (idx < _free.size() && rng.end == _free[idx].start)
        {
            _free[idx].start = ([&]() {
                if (idx > 0 && rng.start == _free[idx - 1].end)
                {
                    auto start = _free[idx].start;
                    _free.erase(_free.begin() + idx);
                    return start;
                }
                else
                {
                    return rng.start;
                }
            })();

            return;
        }

        _free.insert(_free.begin() + idx, rng);
    }

    template <typename T>
    inline void best_fit_scheme<T>::release_all()
    {
        _free.clear();
        _free.push_back(_full);
    }

    template <typename T>
    inline void best_fit_scheme<T>::extend(const T& new_length)
    {
        if (!_free.empty())
        {
            _free.back().end = new_length;
        }
        else
        {
            _free.push_back(range<T>{
                .start{_full.end},
                .end{new_length},
            });
        }
    }

    template <typename T>
    inline T best_fit_scheme<T>::min_extent() const noexcept
    {
        return _full.start;
    }

    template <typename T>
    inline T best_fit_scheme<T>::max_extent() const noexcept
    {
        return _full.end;
    }
} // namespace tempest::core

#endif