#pragma once

namespace khorost {
    namespace data {
        template <typename T, typename SizeT = size_t, SizeT DefaultBufferGranulate = 0x400>
        class auto_buffer_t final {
            typedef SizeT size_type;

            mutable T* m_buffer_; // буфер с данными
            mutable size_type m_full_size_; // полный размер буфера
            mutable size_type m_ready_position_; // позиция в буфере куда можно записывать данные
            bool m_auto_free_; // при "уничтожении" объекта освободить занимаемую им память
        public:
            static const SizeT npos;

            // Заполнение массива данными из внешнего источника
            auto_buffer_t(const T* buffer, const size_type buffer_size) :
                m_buffer_(buffer),
                m_full_size_(buffer_size),
                m_ready_position_(buffer_size),
                m_auto_free_(false) {
            }

            // Создание массива с автоматическим освобождением
            auto_buffer_t() :
                m_buffer_(nullptr),
                m_full_size_(0),
                m_ready_position_(0),
                m_auto_free_(true) {
            }

            auto_buffer_t(const auto_buffer_t& right) {
                if (this != &right) {
                    *this = right;
                }
            }

            ~auto_buffer_t() {
                if (m_auto_free_) {
                    free(m_buffer_);
                }

                m_buffer_ = nullptr;
                m_full_size_ = m_ready_position_ = 0;
            }

            auto_buffer_t& operator=(const auto_buffer_t& right) {
                if (this != &right) {
                    m_auto_free_ = right.m_auto_free_;
                    m_ready_position_ = right.m_ready_position_;
                    m_full_size_ = right.m_full_size_;
                    if (m_auto_free_) {
                        m_buffer_ = static_cast<T*>(realloc(nullptr, m_full_size_ * sizeof(T)));
                        memcpy(m_buffer_, right.m_buffer_, m_ready_position_ * sizeof(T));
                    } else {
                        m_buffer_ = right.m_buffer_;
                    }
                }
                return *this;
            }

            //	**********************************************************************
            // Кол-во элементов буфера
            size_type get_full_size() const { return m_full_size_; }
            // Размер заполненной области буфера
            size_type get_fill_size() const { return m_ready_position_; }
            // Размер свободной области, доступной для записи
            size_type get_free_size() const { return m_full_size_ - m_ready_position_; }
            //	**********************************************************************
            T* get_head() const { return m_buffer_; }
            T* get_position(size_type position = 0) const { return m_buffer_ + position; }
            T* get_free_position() { return m_buffer_ + m_ready_position_; }
            //	**********************************************************************
            T get_element(SizeT position) { return m_buffer_[position]; }
            T get_last_element() { return m_buffer_[m_ready_position_ - 1]; }
            T& operator[](size_type position) { return m_buffer_[position]; }
            //	**********************************************************************
            void flush_free_size() const { m_ready_position_ = 0; }

            size_type find(const size_type from, const T* match, const size_type match_size) {
                const auto byte_match_size = sizeof(T) * match_size;
                const auto p_max = m_ready_position_ - match_size;
                for (auto p_check = from; p_check <= p_max; ++p_check) {
                    if (memcmp(m_buffer_ + p_check, match, byte_match_size) == 0) {
                        return p_check;
                    }
                }
                return npos;
            }

            size_type compare(size_type from, const T* match, size_type match_size) {
                if ((from + match_size) > m_ready_position_) {
                    return npos;
                } else {
                    return memcmp(m_buffer_ + from, match, sizeof(T) * match_size);
                }
            }

            size_type decrement_free_size(size_type size) {
                if ((m_ready_position_ + size) < m_full_size_)
                    m_ready_position_ += size;
                else
                    m_ready_position_ = m_full_size_;
                return m_ready_position_;
            }

            size_type increment_free_size(size_type size) {
                if (m_ready_position_ >= size)
                    m_ready_position_ -= size;
                else
                    m_ready_position_ = 0;
                return m_ready_position_;
            }

            size_type append(const T* buffer, size_type count, size_type buffer_granulate = DefaultBufferGranulate) {
                if (count > get_free_size()) {
                    check_size(get_fill_size() + count, buffer_granulate);
                }

                memcpy(get_free_position(), buffer, count * sizeof(T));

                m_ready_position_ += count;
                return m_ready_position_;
            }

            void check_size(size_type demand_size, size_type buffer_granulate = DefaultBufferGranulate) {
                if (demand_size == 0) {
                    demand_size = m_ready_position_ + buffer_granulate;
                }

                if (demand_size > m_full_size_) {
                    m_full_size_ = (demand_size / buffer_granulate + 1) * buffer_granulate;

                    m_buffer_ = static_cast<T*>(realloc(m_buffer_, m_full_size_ * sizeof(T)));
                    //	буфер "раздувается" по мере приема необработанной информации
                    //	имеет смысл сделать возможность уменьшения рабочего объема по мере
                    //	использования информации из буфера
                }
            }

            void cut_from_head(size_type count_byte) {
                // TODO вырезание вначале сделать только для случая когда в конце не хватает данных.
                // Тогда при частой обработке данных слева может ничего не придется реально двигать
                if (count_byte == 0 || count_byte > m_ready_position_)
                    return;

                if (count_byte != m_ready_position_) {
                    memmove(m_buffer_, m_buffer_ + count_byte, (m_ready_position_ - count_byte) * sizeof(T));
                }

                m_ready_position_ -= count_byte;
            }

            bool replace(const T* match_buffer, size_type match_count, const T* replace_buffer, size_type replace_count, const bool single = true) {
                if (m_ready_position_ < match_count)
                    return false;

                auto match_bytes = match_count * sizeof(T);
                const auto replace_bytes = replace_count * sizeof(T);
                auto max_pos = m_ready_position_ - match_count;
                auto result = false;

                decltype(match_count) delta_expand = 0, delta_sub = 0;

                if (replace_count != match_count) {
                    if (replace_count > match_count) {
                        delta_expand = replace_count - match_count;
                    } else {
                        delta_sub = match_count - replace_count;
                    }
                }

                for (decltype(match_count) km = 0; km <= max_pos;) {
                    if (memcmp(m_buffer_ + km, match_buffer, match_bytes) == 0) {
                        if (delta_expand != 0) {
                            check_size(m_ready_position_ + delta_expand);
                        }
                        // сдвигаем хвост 
                        if (delta_expand != 0 || delta_sub != 0) {
                            memmove(m_buffer_ + km + replace_count, m_buffer_ + km + match_count,
                                    (m_ready_position_ - (km + match_count)) * sizeof(T));
                        }
                        memcpy(m_buffer_ + km, replace_buffer, replace_bytes);

                        if (delta_expand) {
                            m_ready_position_ += delta_expand;
                            max_pos += delta_expand;
                            km += replace_count;
                        } else if (delta_sub) {
                            m_ready_position_ -= delta_sub;
                            max_pos -= delta_sub;
                            km += match_count;
                        } else {
                            km += replace_count;
                        }

                        result = true;
                        if (single)
                            break;
                    } else {
                        ++km;
                    }
                }

                return result;
            }
        };

        template <typename T, typename SizeT, SizeT DefaultBufferGranulate>
        const typename auto_buffer_t<T, SizeT, DefaultBufferGranulate>::size_type
        auto_buffer_t<T, SizeT, DefaultBufferGranulate>::npos =
           static_cast<typename auto_buffer_t<T, SizeT, DefaultBufferGranulate>::size_type>(-1);

        template <typename T, typename SizeT>
        class auto_buffer_chunk_t {
            auto_buffer_t<T, SizeT>& m_parent_;
            SizeT m_reference_;
        public:
            explicit auto_buffer_chunk_t(auto_buffer_t<T>& parent) :
                m_parent_(parent)
                , m_reference_(m_parent_.npos) {
            }

            const T* get_chunk() const {
                return is_valid() ? m_parent_.get_position(m_reference_) : NULL;
            }

            void clear_reference() { m_reference_ = m_parent_.npos; }
            void set_reference(SizeT reference) { m_reference_ = reference; }
            SizeT get_reference() const { return m_reference_; }

            bool is_valid() const { return m_reference_ != m_parent_.npos; }
        };

        typedef auto_buffer_t<char> auto_buffer_char;
        typedef auto_buffer_t<wchar_t> auto_buffer_wide_char;

        typedef auto_buffer_chunk_t<char, size_t> auto_buffer_chunk_char;
    }
}
