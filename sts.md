# BÖLÜM 3 - TEST SENARYOLARI

**Ön koşul (tüm testler için):** Cihaz yapılandırılmış, relay bağlı, sürücü listesi temiz.

## T-LOC-01 Yetkili Sürücü Check-in

**Hazırlık:** block_option = LOCAL_BLOCKING, listede en az 1 yetkili ID kayıtlı, relay başlangıçta aktif (araç bloke).

**Adımlar:**
- Yetkili sürücü kartını okut.
- Kontak durumunun IO_PASSIVE olduğunu doğrula.
- LED yeşil yandığını doğrula.
- Relay pasif olduğunu (RELAY_PASSIVE mesajı gönderildi) doğrula.
- Driver status mesajında DRIVER_CHECKIN ve doğru ID gittiğini doğrula.

**Beklenen:** Relay pasif, LED yeşil, sunucuya DRIVER_CHECKIN mesajı.

## T-LOC-02 Yetkisiz Sürücü Kart Okutma

**Hazırlık:** block_option = LOCAL_BLOCKING, listede tanımsız bir ID okutulacak.

**Adımlar:**
- Listede olmayan kartı okut.
- Herhangi bir check-in/checkout mesajı gitmediğini doğrula.
- Relay durumunun değişmediğini (aktif kalıyor) doğrula.
- Kontak IO_ACTIVE ise buzzer çaldığını doğrula.

**Beklenen:** Hiçbir durum değişikliği yok, kontak açıksa buzzer aktif.

## T-LOC-03 Liste Bulunamadığında Fail-safe

**Hazırlık:** block_option = LOCAL_BLOCKING, sürücü listesi dosyası silinmiş.

**Adımlar:**
- Herhangi bir kart okut.
- LIST_NOT_FOUND döndüğünü logdan doğrula.
- Relay pasif edildiğini doğrula.

**Beklenen:** Relay pasif (listsiz bloke edilmez).

## T-LOC-04 Aktif Sürücü Manuel Checkout

**Hazırlık:** Yetkili sürücü check-in'de, kontak IO_PASSIVE.

**Adımlar:**
- Aynı kartı tekrar okut.
- Checkout mesajı (DRIVER_CHECKOUT) gönderildiğini doğrula.
- Relay aktif olduğunu doğrula.
- LED kırmızı yandığını doğrula.

**Beklenen:** Relay aktif, LED kırmızı, checkout mesajı.

## T-LOC-05 Sürüş Sırasında Checkout Engeli

**Hazırlık:** Aktif sürücü var, kontak IO_ACTIVE.

**Adımlar:**
- Aktif sürücünün kartını okut.
- Checkout işlemi yapılmadığını doğrula.
- Relay durumunun değişmediğini doğrula.

**Beklenen:** İşlem yok, log: "CheckOUT not suitable while driving".

## T-LOC-06 Farklı Sürücü Geçişi

**Hazırlık:** Yetkili sürücü A check-in'de, yetkili sürücü B listede.

**Adımlar:**
- Sürücü B'nin kartını okut.
- Sürücü A için DRIVER_CHECKOUT mesajı gönderildiğini doğrula.
- Sürücü B için DRIVER_CHECKIN mesajı gönderildiğini doğrula.
- Relay pasif kaldığını doğrula.

**Beklenen:** A checkout, B checkin, relay değişmez.

## T-LOC-07 Kontak Açıkken Sürücüsüz Uyarı

**Hazırlık:** block_option = LOCAL_BLOCKING, sürücü yok, kontak IO_ACTIVE.

**Adımlar:**
- Kontağı aç (simüle et veya fiziksel).
- LED kırmızı yandığını doğrula.
- buzzer_start_tmo saniye sonra buzzer çaldığını doğrula.

**Beklenen:** Kırmızı LED + buzzer.

## T-LOC-08 Otomatik Checkout

**Hazırlık:** block_option = LOCAL_BLOCKING, checkout_option = AUTOMATIC_CHECKOUT, checkout_tmo = 20 sn, sürücü check-in'de.

**Adımlar:**
- Kontağı kapat.
- 20 saniye bekle.
- Otomatik DRIVER_CHECKOUT mesajı gönderildiğini doğrula.
- Relay aktif olduğunu doğrula.

**Beklenen:** 20 sn sonra checkout, relay aktif.

## T-LOC-09 Minimum Checkout Süresi (Deadlock Koruması)

**Hazırlık:** block_option = LOCAL_BLOCKING, checkout_option = AUTOMATIC_CHECKOUT, checkout_tmo = 5 sn (15 sn altı).

**Adımlar:**
- UpdateConfig() çağrıldıktan sonra m_config_diu_base.checkout_tmo değerini logdan doğrula.
- Değerin 15 saniyeye zorlandığını doğrula.

**Beklenen:** checkout_tmo = 15.

## T-REM-01 Hareket Halindeyken Block Komutu

**Hazırlık:** block_option = SERVER_BLOCKING, araç hareket halinde (speed > 1.0), GSM bağlı, GPS fix.

**Adımlar:**
- Sunucudan BLOCK_VEHICLE komutu gönder.
- Relay'in hemen aktif olmadığını doğrula.
- Durum WAITING_FOR_VEHICLE_BLOCK_CONDITIONS olduğunu doğrula.
- Araç durdur (speed < 1.0, koşullar tamam).
- 30 saniye bekle.
- Relay aktif olduğunu doğrula.
- Sunucuya VEHICLE_BLOCK_OK gönderildiğini doğrula.

**Beklenen:** Koşullar sağlanınca 30 sn sonra relay aktif.

## T-REM-02 Motor Kapalıyken Block Komutu

**Hazırlık:** vehicle_status = VEHICLE_STABLE, block_option = SERVER_BLOCKING.

**Adımlar:**
- Sunucudan BLOCK_VEHICLE komutu gönder.
- Koşul beklenmeksizin relay'in anında aktif olduğunu doğrula.

**Beklenen:** Anında relay aktif.

## T-REM-03 Unblock Komutu

**Hazırlık:** Araç remote bloke, vehicle_blocking_state = VEHICLE_BLOCKED.

**Adımlar:**
- Sunucudan UNBLOCK_VEHICLE komutu gönder.
- Relay pasif olduğunu doğrula.
- Sunucuya VEHICLE_UNBLOCK_OK gönderildiğini doğrula.

**Beklenen:** Relay pasif, sunucuya başarı cevabı.

## T-REM-04 Relay Devre Dışıyken Block Komutu

**Hazırlık:** RELAY_PWR = false (relay konfigürasyonu kapalı).

**Adımlar:**
- Sunucudan BLOCK_VEHICLE komutu gönder.
- COMMAND_FAIL dönüldüğünü doğrula.
- Relay durumunun değişmediğini doğrula.

**Beklenen:** COMMAND_FAIL, relay değişmez.

## T-REM-05 Beklemedeyken Yeni Komut (İptal)

**Hazırlık:** Araç WAITING_FOR_VEHICLE_BLOCK_CONDITIONS durumunda.

**Adımlar:**
- Sunucudan UNBLOCK_VEHICLE komutu gönder.
- VEHICLE_COMMAND_CANCEL gönderildiğini doğrula.
- Yeni unblock işleminin başladığını doğrula.

**Beklenen:** Önceki iptal, yeni komut işlenir.

## T-REM-06 Remote Blokaj Yerel Kart Okutmayla Kaldırılamaz

**Hazırlık:** Araç remote bloke (blocking_strategy = REMOTE, vehicle_blocking_state = VEHICLE_BLOCKED), block_option = SERVER_BLOCKING.

**Adımlar:**
- Herhangi bir kart okut.
- NO_STOP_COMMAND gönderildiğini logdan doğrula.
- Relay'in pasif edilmediğini doğrula (blokaj korunur).

**Beklenen:** Relay aktif kalır.

> Uyarı: Bu test E-07 eksiğini doğrular. Şu an başarısız olması beklenir.

## T-BAK-01 Reset Sonrası Bloke Durumu Korunuyor

**Hazırlık:** Araç VEHICLE_BLOCKED durumdayken cihaz resetlenir.

**Adımlar:**
- Cihazı resetle.
- Flash'tan vehiclestop.h okunduğunu logdan doğrula.
- Relay otomatik aktif edildiğini doğrula.

**Beklenen:** Reset sonrası blokaj devam eder.

## T-BAK-02 Reset Sonrası Bekleme Durumu Yeniden Başlar

**Hazırlık:** Cihaz WAITING_FOR_VEHICLE_BLOCK_CONDITIONS durumundayken resetlenir.

**Adımlar:**
- Cihazı resetle.
- ChangeVehicleStopState(BLOCK_VEHICLE) çağrıldığını logdan doğrula.
- Blokaj koşullarının yeniden beklemeye alındığını doğrula.

**Beklenen:** İşlem kaldığı yerden devam eder.

## T-PWR-01 Relay Konfigürasyonu Kapatılınca Blokaj İptali

**Hazırlık:** Araç lokal bloke, RELAY_PWR = true.

**Adımlar:**
- RELAY_PWR = false olacak şekilde config güncelle.
- Relay pasif edildiğini doğrula.
- vehicle_blocking_state = NO_STOP_ACTION olduğunu doğrula.

**Beklenen:** Relay pasif, blokaj iptal.

## T-DRV-01 Liste Silinince Blokaj Korunuyor

**Hazırlık:** LOCAL_BLOCKING, araç bloke, sürücü check-in'de.

**Adımlar:**
- DELETE_ALL ile listeyi sil.
- Aktif sürücüyü checkout yaptır.
- Relay'in pasif edilmediğini doğrula.

**Beklenen:** Relay aktif kalır.

> Uyarı: Bu test E-11 eksiğini doğrular. Şu an başarısız olması beklenir.

<div style="page-break-before: always;"></div>

# BÖLÜM 4 - TEST CHECKLIST (YAZDIRMA SAYFASI)

<style>
@page {
  size: A4 portrait;
  margin: 8mm;
}

@media print {
  body {
    margin: 0;
  }

  .print-checklist {
    width: 100%;
  }

  .print-checklist h1 {
    margin: 0 0 4mm 0;
    font-size: 14pt;
  }

  .print-checklist table {
    width: 100%;
    border-collapse: collapse;
    table-layout: fixed;
    font-size: 10pt;
  }

  .print-checklist th,
  .print-checklist td {
    border: 1px solid #000;
    padding: 1.2mm;
    vertical-align: top;
  }

  .print-checklist th {
    text-align: left;
  }

  .print-checklist td {
    height: 10.5mm;
  }
}

.print-checklist {
  width: 100%;
}

.print-checklist table {
  width: 100%;
  border-collapse: collapse;
  table-layout: fixed;
  font-size: 10pt;
}

.print-checklist th,
.print-checklist td {
  border: 1px solid #000;
  padding: 1.2mm;
  vertical-align: top;
}

.print-checklist td {
  height: 10.5mm;
}
</style>

<div class="print-checklist">

<table>
  <colgroup>
    <col style="width:12%">
    <col style="width:36%">
    <col style="width:8%">
    <col style="width:12%">
    <col style="width:32%">
  </colgroup>
  <thead>
    <tr>
      <th>Test ID</th>
      <th>Konu</th>
      <th>Durum</th>
      <th>Tarih</th>
      <th>Notlar</th>
    </tr>
  </thead>
  <tbody>
    <tr><td>T-LOC-01</td><td>Yetkili sürücü check-in -&gt; relay pasif</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-02</td><td>Yetkisiz sürücü -&gt; işlem yok</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-03</td><td>Liste yok -&gt; fail-safe unblock</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-04</td><td>Manuel checkout -&gt; relay aktif</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-05</td><td>Sürüş sırasında checkout engeli</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-06</td><td>Farklı sürücü geçişi</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-07</td><td>Sürücüsüz kontak açma -&gt; buzzer</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-08</td><td>Otomatik checkout</td><td></td><td></td><td></td></tr>
    <tr><td>T-LOC-09</td><td>Minimum checkout süresi (15 sn)</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-01</td><td>Hareket halindeyken block</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-02</td><td>Motor kapalıyken block</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-03</td><td>Unblock komutu</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-04</td><td>Relay kapalıyken block -&gt; FAIL</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-05</td><td>Beklemedeyken yeni komut</td><td></td><td></td><td></td></tr>
    <tr><td>T-REM-06</td><td>Remote blokaj yerel kartla kaldırılamaz</td><td></td><td></td><td>E-07 - şu an FAIL beklenir</td></tr>
    <tr><td>T-BAK-01</td><td>Reset -&gt; blokaj devam</td><td></td><td></td><td></td></tr>
    <tr><td>T-BAK-02</td><td>Reset -&gt; bekleme yeniden başlar</td><td></td><td></td><td></td></tr>
    <tr><td>T-PWR-01</td><td>Relay config kapatılınca blokaj iptal</td><td></td><td></td><td></td></tr>
    <tr><td>T-DRV-01</td><td>Liste silinince blokaj korunur</td><td></td><td></td><td>E-11 - şu an FAIL beklenir</td></tr>
  </tbody>
</table>

</div>
