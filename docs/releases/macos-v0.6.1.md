# macOS arm64 0.6.1

Apple Silicon向けの開発版です。M1・macOS 26.5.2でReleaseビルドと自動テスト4/4、DMGからコピーしたアプリのGUI起動を確認しました。

- Command Line ToolsとCMakeでビルド可能
- 画面に合わせた初期サイズとシーン欄のスクロール
- CoreMIDI仮想宛先へのNote On/Off・CC・Gate・Panicの実送受信テスト
- 検証用.appが重複表示されないためのパッケージ作成後の整理

アプリはad-hoc署名、PKGは未署名で、いずれも未公証です。実LTC入力機器、IAC→Daslight、PKGの実インストール/削除、別のMacやOSでの確認は残っています。

[日本語の使い方](../../README-macOS-JA.md) / [検証記録](../../VERIFICATION-macOS-JA.md)

このタグは共通ソース0.6.1を記録します。MacのC++/Objective-C++実装はローカルで検証した0.6.1と同一です。GitHub管理準備では文書整理とWindowsインストーラーのバージョン取得処理を追加しました。Windows側の変更はMac上では実行確認していません。
