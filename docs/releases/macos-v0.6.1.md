# macOS arm64 0.6.1

Apple Silicon向けの開発版です。2026年10月22日、ユーザーからMac版0.6.1の実機テスト合格の報告を受けました。個別の機器・接続構成・テスト項目は未記録です。M1・macOS 26.5.2でReleaseビルドと自動テスト4/4、DMGからコピーしたアプリのGUI起動を確認しました。

- Command Line ToolsとCMakeでビルド可能
- 画面に合わせた初期サイズとシーン欄のスクロール
- CoreMIDI仮想宛先へのNote On/Off・CC・Gate・Panicの実送受信テスト
- 検証用.appが重複表示されないためのパッケージ作成後の整理

アプリはad-hoc署名、PKGは未署名で、いずれも未公証です。実機テストは上記のユーザー報告として記録し、個別項目は[検証記録](../../VERIFICATION-macOS-JA.md)で初回ビルド時点の情報と区別しています。

[日本語の使い方](../../README-macOS-JA.md) / [検証記録](../../VERIFICATION-macOS-JA.md)

このタグは共通ソース0.6.1を記録します。MacのC++/Objective-C++実装はローカルで検証した0.6.1と同一です。GitHub管理準備では文書整理とWindowsインストーラーのバージョン取得処理を追加しました。Windows側の変更はMac上では実行確認していません。
