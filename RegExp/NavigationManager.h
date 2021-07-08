#pragma once

template<typename T>
struct NavigationManager {
	bool CanGoBack() const;
	bool GoBack();
	bool CanGoForward() const;
	bool GoForward();

private:
	std::vector<T> _items;
	int _current;
};
