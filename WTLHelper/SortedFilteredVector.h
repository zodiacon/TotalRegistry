#pragma once

#include <vector>
#include <algorithm>
#include <functional>

template<typename T>
class SortedFilteredVector {
public:
	SortedFilteredVector(size_t capacity = 16) {
		_items.reserve(capacity);
		_indices.reserve(capacity);
	}

	void reserve(size_t capacity) {
		_items.reserve(capacity);
		_indices.reserve(capacity);
	}

	void clear() {
		_items.clear();
		_indices.clear();
	}

	bool empty() const {
		return _items.empty();
	}

	void push_back(const T& value) {
		_items.push_back(value);
		_indices.push_back(_indices.size());
	}

	void shrink_to_fit() {
		_items.shrink_to_fit();
		_indices.shrink_to_fit();
	}

	void Remove(size_t index) {
		auto realIndex = _indices[index];
		_items.erase(_items.begin() + realIndex);
		_indices.erase(_indices.begin() + index);
		for (size_t i = index; i < _indices.size(); i++)
			_indices[i]--;
	}

	void ClearSort() {
		int c = 0;
		for (auto& i : _indices)
			i = c++;
	}

	typename std::vector<T>::const_iterator begin() const {
		return _items.begin();
	}

	typename std::vector<T>::const_iterator end() const {
		return _items.end();
	}

	template<typename Iterator>
	void append(Iterator begin, Iterator end) {
		for (auto it = begin; it != end; ++it)
			push_back(std::move(*it));
	}

	template<typename Iterator>
	void insert(size_t at, Iterator begin, Iterator end) {
		//
		// only call after ResetSort and no filter
		//
		size_t count = end - begin;
		_items.insert(_items.begin() + at, begin, end);
		std::vector<size_t> indices(count);
		for (size_t c = 0; c < count; ++c) {
			indices[c] = c + at;
		}
		_indices.insert(_indices.begin() + at, indices.begin(), indices.end());
		for (size_t c = at + count; c < _indices.size(); c++)
			_indices[c] += count;
	}

	void Set(std::vector<T> items) {
		_items = std::move(items);
		auto count = _items.size();
		_indices.clear();
		_indices.reserve(count);
		for (decltype(count) i = 0; i < count; i++)
			_indices.push_back(i);
	}

	const T& operator[](size_t index) const {
		return _items[_indices[index]];
	}

	T& operator[](size_t index) {
		return _items[_indices[index]];
	}

	const T& GetReal(size_t index) const {
		return _items[index];
	}

	void Sort(std::function<bool(const T& value1, const T& value2)> compare) {
		std::sort(_indices.begin(), _indices.end(), [&](size_t i1, size_t i2) {
			return compare(_items[i1], _items[i2]);
			});
	}

	void Sort(size_t start, size_t end, std::function<bool(const T& value1, const T& value2)> compare) {
		if (start >= _indices.size())
			return;

		std::sort(_indices.begin() + start, end == 0 ? _indices.end() : (_indices.begin() + end), [&](size_t i1, size_t i2) {
			return compare(_items[i1], _items[i2]);
			});
	}

	size_t size() const {
		return _indices.size();
	}

	size_t TotalSize() const {
		return _items.size();
	}

	void Filter(std::function<bool(const T&, int)> predicate, bool append = false) {
		if (!append) {
			_indices.clear();
		}
		auto count = _items.size();
		if (predicate == nullptr && !append) {
			for (decltype(count) i = 0; i < count; i++)
				_indices.push_back(i);
		}
		else if (append) {
			std::vector<size_t> indices2(_indices);
			int j = 0;
			for (decltype(count) i = 0; i < _indices.size(); i++, j++) {
				if (!predicate(_items[_indices[i]], (int)i)) {
					indices2.erase(indices2.begin() + j);
					j--;
				}
			}
			_indices = std::move(indices2);
		}
		else {
			for (decltype(count) i = 0; i < count; i++)
				if (predicate(_items[i], int(i)))
					_indices.push_back(i);
		}
	}

	const std::vector<T>& GetRealAll() const {
		return _items;
	}

	const std::vector<T> GetItems() const {
		std::vector<T> items;
		items.reserve(size());
		for (size_t i = 0; i < size(); i++)
			items.push_back(_items[_indices[i]]);
		return items;
	}

	const std::vector<T> GetAllItems() const {
		return _items;
	}

private:
	std::vector<T> _items;
	std::vector<size_t> _indices;
};

