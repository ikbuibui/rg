#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <vector>

namespace rg
{
    template<typename T>
    concept Semiregular = std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>
                          && std::is_copy_assignable_v<T> && std::is_destructible_v<T>;

    template<Semiregular T, std::integral N = std::size_t>
    class ListPool
    {
    public:
        using list_type = N;
        using value_type = T;

    private:
        struct node_t
        {
            T value{};
            list_type next{};
        };

        std::vector<node_t> pool;
        list_type free_list = end();

        node_t& node(list_type x)
        {
            return pool[x - 1];
        }

        node_t const& node(list_type x) const
        {
            return pool[x - 1];
        }

        list_type new_list()
        {
            pool.emplace_back();
            return static_cast<list_type>(pool.size());
        }

    public:
        using size_type = typename std::vector<node_t>::size_type;

        constexpr list_type end() const noexcept
        {
            return list_type(0);
        }

        [[nodiscard]] bool is_end(list_type x) const noexcept
        {
            return x == end();
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return pool.empty();
        }

        [[nodiscard]] size_type size() const noexcept
        {
            return pool.size();
        }

        [[nodiscard]] size_type capacity() const noexcept
        {
            return pool.capacity();
        }

        void reserve(size_type n)
        {
            pool.reserve(n);
        }

        ListPool() = default;

        explicit ListPool(size_type n)
        {
            reserve(n);
        }

        T& value(list_type x)
        {
            return node(x).value;
        }

        T const& value(list_type x) const
        {
            return node(x).value;
        }

        list_type& next(list_type x)
        {
            return node(x).next;
        }

        list_type const& next(list_type x) const
        {
            return node(x).next;
        }

        list_type free(list_type x)
        {
            list_type tail = next(x);
            next(x) = free_list;
            free_list = x;
            return tail;
        }

        list_type free(list_type front, list_type back)
        {
            if(is_end(front))
                return end();
            list_type tail = next(back);
            next(back) = free_list;
            free_list = front;
            return tail;
        }

        list_type allocate(T const& val, list_type tail)
        {
            list_type list = free_list;
            if(is_end(free_list))
            {
                list = new_list();
            }
            else
            {
                free_list = next(free_list);
            }
            value(list) = val;
            next(list) = tail;
            return list;
        }

        struct iterator
        {
            using value_type = T;
            using difference_type = list_type;
            using iterator_category = std::forward_iterator_tag;
            using reference = value_type&;
            using pointer = value_type*;

            ListPool* pool = nullptr;
            list_type node = 0;

            iterator() = default;

            iterator(ListPool& p, list_type n) : pool(&p), node(n)
            {
            }

            explicit iterator(ListPool& p) : pool(&p), node(p.end())
            {
            }

            reference operator*() const
            {
                return pool->value(node);
            }

            pointer operator->() const
            {
                return &**this;
            }

            iterator& operator++()
            {
                node = pool->next(node);
                return *this;
            }

            iterator operator++(int)
            {
                iterator tmp(*this);
                ++(*this);
                return tmp;
            }

            friend bool operator==(iterator const& lhs, iterator const& rhs)
            {
                // assert(lhs.pool == rhs.pool);
                return lhs.node == rhs.node;
            }

            friend bool operator!=(iterator const& lhs, iterator const& rhs)
            {
                return !(lhs == rhs);
            }

            friend void set_successor(iterator x, iterator y)
            {
                // assert(x.p == y.p)
                x.pool->next(x.node) = y.node;
            }

            friend void push_front(iterator& x, T const& value)
            {
                x.node = x.pool->allocate(value, x.node);
            }

            friend void push_back(iterator& x, T const& value)
            {
                auto tmp = x.pool->allocate(value, x.pool->next(x.node));
                x.pool->next(x.node) = tmp;
            }

            friend void free(iterator& x)
            {
                x.pool->free(x.node);
            }
        };
    };

} // namespace rg
