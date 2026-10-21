# Timecode Cue Bridge 0.6.1 — Mac版の検証結果

検証日: 2026年10月22日（日本時間）。提供された0.6.0 Macソースを引き継ぎ、Apple Silicon Macでアプリ・DMG・PKG・アプリZIPを生成しました。

## 環境

- Apple M1 / arm64、macOS 26.5.2（25F84）
- Apple Command Line Tools、Apple Clang 15.0.0、macOS SDK 14.4
- CMake 3.31.8、Unix Makefiles、Release、C++17
- JUCE 8.0.12 / libltc 1.3.2（提供ZIPの同梱ソース）
- deployment target: macOS 11.0。macOS 11での実行確認を意味しません。

フル版Xcodeは使っていません。CMake公式配布物のSHA-256を公式チェックサムと照合して使用しました。システムの開発ツール選択やIAC構成は変更していません。

## 今回の変更

- Mac用ビルドをUnix Makefilesへ変更し、Command Line Toolsでビルド可能にしました。TCB_CMAKE_GENERATORで別のジェネレーターを選べます。
- CMakeの旧FetchContent_Populate呼び出しをMakeAvailableへ更新しました。
- ウィンドウの初期サイズを画面の使用可能領域に合わせ、最小サイズを1040×650へ変更しました。Scenesを縦スクロール可能にし、低い画面でも下のMIDI設定にアクセスできる構成にしました。
- CoreMIDI仮想ポートテストのポート検出待ちで、JUCEのメッセージ処理を進めるよう修正しました。旧テストはスレッドを停止して待っていたため、新設ポートのキャッシュ更新を受け取れず失敗していました。
- GUI専用smoke testに任意のPNG保存を追加しました。音声入力とマイク許可要求を行わず画面確認できます。
- 日本語のMac操作手順、現行版の検証記録、配布物のライセンス表記を整理しました。

シーン操作、LTC復帰、OSC/MIDI出力、発火履歴、個別Rearmは既存の共通実装を維持しています。Windowsインストーラーの実装は変更していません。Windows版の再ビルド・回帰検証は今回行っていません。

## 自動テスト

最終CTestは4/4成功、所要時間3.01秒です。

| テスト | 結果 | 確認範囲 |
|---|---|---|
| cue_engine | PASS | 時刻解析、キューの判定、シーク、不連続、発火履歴、個別・全体リセット、MIDIデータ生成 |
| ltc_osc_integration | PASS | 生成LTC→実際のlibltc→キュー→OSC UDP受信バイト照合。44.1/48/96kHz × 24/25/30fps、同時刻キュー、再開、ノイズ、DC、時計の周回、逆再生/DF拒否、設定移行、MIDI処理 |
| coremidi_virtual_routing | PASS | CoreMIDI仮想宛先を作成し、実MIDIポート経由でNote On、GateによるNote Off、CC、PanicのNote Offを受信・照合 |
| scene_editor | PASS | 実際のGUI部品による追加・削除・複製・復元、名前、時刻の入力/ドラッグ/Shift/Esc、FPS、MIDI欄、FIRED表示、一覧の非選択行/再利用行のRearm |

統合試験のライブ経路は、テストハーネスから音声を注入して本番のAudioCallback→FIFO→ワーカースレッド→デコーダー→キューを通しています。OSCは実UDP、統合試験内のMIDIは注入先です。別のCoreMIDI試験では実際のOS仮想ポートを使いました。外部オーディオIFによる録音試験ではありません。

## アプリと配布物

| 項目 | 結果 |
|---|---|
| GUIを含むMacネイティブReleaseビルド | 成功 |
| 実行形式 | Mach-O arm64、最小OS設定11.0、SDK14.4 |
| bundle ID / version | org.timecodecuebridge.app / 0.6.1 |
| 用途説明 | マイク・ローカルネットワークの説明をInfo.plistで確認 |
| 権限設定 | audio-input entitlementを確認 |
| アイコン・ライセンス | Icon.icnsとライセンス文書をバンドル内で確認 |
| 署名 | ad-hoc署名。codesign --verify --deep --strict成功 |
| GUI起動・終了 | 音声なしのsmoke test成功。メイン画面とFIRED状態の画面画像を確認 |
| DMG | 生成、hdiutil verify成功、読み取り専用マウント成功、日本語ガイドとApplicationsリンクを確認 |
| DMGからコピーした.app | 元実行ファイルとSHA-256一致、署名検査成功、GUI起動・終了成功。コピー先は隔離した作業フォルダー |
| PKG | 生成・展開成功。アプリとアンインストーラーのみをApplications配下へ配置する内容を確認。ユーザー設定・ログのパスとインストールスクリプトは含まない |
| ソース | アプリ・テスト・ビルド手順・依存ソース・ライセンスを同梱 |

## 未検証・制約

- マイク許可ダイアログの許可/拒否/再許可、権限待機中の終了。音声なしのsmoke testはマイクを開きません。
- 外部LTC音源と実オーディオIF、ケーブル、実機の停止/再開や抜き差し、スリープ復帰、本番での遅延。
- IAC Driver→Daslightや他の受信アプリ、物理MIDI機器、LAN上のOSC受信機器。CoreMIDI仮想宛先とlocalhost OSCは自動試験済みです。
- 管理者権限を使うPKGの実インストール/アンインストール/再インストール、Applicationsへの実配置。作業フォルダーへのDMGコピー・起動試験と区別してください。
- アンインストーラーによる設定/ログ保持の実地ライフサイクル試験。ソース上では設定ディレクトリを削除せず、固定のアプリ・自身・パッケージ受領情報のみを対象にしています。
- macOS 11〜25や別のMacでの動作、別ディスプレイ構成、GitHub Actionsでの実行。
- Developer ID署名、公証、オンライン配布後のGatekeeper受け入れ。利用可能な署名用証明書は0件でした。アプリはad-hoc署名、PKGは未署名、いずれも未公証の開発版です。

使用手順はREADME-macOS-JA.mdに記載しています。まずDMGからアプリをコピーして起動し、マイク権限と実際のLTC/受信ソフトの接続を確認してください。

## 配布後の整理

アプリ一覧に、作業フォルダー内のビルド用/DMG作成用/コピー試験用の3つの.appが表示されることを確認しました。Applications内の0.6.1と実行ファイルのSHA-256が一致すること、および配布アプリZIPから復元できることを確認してから、この3つを登録解除・削除しました。DMGも取り出しています。Applicationsのアプリと設定・ログは保持しました。再発防止として、build.shはパッケージ生成成功後にビルド用.appを登録解除・削除するよう変更しました。この後の変更はシェルスクリプトと文書のみで、配布バイナリは同一です。
