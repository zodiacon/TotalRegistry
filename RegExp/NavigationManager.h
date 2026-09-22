#pragma once

template<typename T>
struct NavigationManager {
	bool CanGoBack() const {
		return m_Current > 0;
	}

	T const& GoBack() {
		ATLASSERT(CanGoBack());
		return m_Items[--m_Current];
	}

	bool CanGoForward() const {
		return m_Current < m_Items.size() - 1;
	}

	T const& GoForward() {
		ATLASSERT(CanGoForward());
		return m_Items[++m_Current];
	}

	void Add(T const& address) {
		m_Current++;
		m_Items.resize(m_Current);
		m_Items.push_back(address);
	}

private:
	std::vector<T> m_Items;
	int m_Current{ -1 };
};

