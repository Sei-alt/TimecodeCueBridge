# GitHub Actionsの雛形

macos-build.ymlは手動起動するMacビルドの雛形です。現在はdocs内に保管しており、自動実行されません。

有効化する場合は `.github/workflows/macos-build.yml` へ配置します。GitHub OAuthトークンからワークフローを登録するにはworkflow権限が必要です。今回使用したGitHub認証の権限はrepo/read:org/gistであるため、認証を変更せず雛形として保存しています。

Windowsは `packaging/windows/build.ps1` で同じソースをビルドします。Windows CIの追加時には、GUIテストを実行できる環境とNSISを用意してください。
