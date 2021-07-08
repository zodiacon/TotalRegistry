#pragma once

struct SecurityHelper abstract final {
	static bool IsRunningElevated();
	static bool RunElevated();
};
