# MIMIC

Sanatçı seç, vokal zinciri o sanatçının kayıtlarındaki mix karakterine kurulsun. Sesini klonlamaz, mix'ini verir: tune hızı, formant, EQ, kompres, saturasyon, doubler, delay ve reverb tek seferde ayarlanır.

## Sanatçılar

- **Rap:** Juice WRLD, Playboi Carti, Lil Uzi Vert, Travis Scott, Don Toliver, Ken Carson, Future
- **Türkçe Rap:** Ezhel, Uzi, Lvbel C5, Blok3
- **Pop / R&B:** Billie Eilish, The Weeknd, Frank Ocean, SZA

## Kontroller

Bütün makrolar **%50'de sanatçının kendi ayarı**dır (düğmenin üstündeki küçük nokta). Çift tık %50'ye döndürür, Shift ile ince ayar yapılır.

| Kontrol | Ne yapar |
|---|---|
| Tune | %0 kapalı, %50 sanatçı, %100 anında (T-Pain) tune |
| Air | Tepe parlaklığı; artırınca de-esser da biraz güçlenir |
| Grit | Saturasyon → distortion. Ses seviyesi otomatik eşitlenir |
| Space | Delay + reverb miktarı ve kuyruk uzunluğu (delay şarkının BPM'ine kilitli) |
| Width | Doubler ve stereo genişlik |
| Amount | Bütün zincirin ne kadar uygulanacağı (dry/wet) |
| Formant | Ses karakteri: + daha ince/genç (Carti "baby voice"), − daha kalın |
| Ton / Dizi | Tune'un hangi notalara oturacağı. Kromatik, Majör, Minör, Harmonik Minör, **Hicaz**, Minör Pentatonik |

Gecikme yaklaşık 38 ms (pitch analizi için). FL Studio bunu otomatik telafi eder; canlı kayıtta dinlerken MIMIC'i kapatıp mix'te aç.

## Derleme (Windows)

1. Visual Studio 2022'yi (ücretsiz Community veya Build Tools) **"Desktop development with C++"** ile kur.
2. `build.bat`'a çift tıkla. İlk seferde JUCE 9 ve WebView2 SDK otomatik indirilir.
3. Çıktı: `build\MIMIC_artefacts\Release\VST3\MIMIC.vst3`. Yönetici olarak çalıştırırsan `C:\Program Files\Common Files\VST3` içine kendisi kopyalar.

**Visual Studio kurmadan:** klasörü bir GitHub reposuna yükle. `.github/workflows/build.yml` her push'ta Windows'ta derler; hazır VST3 "Actions" sekmesinde **MIMIC-VST3-Windows** olarak inir.

Arayüz Microsoft Edge WebView2 ile çizilir (Windows 10/11'de hazır gelir). Eksikse plugin bir uyarı gösterir, ses motoru yine çalışır.

## Proje yapısı

- `Source/dsp/` — ses motoru (JUCE'tan bağımsız): `Tuner.h` (YIN + PSOLA tune/formant), `Effects.h`, `Chain.h`, `Presets.h` (sanatçı tarifleri)
- `Source/Plugin*.cpp` — JUCE sarmalayıcı ve WebView köprüsü
- `ui/` — arayüz (HTML/CSS/JS). Tarayıcıda açınca demo modda çalışır
- `tests/dsp_test.cpp` — ses motoru testi: `g++ -O2 -std=c++17 -ISource tests/dsp_test.cpp -o dsp_test`

Yeni sanatçı eklemek için `Presets.h` içindeki `kArtists` listesine bir satır eklemen yeterli; arayüz ve parametre listesi kendiliğinden güncellenir.
