1. プロジェクト概要
YouTube上の音源を指定したURLからキャッシュ（ダウンロード）し、DAW内で再生・同期演奏するためのVST3インストゥルメント/プラグイン。
本プロジェクトはjucerproで作成されたベースプロジェクトで、あなたのミッションはこれらを利用して下記のようなプラグインを作ることです。

主要機能
YouTubeダウンローダー: yt-dlp を使用して音源を取得。

オーディオキャッシュ: ダウンロードした音声をWAV形式でローカルに保存。

独立再生エンジン: DAWの再生ヘッドに依存せず、プラグイン独自の操作で再生・停止が可能。

オーディオプレイヤー: juce::AudioTransportSource を用いた高品位な再生。

2. 技術スタック・前提条件
Framework: JUCE 7.x / 8.x (Audio Plug-in Project)

Language: C++17 以上

Format: VST3

External Tools:

yt-dlp: YouTube動画の解析・音声抽出用

ffmpeg: 音声のWAV変換用（yt-dlpが内部で使用）

Modules: juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils, juce_core, juce_graphics, juce_gui_basics

3. 開発工程ロードマップ
Phase 1: 基本オーディオエンジンの実装
AudioFormatManagerの初期化: WAV/MP3の読み込み対応。

AudioTransportSourceの実装: processBlock での再生ロジック構築。

ファイルロード機能: ローカルのWAVファイルを読み込み、再生できる状態にする。

Phase 2: YouTube連携（ダウンロード機能）
ChildProcessのラッパー作成: yt-dlp を実行するヘルパークラスの実装。

非同期ダウンロードの実装: juce::Thread を継承し、UIをブロックせずにダウンロードを実行。

キャッシュ管理: ダウンロード済みファイルのパス管理と重複ダウンロードの防止。

Phase 3: ユーザーインターフェース (UI)
URL入力フィールド: juce::TextEditor の設置。

実行コントロール: Download/Play/Stop/Reset ボタンの実装。

ステータス表示: ダウンロード中、読み込み完了、エラー等のフィードバック表示。

Phase 4: ブラッシュアップ
サンプルレート変換: ダウンロードした音源とDAWのサンプルレートが異なる場合の自動リサンプリング。

エラーハンドリング: URL無効、オフライン状態、ツール未インストール時の例外処理。

4. クラス設計のヒント
AudioProcessor (バックエンド)
juce::AudioTransportSource: 再生制御。

std::unique_ptr<juce::AudioFormatReaderSource>: 現在読み込んでいるソース。

void loadFile(const juce::File& file): 指定ファイルを再生可能にするメソッド。

DownloadThread (ユーティリティ)
run() メソッド内で juce::ChildProcess を実行。

完了時に ChangeListener やコールバックで AudioProcessor に通知。
