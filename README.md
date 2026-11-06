# N-Air OBS (ニコニコ生放送 連携プラグイン)

OBS Studio 向けに開発された、ニコニコ生放送（ニコ生）の公式APIおよびメッセージサーバと連携するネイティブプラグインです。
公式配信ソフトウェア「N-AIR」の便利な機能（ワンクリック枠情報取得、ストリームキー自動設定、リアルタイムコメント表示・投稿、番組開始・終了・延長）を、OBS Studio の標準Qt UI（ドックウィジェット）としてシームレスに提供します。

---

## 主な機能

### 1. 配信管理ドック (`NicoliveControlDock`)
- **アカウント状態表示**:
  - ニコニコユーザーアイコン、ニックネーム、ユーザーID、プレミアム会員バッジ
  - ログイン / ログアウト機能
- **番組情報の自動同期**:
  - タイトル、番組ID（`lv...`）、放送ステータス（放送前 / 放送中 / 終了）
  - 経過時間デジタルタイマー（`00:15:32`）
  - 来場者数およびコメント数のリアルタイム表示
- **OBS配信設定の自動/ワンクリック反映**:
  - ニコ生から取得した RTMP URL および ストリームキーをワンクリックで OBS のカスタム配信設定に即時適用
  - コピペの手間を解消
- **番組ライフサイクル制御**:
  - 番組開始（`PUT /api/v1/unama/programs/start`）
  - 番組終了（`PUT /api/v1/unama/programs/end`）
  - +30分延長（`PUT /api/v1/unama/programs/extension`）
  - ブラウザで番組視聴ページを開くショートカット

### 2. コメントドック (`NicoliveCommentDock`)
- **リアルタイムコメント受信**:
  - 新メッセージサーバ（NDGR）および WebSocket API に対応
  - コメント番号、ユーザー（名前 / 184 / 放送者）、コメント本文、時刻を一覧表示
  - 運営・放送者コメントの強調表示
  - コメント色（コマンド）の反映
  - 自動スクロール追従（手動スクロール時は自動一時停止）
- **フィルタ機能**:
  - 184（匿名）コメントのワンクリック非表示切替
  - コメント一覧クリア
- **コメント投稿**:
  - 放送者コメント（運営コメント）と通常コメントの切り替え投稿
  - コマンド（文字色など）指定
  - Enterキーによるクイック送信

### 3. 自動化 & 設定ダイアログ (`NicoliveSettingsDialog`)
- **OAuth 2.0 認可 & トークン管理**:
  - ブラウザを起動してローカルサーバー（`127.0.0.1`）経由で認証コードを自動受信
  - 手動アクセストークン / `user_session` Cookie 入力フォールバック対応
- **配信自動連携**:
  - 番組検出時にストリーム設定を自動反映
  - OBSで配信開始した際に自動で番組を開始
  - OBSで配信終了した際に自動で番組を終了
- **コメント表示カスタマイズ**:
  - 最大保持行数
  - フォントサイズ

---

## ディレクトリ構成

```text
nair-obs/
├── buildspec.json             # OBS依存関係・メタデータ定義
├── CMakeLists.txt             # プロジェクトビルド定義 (Qt6 + libobs + obs-frontend-api)
├── CMakePresets.json          # プリセット定義
├── data/
│   └── locale/                # 言語ファイル (en-US.ini, ja-JP.ini)
└── src/
    ├── plugin-main.cpp        # モジュール登録、ライフサイクル・イベント管理
    ├── common/
    │   ├── NicoliveData.h     # 番組情報、ユーザー情報、コメント構造体
    │   ├── ConfigManager.h    # 設定管理
    │   └── ConfigManager.cpp
    ├── auth/
    │   ├── NicoliveAuthManager.h   # OAuth 2.0 / セッション認証マネージャー
    │   └── NicoliveAuthManager.cpp
    ├── api/
    │   ├── NicoliveApiClient.h     # ニコ生公式 REST API クライアント
    │   └── NicoliveApiClient.cpp
    ├── comment/
    │   ├── NdgrDecoder.h           # NDGR (Protobuf) 軽量デコーダー
    │   ├── NdgrDecoder.cpp
    │   ├── NicoliveCommentReceiver.h # コメント・統計受信マネージャー
    │   └── NicoliveCommentReceiver.cpp
    ├── obs/
    │   ├── ObsStreamHelper.h       # OBS配信設定（RTMP/キー）反映ヘルパー
    │   └── ObsStreamHelper.cpp
    └── ui/
        ├── NicoliveControlDock.h   # 配信管理ドック
        ├── NicoliveControlDock.cpp
        ├── NicoliveCommentDock.h   # コメントドック
        ├── NicoliveCommentDock.cpp
        ├── NicoliveSettingsDialog.h # 設定ダイアログ
        └── NicoliveSettingsDialog.cpp
```

---

## ビルド方法

本プロジェクトは公式の `obs-plugintemplate` をベースにしており、CMake および Visual Studio 2022 (または Build Tools) / Ninja でビルド可能です。

### 前提条件
- CMake 3.28 以上
- Visual Studio 2022 (C++ ワークロード) または Build Tools
- Windows x64

### ビルド手順

```powershell
# 依存関係のダウンロードと CMake の構成
cmake --preset windows-x64

# ビルド
cmake --build build_x64 --config RelWithDebInfo
```

ビルドが完了すると、`build_x64/rundir/` または `release/RelWithDebInfo/` にプラグインバイナリ（`nair-obs.dll`）が生成されます。
OBS Studio のプラグインディレクトリ（`%APPDATA%\obs-studio\plugins\` または OBS インストール先）に配置してご利用ください。

---

## 参考資料・仕様
- `integration-docs`: ニコニコ生放送 API / OAuth / メッセージサーバ (NDGR) 仕様書
- `n-air-app`: N-AIR 公式デスクトップアプリケーション実装
- `obs-plugintemplate`: OBS Studio 公式プラグインテンプレート
