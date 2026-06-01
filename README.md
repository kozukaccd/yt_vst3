# YT — YouTube Player (VST3)

YouTube の URL から音源をダウンロードし、**DAW 内で独立して再生**できる VST3 インストゥルメント・プラグインです。DAW の再生ヘッドに依存せず、プラグイン単体で再生・停止・シークができます。

## 主な機能
- **YouTube ダウンロード**: URL を入力して音源を取得（`yt-dlp` + `ffmpeg` を使用）。
- **独立再生エンジン**: 高品位な再生（`juce::AudioTransportSource`）。サンプルレート差は自動補正。
- **波形表示＆シーク**: 波形をドラッグして再生位置を移動。
- **Key（ピッチ）**: 音程を ±12 半音、再生速度はそのままで変更。
- **Speed（再生速度）**: 音程はそのままで 0.5×〜2.0× のタイムストレッチ。
- ローカルの WAV / MP3 の読み込み再生も可能。

## 動作環境
- VST3 対応 DAW（macOS / Windows）
- 外部ツール: **yt-dlp** と **ffmpeg**（ダウンロード機能に必要。下記セットアップで自動取得可）

## インストール
### 配布済みの `YT.vst3` を使う場合
プラグインを以下へコピーして DAW を再起動：
- macOS: `~/Library/Audio/Plug-Ins/VST3/`
- Windows: `C:\Program Files\Common Files\VST3\`

### ソースからビルドする場合
- macOS:
  ```
  xcodebuild -project Builds/MacOSX/YT.xcodeproj -target "YT - VST3" -configuration Release build
  ```
  生成された `YT.vst3` は `~/Library/Audio/Plug-Ins/VST3/` に自動コピーされます。
- Windows: 手順は [`docs/windows-setup.md`](docs/windows-setup.md) を参照（Projucer → Visual Studio 2022）。

## 初回セットアップ（yt-dlp / ffmpeg）
ダウンロード機能には `yt-dlp` と `ffmpeg` が必要です。

- **自動（推奨）**: 設定パネルにツール未検出時は「**First-time Setup**」ボタンが表示されます。押すと公式配布元から自動でダウンロード・検証（SHA-256）し、アプリ管理フォルダに配置します。
  - 保存先: macOS `~/Library/Application Support/YourCompanyName/YT/bin/`、Windows `%APPDATA%\YourCompanyName\YT\bin\`
- **手動**: 既に `yt-dlp` / `ffmpeg` を導入済みなら、設定パネルの各「**Browse...**」で実行ファイルを指定できます（ネットワーク制限環境などの保険）。

## 使い方
1. **URL を入力**して「Download」→ 取得後に自動で読み込まれます（または「Load File」でローカルファイルを読み込み）。
2. 再生コントロール（再生 / 一時停止 / 停止）で操作。
3. 波形をドラッグ、またはシークバーで位置移動。
4. **Key** スライダーで音程、**Speed** スライダーで再生速度（音程維持）を調整。

## 設定
設定パネルで以下を指定できます：
- **WAV Output Path**: ダウンロード音源の保存先
- **yt-dlp Path** / **ffmpeg Path**: 実行ファイルのパス（自動セットアップ時は自動入力）

設定の保存場所:
- macOS: `~/Library/Application Support/YourCompanyName/YT/YT.settings`
- Windows: `%APPDATA%\YourCompanyName\YT\YT.settings`

## トラブルシュート
- ダウンロード失敗時は、保存先フォルダの `yt-dlp-debug.log` にログが出ます。
- Windows で SmartScreen / ウイルス対策ソフトが exe を隔離する場合は、許可するか手動セットアップ（Browse 指定）を使用。
- 詳細は [`docs/windows-setup.md`](docs/windows-setup.md) を参照。

## ライセンス / 法的な注意
- YouTube からのダウンロードは **YouTube 利用規約・著作権**に関わります。利用は各自の責任で、商用配布時は別途検討が必要です。
- 同梱・利用ライブラリ: SoundTouch（LGPL）、JUCE（GPL もしくは商用ライセンス）。yt-dlp（Unlicense）/ ffmpeg（LGPL）は利用者端末が公式配布元から取得する構成です。

## ドキュメント
- [`docs/windows-setup.md`](docs/windows-setup.md) — Windows のビルド＆セットアップ
- [`docs/first-run-setup-design.md`](docs/first-run-setup-design.md) — 初回セットアップ自動取得の設計仕様
- [`docs/development-brief.md`](docs/development-brief.md) — 開発の元指示書・ロードマップ
