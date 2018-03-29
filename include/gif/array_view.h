/**
 * \file    array_view.h
 *
 * \author  Magnus Leksell
 */
#ifndef ARRAY_VIEW_H
#define ARRAY_VIEW_H

#include <cstring>
#include <vector>
#include <iostream>
#include <assert.h>
#include <stdexcept>

/**
 * \class   array_view
 */
template<class T>
class array_view
{
public:
    typedef T   value_type;
    typedef T*  pointer;
    typedef T&  reference;

    /**
     * \brief   Default constructor
     */
    array_view() :
        data_(nullptr),
        size_(0)
    {
        // empty
    }

    const T* data() const noexcept {
        return data_;
    }

    size_t size() const noexcept {
        return size_;
    }

    /**
     * \brief   Constructor
     */
    array_view(const T* data, size_t size) :
        data_(data),
        size_(size)
    {
        // empty
    }

    array_view(const array_view& rhs) :
        data_(rhs.data_),
        size_(rhs.size_)
    {
        // empty
    }

    array_view(const std::vector<T>& data) :
        data_(data.data()),
        size_(data.size())
    {
        // empty
    }

    array_view(const std::string& data) :
        data_(reinterpret_cast<const uint8_t*>(data.c_str())),
        size_(data.length())
    {
        // empty
    }

    /**
     * \class   reverse_iterator
     */
    class reverse_iterator :
        public std::iterator<std::forward_iterator_tag, T>
    {
    public:
        reverse_iterator(const array_view<T>* view, int64_t offset) noexcept :
            offset_(offset)
        {
            view_ = view;
        }

        reverse_iterator() :
            view_(nullptr),
            offset_(-1)
        {

        }

        const T& operator*() const noexcept {
            assert(view_);
            assert(offset_ >= 0);
            return view_->data_[offset_];
        }

        reverse_iterator& operator++() noexcept
        {
            --offset_;
            return *this;
        }

        reverse_iterator operator++(int) noexcept
        {
            reverse_iterator clone(*this);
            --offset_;
            return clone;
        }

        bool operator==(const reverse_iterator& rhs) const noexcept
        {
            return (view_ == rhs.view_) && (offset_ == rhs.offset_);
        }

        bool operator!=(const reverse_iterator& rhs) const noexcept
        {
            return !(*this == rhs);
        }

        reverse_iterator operator+(size_t count) const noexcept
        {
            if (count > offset_) {
                return reverse_iterator(view_, -1);
            } else {
                return reverse_iterator(view_, offset_ - count);
            }
        }

        bool operator!() const noexcept
        {
            return (view_ == nullptr) && (offset_ == 0);
        }

        explicit operator bool() const noexcept
        {
            return !!*this;
        }

        int64_t offset() const            { return offset_; }

        const array_view<T>* view() const { return view_; }

    private:
        const array_view<T>* view_;
        int64_t offset_;
    };

    /**
     * \class   iterator
     */
    class const_iterator :
        public std::iterator<std::forward_iterator_tag, T>
    {
    public:
        explicit const_iterator(const array_view<T>* view, size_t offset = 0) noexcept :
            offset_(offset),
            view_(view)
        {
        }

        explicit const_iterator(const array_view<T>& view, size_t offset = 0) noexcept :
            offset_(offset),
            view_(&view)
        {
        }

        explicit const_iterator() :
            view_(nullptr),
            offset_(0)
        {

        }

        explicit const_iterator(const array_view<T>::reverse_iterator& iter) :
            view_(iter.view()),
            offset_(0)
        {
            if (view_) {
                auto pos = iter.offset();
                if ((pos < 0) || (pos > view_->size())) {
                    throw std::runtime_error(
                        "reverse iterator can't be converted to a const_iterator.");
                }
                offset_ = static_cast<size_t>(pos);
            }
        }

        const T& operator*() const noexcept {
            assert(view_);
            return view_->data_[offset_];
        }

        const_iterator& operator++() noexcept
        {
            ++offset_;
            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator clone(*this);
            ++offset_;
            return clone;
        }

        bool operator==(const const_iterator& rhs) const noexcept
        {
            return (view_ == rhs.view_) && (offset_ == rhs.offset_);
        }

        bool operator!=(const const_iterator rhs) const noexcept
        {
          return !(*this == rhs);
        }

        const_iterator operator+(size_t count) const
        {
            if (view_) {
                if (count > (view_->size() - offset_)) {
                    throw std::out_of_range("Index out of range.");
                }
            }
            return const_iterator(view_, offset_ + count);
        }

        bool operator!() const noexcept
        {
            return (view_ == nullptr) && (offset_ == 0);
        }

        size_t offset() const             { return offset_; }

        const array_view<T>* view() const { return view_; }

    private:
        const array_view<T>* view_;
        size_t offset_;
    };

    array_view(array_view<T>::const_iterator&& begin_iter, array_view<T>::const_iterator&& end_iter) :
        data_(nullptr),
        size_(0)
    {
        const auto& data_view = begin_iter.view();
        if (data_view) {
            data_ = data_view->data_ + begin_iter.offset();
            size_ = end_iter.offset() - begin_iter.offset();
        }
    }


    array_view(const array_view<T>::const_iterator& begin_iter, const array_view<T>::const_iterator& end_iter) :
        data_(nullptr),
        size_(0)
    {
        const auto& data_view = begin_iter.view();
        if (data_view) {
            data_ = data_view->data_ + begin_iter.offset();
            size_ = end_iter.offset() - begin_iter.offset();
        }
    }

    bool empty() const noexcept {
        return (size_ == 0);
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    const_iterator begin() const noexcept {
        return const_iterator(this, 0);
    }
    
    const_iterator end() const noexcept {
        return const_iterator(this, size_);
    }

    reverse_iterator rbegin() const noexcept {
        return reverse_iterator(this, static_cast<int64_t>(size_) - 1);
    }
    
    reverse_iterator rend() const noexcept {
        return reverse_iterator(this, -1);
    }

    friend class const_iterator;

private:
    const T*    data_;
    size_t      size_;
};

#endif // ARRAY_VIEW_H
