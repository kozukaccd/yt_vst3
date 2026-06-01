# Windows セットアップ手順

Windows での **(A) ビルド（開発者向け）** と **(B) 利用開始＝yt-dlp / ffmpeg のセットアップ（利用者向け）** をまとめる。

---

## A. ビルド（開発者向け）

### 必要なもの
- **Visual Studio 2022**（「C++ によるデスクトップ開発」ワークロード）
- **JUCE**（Projucer.exe を含む。macで使用しているのと同じバージョン推奨）
- JUCE モジュール一式（`juce_*`）。`ea_soundtouch` はリポジトリの `modules/` に同梱済み。

### 手順
1. `Bard.jucer` を **Projucer** で開く。
   - `juce_*` モジュールはグローバルパス参照のため、Projucer の **Global Paths** に `JUCE/modules` を設定。
   - `ea_soundtouch` はリポジトリ内 `modules/` を参照（設定済み）。
   - **VST3 を有償・クローズドで配布するなら JUCE の商用ライセンスが必要**（GPL版のままだと公開義務が生じる）。
2. Projucer で **Save Project** → `Builds/VisualStudio2022/Bard.sln` が生成される。
   （VS2022 エクスポーターは `Bard.jucer` に追加済み。`.sln`/`.vcxproj` 自体は Projucer が生成するため、初回は Save が必須）
3. `Bard.sln` を Visual Studio で開く。
4. 構成を **Release / x64** にし、**`Bard_VST3`** ターゲットをビルド。
5. 生成された `Bard.vst3` を `C:\Program Files\Common Files\VST3\` にコピー（多くのDAWの既定の読込先）。

### 備考
- `JUCE_USE_MP3AUDIOFORMAT=1` は `Bard.jucer` 設定済み（MP3読み込み対応）。
- SoundTouch（`ea_soundtouch`）の SSE/MMX 最適化は x64 でそのままコンパイルされる。
- SoundTouch は LGPL。配布時は表記・ソース・差し替え可能性の順守が必要（別途）。

---

## B. yt-dlp / ffmpeg のセットアップ（利用者向け）

YouTube のダウンロードには `yt-dlp.exe` と `ffmpeg.exe` が必要。プラグイン内蔵の自動取得を使うのが簡単。

### 自動セットアップ（推奨）
1. DAW で **Bard** を読み込む。
2. 設定パネルにツールが未検出なら **「First-time Setup」ボタン**だけが表示される。これを押す。
3. 公式配布元から自動ダウンロード＆検証され、配置される：
   - yt-dlp: GitHub Releases（最新版）+ `SHA2-256SUMS` で検証
   - ffmpeg: BtbN の **LGPL** ビルド + `checksums.sha256` で検証
   - 保存先: `%APPDATA%\YourCompanyName\Bard\bin\`（`yt-dlp.exe` / `ffmpeg.exe`）
4. 完了すると通常のパス表示UIに切り替わり、ダウンロードが使えるようになる。

### 手動セットアップ（自動が使えない場合）
ネットワーク制限・SmartScreen・ウイルス対策ソフトのブロック等で自動取得が失敗する場合：
1. `yt-dlp.exe` を公式（https://github.com/yt-dlp/yt-dlp/releases ）から入手。
2. `ffmpeg.exe` を入手（BtbN LGPL ビルド推奨: https://github.com/BtbN/FFmpeg-Builds/releases ）。
3. 任意のフォルダに置き、設定パネルの各 **「Browse...」** で `yt-dlp.exe` / `ffmpeg.exe` を指定。
   - 既に PATH に通っている場合でも、確実性のため絶対パス指定を推奨。

### よくあるトラブル
| 症状 | 対処 |
|---|---|
| 「First-time Setup」を押しても失敗 | ネットワーク/プロキシ確認。ダメなら手動セットアップで Browse 指定 |
| SmartScreen / AV が exe を隔離 | ダウンロードを許可、または手動で配置して Browse 指定 |
| ダウンロードはDAW起動中の通信がブロックされる環境 | 手動セットアップを使用 |
| 取得後にダウンロードが失敗する | `%APPDATA%\YourCompanyName\Bard\yt-wav\yt-dlp-debug.log` を確認 |

### 注意（法務）
- YouTube からのダウンロードは YouTube 利用規約・著作権に関わる。商用配布時は別途検討が必要。
- 本方式（利用者端末が公式配布元から取得）は ffmpeg/yt-dlp の再頒布義務を負わない構成だが、SoundTouch(LGPL)・JUCE 商用ライセンスは別途対応が必要。
