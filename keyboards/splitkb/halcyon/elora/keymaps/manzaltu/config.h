// Copyright 2026 Yoav Orot
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#define TAPPING_TERM 200

// Split transaction that mirrors the display state (Caps Word, last key) to the
// other half, so the display can show it even when it sits on the slave side
#define SPLIT_TRANSACTION_IDS_USER USER_SYNC_DISPLAY
