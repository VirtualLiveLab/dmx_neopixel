# dmx neopixel
DMX信号をNeoPixelの信号に変換する基板

## 基板設計
- KiCad 9.0で作成
- [JLCPCB](https://jlcpcb.com)に発注
- 部品はほぼ全て秋月電子通商にて購入可能(XLRのレセプタクルのみサウンドハウスで購入)

## ファームウェア書き込み
### 環境構築
- [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html)をインストール
[get-platformio.py](https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py)をダウンロードしてPython3で実行
- USBで基板をPCに接続
- 書き込みコマンド実行

```bash
cd ./platformio/
pio run
```
