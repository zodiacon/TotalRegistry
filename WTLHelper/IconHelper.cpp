#include "pch.h"
#include "IconHelper.h"

HICON IconHelper::GetShieldIcon() {
	return GetStockIcon(SIID_SHIELD);
}

HICON IconHelper::GetStockIcon(SHSTOCKICONID id, bool big) {
	SHSTOCKICONINFO ssii = { sizeof(ssii) };
	if (FAILED(::SHGetStockIconInfo(id, (big ? 0 : SHGSI_SMALLICON) | SHGSI_ICON, &ssii)))
		return nullptr;

	return ssii.hIcon;
}
