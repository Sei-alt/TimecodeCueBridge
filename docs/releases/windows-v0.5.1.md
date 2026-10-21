# Windows 0.5.1 — 仮完成版

ユーザーが仮完成としたWindows 11 x64版です。提供された元のSource.zip、Setup.exe、Verification.zipを変更せずReleaseへ保存しています。

元の検証記録と照合し、次のSHA-256が一致しました。

- Source.zip: `a18134a4bae823baf29c4e28f99f36a8a2c7fdf3ff044c555b4bdb839eb73f38`
- Setup.exe: `215a3a43a39228deac9eec13848a31ecdcf6e1b4684181792d8a6b46aeff0d3f`

release/windows-0.5.1ブランチとwindows-v0.5.1タグに、この版のアプリ・依存ソースを保存しています。アプリのソースは元ZIPと一致します。既存のGitHub認証の権限を維持するため、元ZIP内のActionsワークフローだけはdocs/ciへ移し、実行しない雛形として保管しています。元のパスを含む完全なZIPはReleaseのTimecodeCueBridge-0.5.1-Source.zipです。

元の検証記録: [BUILD_REPORT](../history/windows-0.5.1-BUILD_REPORT_JA.md) / [SHA-256一覧](../history/windows-0.5.1-original-SHA256SUMS.txt)

今回は過去の配布物の再登録であり、Windowsバイナリを再ビルドしていません。
