# 初回セットアップ自動ダウンロード 設計仕様

yt-dlp / ffmpeg を、アプリに同梱せず**初回セットアップ時にユーザー端末が公式配布元から取得**し、保存先パスを設定に書き込む方式の仕様。

- 目的: バンドル頒布を避けることで ffmpeg/yt-dlp のライセンス頒布義務を実質的に外しつつ、yt-dlp を常に更新可能に保つ。
- 非対象（別途対応が必要）: SoundTouch(LGPL, 静的リンク) の対応、JUCE 商用ライセンス、YouTube 規約・著作権リスク。これらは本方式では解決しない。

### 確定した方針（2026-06-01）
- **開始タイミング**: ユーザーがボタン押下で開始。ただし**ツール未検出時のみ**「初回セットアップ」ボタンだけを表示し、検出済みなら従来どおりの設定UI（パス表示＋Browse）を表示する（§5）。
- **ffmpeg ビルド種別**: 可能な限り **LGPL 優先**（Windows=BtbN-LGPL。mac/Linux は入手性次第でGPL許容）。
- **ffmpeg バージョン**: **特定版をピン留め**し、既知 sha256 で検証。更新は手動。

---

## 1. 保存先（パス仕様）

設定ファイルと同じアプリデータ配下に `bin/` を作る。`.vst3` バンドル内には**置かない**（読取専用・コード署名・更新時上書きの問題）。

| OS | ベース | 実効パス（例） |
|---|---|---|
| macOS | `~/Library/Application Support/<Company>/YT/` | `.../YT/bin/yt-dlp`, `.../YT/bin/ffmpeg` |
| Windows | `%APPDATA%\<Company>\YT\` | `...\YT\bin\yt-dlp.exe`, `...\YT\bin\ffmpeg.exe` |
| Linux | `~/.config/<Company>/YT/` | `.../YT/bin/yt-dlp`, `.../YT/bin/ffmpeg` |

- `<Company>` は現状 `initSettings()` のフォルダ名が `"YourCompanyName"` 固定。**要判断**: 実運用前に `JucePlugin_Manufacturer`（= `ichiba`）等へ統一するか。
- 取得先は `juce::PropertiesFile` の親ディレクトリ（`settings->getFile().getParentDirectory()`）から `bin` を派生させ、OS差を吸収する。

---

## 2. 取得元URL（OS × ツール）

必ず**公式 / 著名な配布元**に固定する。

### yt-dlp（GitHub Releases / スタンドアロン版）
Python 同梱の単一バイナリを使う（別途 Python 不要）。

| OS | ファイル | URL（latest） |
|---|---|---|
| macOS | `yt-dlp_macos`（universal2） | `https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos` |
| Windows | `yt-dlp.exe` | `https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe` |
| Linux | `yt-dlp_linux` | `https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux` |

- チェックサム: 各リリースに `SHA2-256SUMS`（+署名 `SHA2-256SUMS.sig`）が公開される。
- 更新: 取得後は yt-dlp 自身の `-U`（または `--update-to`）で自己更新可能。**latest 取得**を基本とする（YouTube 仕様変更で頻繁に陳腐化するため固定版は不可）。

### ffmpeg（静的ビルド）
ffmpeg は yt-dlp ほど壊れないため**特定バージョンをピン留め**し、既知のチェックサムで検証する方が堅牢。

| OS | 配布元（候補） | 備考 |
|---|---|---|
| Windows | BtbN/FFmpeg-Builds の **LGPL** zip（`ffmpeg-n*-win64-lgpl.zip`） | LGPL ビルドを選べる。zip 展開要 |
| macOS | evermeet.cx 静的ビルド（API で sha256 取得可） | 多くは GPL。要 quarantine 解除 |
| Linux | johnvansickle.com 静的（tar.xz） | GPL 中心 |

- **要判断**: GPL ビルドでも「頒布者にならない」ため利用可能だが、可能なら LGPL（Windows は BtbN-LGPL が容易）を優先するか。
- 展開: Windows=zip、mac/Linux=tar.xz。アーカイブ内の `bin/ffmpeg(.exe)` のみ取り出して `bin/` に配置。

---

## 3. ダウンロード & 検証フロー

```
[初回 or 未検出時]
  1. bin/ ディレクトリを作成
  2. 既存バイナリの有無を確認（あれば検証のみ）
  3. yt-dlp:
       a. SHA2-256SUMS を取得
       b. 本体を bin/ に一時名でダウンロード（HTTPS, 進捗通知）
       c. SHA-256 を計算し SUMS と照合
       d. 一致 → 正式名へリネーム / 不一致 → 破棄してエラー
  4. ffmpeg:
       a. ピン留めURL（+既知sha256）でアーカイブ取得
       b. sha256 照合
       c. 展開して ffmpeg(.exe) を bin/ へ
  5. OS別の後処理（§4）
  6. 設定に yt-dlp / ffmpeg のフルパスを書き込み（setYtDlpPath / setFfmpegPath）
  7. 動作確認: `yt-dlp --version` / `ffmpeg -version` を実行し成功を確認
  8. ステータス更新（成功/失敗）
```

- ダウンロードは `juce::URL::downloadToFile`（または `WebInputStream`）をワーカースレッドで実行。`processBlock`/メッセージスレッドをブロックしない。
- SHA-256 は `juce::SHA256`。

---

## 4. OS別の後処理

| 処理 | macOS | Windows | Linux |
|---|---|---|---|
| 実行権限 | `setExecutePermission(true)`（= chmod +x） | 不要 | `chmod +x` |
| アーカイブ展開 | tar.xz（`juce::ZipFile` はzipのみ→ tar.xz は外部 or 実装要） | `juce::ZipFile` で zip 展開 | tar.xz |
| 検疫(Gatekeeper) | `com.apple.quarantine` 除去が必要。`xattr -d` 相当 or ユーザー操作案内 | SmartScreen/AV 警告の可能性 | なし |
| 署名 | 未署名/ad-hoc の Apple Silicon 実行に注意。要実機検証 | — | — |

- **mac の tar.xz 展開**: JUCE 標準は zip のみ。`/usr/bin/tar`（macに標準搭載, xz対応）を `ChildProcess` で呼ぶのが現実的。
- **mac quarantine**: ダウンロード物に付与される拡張属性を除去しないと実行時にブロック。`xattr -dr com.apple.quarantine <path>` 相当を実施。

---

## 5. UI / UX（確定仕様）

設定パネルは**ツールの検出状態で2モードに切り替える**:

- **未検出モード**（yt-dlp と ffmpeg の少なくとも一方が有効に解決できない）:
  - **「初回セットアップ」ボタンのみ**を表示（WAV出力先などの通常設定UIは隠す or 無効化）。
  - 押下でダウンロード開始。進捗は `logDisplay` に表示。
- **検出済みモード**（yt-dlp / ffmpeg 双方が有効）:
  - 従来どおりのパス表示＋「Browse...」UI を表示（WAV Output / yt-dlp / ffmpeg）。
  - 併せて「Update yt-dlp」「Re-run setup」程度の操作を置く（任意）。

判定ロジック:
- 「有効」= 設定パスが存在し実行ファイルで、`--version` が成功（起動時は存在チェックのみ＋必要に応じ遅延検証）。
- 設定パスが空/無効なら管理用 `bin/` 配下の既定パスを参照、それも無ければ未検出。

その他:
- **手動フォールバックを維持**: 「Browse...」で既存インストールを指定可能（ネットワーク不可環境の保険）。検出済みモードで露出。
- 進捗表示: 「Downloading yt-dlp... / ffmpeg...（xx%）」。可能ならプログレスバー。
- 再取得/更新: 「Update yt-dlp」（`yt-dlp -U`）。ffmpeg は再ダウンロード。
- About/Settings に**取得ツールのライセンス表記と出所**を表示（透明性）。

---

## 6. エラーハンドリング

| ケース | 挙動 |
|---|---|
| ネットワーク不可 | エラー表示＋「Browse...で手動指定」を案内 |
| チェックサム不一致 | ダウンロード破棄、再試行を促す（改竄/破損の可能性） |
| 展開失敗 | 一時ファイル削除しエラー |
| 実行検証失敗（--version 非0） | パスを設定に書かずエラー（mac署名/検疫を疑う） |
| 既に存在 | 検証して有効なら再DLしない |

---

## 7. 設定 / 状態管理

- 既存キー `ytDlpPath` / `ffmpegPath` に**自動取得したフルパス**を書き込む（手動指定と同じ仕組みに統一）。
- 追加キー（案）: `toolsManagedByApp`(bool), `ytDlpVersion`, `ffmpegVersion`, `lastSetupCheck`。
- 自動取得したパスと手動指定パスの優先順位を定義（手動指定が空 or 無効なら自動取得を使う等）。

---

## 8. セキュリティ

- すべて **HTTPS**。取得後に **SHA-256 検証**必須。
- yt-dlp は公式 `SHA2-256SUMS` で照合。可能なら GPG 署名も検証（オプション）。
- ffmpeg はピン留め版の**既知 sha256 をアプリに同梱**して照合。
- 一時ファイル→検証→アトミックにリネーム。検証前のバイナリは実行しない。

---

## 9. ライセンス順守（本方式での扱い）

- 自分で再頒布しない構成だが、透明性のため **About に yt-dlp(Unlicense) / ffmpeg(LGPL or GPL) の名称・バージョン・出所URL・ライセンス種別**を明記。
- ffmpeg を GPL ビルドにする場合でも、頒布者はユーザーが取得した公式側。なお SoundTouch(LGPL) と JUCE は本件と無関係に別途対応。

---

## 10. 決定事項 / 残課題

確定:
- 開始タイミング → ボタン押下。未検出時のみ「初回セットアップ」ボタンを表示（§5）。【確定】
- ffmpeg → LGPL 優先（Win=BtbN-LGPL、mac/Linux はGPL許容）。【確定】
- ffmpeg → 特定版ピン留め＋既知sha256検証。【確定】
- mac の tar.xz 展開・quarantine 除去 → `/usr/bin/tar` と `xattr` を `ChildProcess` で呼ぶ方針を採用。【確定】

残課題（実装中に確定でも可）:
- `<Company>` フォルダ名（現状 `YourCompanyName`）を将来 `ichiba` 等へ統一するか。当面は現行値を踏襲し、`PropertiesFile` の親から `bin/` を派生して齟齬を防ぐ。
- ピン留めする ffmpeg の具体バージョン／URL／sha256 の確定値（実装時に最新の安定 LGPL ビルドへ）。

---

## 11. 実装タスク分解（仕様確定後）

1. パス解決ユーティリティ（`bin/` 派生、OS差吸収）。
2. ダウンローダ（ワーカースレッド、進捗コールバック、HTTPS、SHA-256）。
3. 展開・後処理（zip / tar.xz、chmod、quarantine 除去）。
4. 検証（`--version` 実行）＋設定書き込み。
5. UI（Setup/Update ボタン、進捗、フォールバック維持、ライセンス表記）。
6. エラーハンドリングと再試行。
7. mac/Win 実機での E2E 確認。
