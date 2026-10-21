# Timecode Cue Bridge 0.6.1 — Apple Silicon Mac

LTC音声を読み取り、指定した時刻でOSCとMIDIを送るアプリです。画面は英語です。2026年10月22日のWindows/Mac統合正式リリースに収録しています。

## 起動

1. DMGを開き、Timecode Cue Bridge.appをApplicationsへドラッグします。
2. アプリを起動し、マイクへのアクセスを許可します。外部オーディオ入力にもこの許可が必要です。
3. I/O Settingsで入力機器とLTCが入る1チャンネルを選び、音源と同じ24 / 25 / 30 fpsを選びます。non-dropのみ対応します。
4. OSCの宛先、必要ならMIDI出力ポートを選びます。
5. Scenesでシーンを追加し、名前、時刻、OSCやMIDIの送信内容を設定します。
6. Save & Applyを押し、Test Sceneで受信を確認します。Test SceneはDisarm中にも送信します。
7. LTCを流してLOCKEDを確認し、Armを押します。キュー時刻の1秒以上前から再生してください。

開発者証明書とAppleの公証は付いていません。起動が拒否された場合は、ファイルの出所を確認し、[Appleの開き方](https://support.apple.com/102445)に従って「システム設定 → プライバシーとセキュリティ → このまま開く」を使います。

## シーンと時刻

＋で追加、−で削除、Duplicateで複製、Restoreで直前の削除を戻します。時刻の数字はクリックして直接入力、上下ドラッグで変更できます。Shiftで細かく調整、Enterで確定、Escで取り消します。

発火済みシーンは緑色のFIRED表示になります。一覧の各行にあるRearmでそのシーンだけを未発火に戻せます。即時再送する操作はTest Sceneです。Reset Allは全シーンを戻します。停止・再開や巻き戻しだけでは発火履歴を消しません。

画面が低い場合はScenes欄を縦スクロールしてください。MIDIのValueとGateも下部にあります。Save & ApplyやArmなどの操作ボタンは画面下部に残ります。

## 同じMacのソフトへMIDI送信

「Audio MIDI設定 → ウインドウ → MIDIスタジオを表示」でIACドライバを開き、「装置はオンライン」を有効にしてバスを作ります。本アプリのI/O SettingsでRefreshを押してそのIACバスを出力に選び、受信ソフトでは同じバスを入力に選択します。各シーンのMIDIを有効にして、Note/CCの割り当てを受信側で設定してください。[AppleのIAC設定手順](https://support.apple.com/guide/audio-midi-setup/ams1013/mac)

MIDI Panicは本アプリが保持しているノートを停止してDisarmします。2026年10月22日、M1 MacBook Air上で本アプリ → IACドライバ → Daslight 5のMIDIルーティングが動作したとのユーザー確認を受けています。

## 設定と削除

設定とログは `~/Library/Application Support/TimecodeCueBridge/` に保存します。Data Folderから開けます。起動時は必ずDisarmです。Windowsのcues.jsonを使う場合は、Mac側の音声機器とMIDIポートを選び直して保存してください。

DMGから入れた場合は、終了してからアプリをゴミ箱へ移します。PKGを使った場合はApplicationsのUninstall Timecode Cue Bridge.commandを実行し、REMOVEと入力します。設定とログは保持します。

## 開発と検証

ビルドにはCommand Line Tools（またはXcode）とCMake 3.24以上が必要です。JUCEとlibltcのソースは同梱しています。ソースのルートで `bash packaging/macos/build.sh` を実行すると、Releaseビルド、自動テスト、署名検査、GUI起動試験、DMG/PKG生成を行います。フル版Xcodeのインストールは必須ではありません。

検証済み項目と未検証項目は `VERIFICATION-macOS-JA.md` を確認してください。実オーディオIFのLTC入力、マイク許可ダイアログの操作、本番環境の遅延、PKGの管理者権限でのインストールは別途確認が必要です。

## アプリ一覧の重複を避ける

DMGからApplicationsへコピーした後はDMGを取り出してください。配布用ビルドスクリプトは、パッケージ作成成功後にビルド用.appの登録を解除して削除し、DMG/PKG/アプリZIPだけを残します。開発用.appを残す場合だけ `TCB_KEEP_APP_BUNDLE=1` を指定します。
