# Timecode Cue Bridge — Windows / macOS 統合正式リリース

公開日: 2026年10月22日（日本時間）。両OSの配布物を1つのGitHub Releaseにまとめました。

| OS | アプリの版 | 通常利用のダウンロード |
|---|---|---|
| Windows 11 x64 | 0.5.1 | TimecodeCueBridge-0.5.1-Win11-x64-Setup.exe |
| macOS Apple Silicon | 0.6.1 | TimecodeCueBridge-0.6.1-macOS-arm64.dmg |

WindowsのSetup.exeとMacアプリの実行ファイルは、それぞれ従来の検証済み配布物と同一です。MacのDMGは説明書を正式版の表記へ更新して再梱包しました。アプリの版を揃えるための再ビルドは行っていません。

Macは自動テスト4/4と、M1 MacBook Air上でIACドライバ経由のMIDI出力をDaslight 5で受信する実機テストに合格しています（実機テストはユーザー報告）。Windowsの確認範囲は0.5.1の検証資料を参照してください。

Windowsは未署名、Macアプリはad-hoc署名・未公証、Mac PKGは未署名です。この配布状態を明記して正式リリースとしています。App Store/Microsoft Storeへの配信を前提としません。

## ソースと資料

- Windows 0.5.1の元Source.zipと元の検証資料を同梱。
- Mac 0.6.1のSource.zipはこの統合タグの共通ソースです。アプリのC++/Objective-C++実装は検証した0.6.1と同一です。
- 自動生成Source code ZIP/tar.gzもこの統合タグの共通ソースです。Windows 0.5.1当時のソースには添付の元Source.zipを使ってください。
- 両OSの使い方・検証資料、全添付ファイルのSHA256SUMS.txtを同梱。

今後はOS別のリリースを新設せず、同じリリースにWindowsとMacの配布物をまとめる運用とします。過去のOS別リリースとタグは履歴として保持します。
