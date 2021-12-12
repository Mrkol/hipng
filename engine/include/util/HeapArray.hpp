#pragma once


template<typename T>
class HeapArray
{
public:
    HeapArray() : HeapArray(0) {}

    explicit HeapArray(std::size_t capacity) : capacity_{capacity}
    {
        if (capacity > 0)
        {
			start_ = reinterpret_cast<T*>(new std::byte[capacity * sizeof(T)]);
        }
    }

    HeapArray(const HeapArray&) = delete;

    HeapArray(HeapArray&& other)
        : start_{std::exchange(other.start_, nullptr)}
        , capacity_{std::exchange(other.capacity_, 0)}
        , size_{std::exchange(other.size_, 0)}
    {

    }

    HeapArray& operator=(const HeapArray&) = delete;

    HeapArray& operator=(HeapArray&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }
        this->~HeapArray();
        start_ = std::exchange(other.start_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
        size_ = std::exchange(other.size_, 0);

        return *this;
    }

    T& operator[](std::size_t i)
    {
        NG_ASSERT(i < size_);
        return start_[i];
    }

    const T& operator[](std::size_t i) const
    {
        NG_ASSERT(i < size_);
        return start_[i];
    }

    template<class... Args>
    T& emplace_back(Args&&... args)
    {
        NG_ASSERT(size_ < capacity_);
        ++size_;
        return *new (start_ + size_ - 1) T((Args&&) args...);
    }

    std::size_t size() const { return size_; }

    void pop_back()
    {
        NG_ASSERT(size_ > 0);
        --size_;
        delete (start_ + size_);
    }

    T& back()
    {
        NG_ASSERT(size_ > 0);
        return start_[size_ - 1];
    }

    const T& back() const
    {
        NG_ASSERT(size_ > 0);
        return start_[size_ - 1];
    }

    T& front()
    {
        NG_ASSERT(size_ > 0);
        return start_[0];
    }

    const T& front() const
    {
        NG_ASSERT(size_ > 0);
        return start_[0];
    }

    ~HeapArray() noexcept
    {
        for (std::size_t i = 0; i < size_; ++i)
        {
            (start_ + i)->~T();
        }
        delete[] reinterpret_cast<std::byte*>(start_);
    }
    
    auto begin() { return start_; }
    auto end() { return start_ + size_; }
    auto cbegin() const { return start_; }
    auto cend() const { return start_ + size_; }

private:
    T* start_{nullptr};
    std::size_t capacity_;
    std::size_t size_{0};
};

