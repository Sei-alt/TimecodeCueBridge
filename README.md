# Timecode Cue Bridge

LTC音声のタイムコードを読み取り、指定したシーン時刻でOSC/UDPとMIDIを送信する、Windows / Apple Silicon Mac向けのアプリです。C++17、JUCE、libltcを使用します。

## 現在の状態

| 対象 | 状態 |
|---|---|
| Windows 11 x64 / 0.5.1 | 仮完成版。操作・変更履歴は[Windows向けREADME](README_JA.md)に記載 |
| macOS arm64 / 0.6.1 | M1・macOS 26.5.2でネイティブビルド、自動試験4/4、DMGコピー後のGUI起動を確認。未検証の開発版 |
| mainの共通ソース / 0.6.1 | Windows実装を引き継いだ0.6.0 MacソースにMac修正を統合。Windowsでの再ビルド・実機回帰確認はこれから |

Windows 0.5.1の元Source.zip、Setup.exe、検証ZIPは、windows-v0.5.1のReleaseで保存しています。当時のアプリ・依存ソースはrelease/windows-0.5.1ブランチと同じタグにも保存しています。現在のリポジトリにはWindowsの入力・出力・GUI・インストーラー実装も含まれます。

## 機能

- LTC 24 / 25 / 30 fps non-drop、停止・再開時の自動再取得
- シーンの追加・削除・複製・復元、名前、時刻の直接入力と上下ドラッグ
- OSC int32、MIDI Note On/Off・CC・Gate・Panic
- FIRED表示、各行のRearm、Reset All
- 設定・ログ保存、削除時のユーザーデータ保持

## 使用方法と検証記録

- [Macの使い方（日本語）](README-macOS-JA.md) / [English Mac guide](README-macOS.md)
- [Macの実施済み・未検証項目](VERIFICATION-macOS-JA.md)
- [Windowsの使い方と変更履歴](README_JA.md)
- [Windowsの過去の検証記録](BUILD_REPORT_JA.md) / [実機確認項目](RELEASE_CHECKLIST_JA.md)

## ビルド

JUCE 8.0.12とlibltc 1.3.2のソースをthird_partyに同梱し、[DEPENDENCIES.lock](DEPENDENCIES.lock)に版と上流コミットを記録しています。

### Apple Silicon Mac

Command Line ToolsまたはXcodeと、CMake 3.24以上を用意します。cmakeとctestにPATHが通った状態で実行します。

```bash
bash packaging/macos/build.sh
```

ビルド・自動テスト・GUI起動確認・署名検査の後、dist-macにDMG、PKG、アプリZIP、ログを作成します。正常終了後は重複したアプリが一覧に表示されないよう、ビルド用.appを登録解除・削除します。開発用に保持する場合は `TCB_KEEP_APP_BUNDLE=1` を指定してください。

### Windows x64

Visual Studio 2022のC++デスクトップ開発、CMake 3.24以上、NSISを用意し、PowerShellで実行します。

```powershell
./packaging/windows/build.ps1
```

ビルドとCTestに成功するとdistにSetup.exeを作成します。インストーラーの版はCMakeのプロジェクト版から取得します。mainをビルドした場合は0.6.1になります。既存Windows 0.5.1の検証実績は、mainのWindowsビルドの検証実績とは別です。

## リポジトリの構成と運用

- src: 共通GUI、LTC、OSC、MIDI、設定。OS固有処理は条件コンパイルと専用ファイルで管理
- tests: キュー、LTC/OSC/MIDI統合、シーン操作、CoreMIDI、Windowsインストール試験
- packaging/windows・packaging/macos: OSごとの配布処理
- docs/ci: 将来有効化できるGitHub Actionsの雛形。現在は自動実行しません
- third_party・licenses: 固定した依存ソースとライセンス

通常の変更はmainで共有し、検証済みの配布版はOSを付けたタグ（例: macos-v0.6.1、windows-v0.5.1）とGitHub Releasesで記録します。利用者のcues.jsonやevents.log、ビルド成果物、署名用の秘密情報はGitへ追加しないでください。

## ライセンス

アプリはAGPL-3.0-or-laterです。[LICENSE](LICENSE)と[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)を参照してください。Mac配布物はad-hoc署名、PKGは未署名で、公証していません。実オーディオIF、IAC→Daslight、本番ネットワークと遅延の確認状況は検証記録に記載しています。
