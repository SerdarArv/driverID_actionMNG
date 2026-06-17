# BÖLÜM 1 - GEREKSİNİM LİSTESİ

## 1.1 Genel

| ID | Gereksinim |
| --- | --- |
| GEN-01 | Blokaj kararı, mesaj gönderildiği anda geçerli olan block_option değerine göre alınır. Geçmiş strateji tarihi, mevcut kararı değiştirmez. |
| GEN-02 | ActionManager başlangıçta nötr bir strateji durumunda (NON_BLOCKING) olmalıdır. |
| GEN-03 | block_option konfigürasyonu değiştiğinde, mevcut araç ve sürücü durumu yeniden değerlendirilmeli ve relay durumu anlık güncellenmelidir. |

## 1.2 Lokal Blokaj

| ID | Gereksinim |
| --- | --- |
| LOC-01 | LOCAL_BLOCKING aktifken yetkili sürücü (ID_MATCHED) check-in yaparsa relay pasif edilir. |
| LOC-02 | LOCAL_BLOCKING aktifken yetkisiz sürücü kart okutarsa hiçbir check-in/checkout işlemi yapılmaz; relay aktif kalır. |
| LOC-03 | LOCAL_BLOCKING aktifken sürücü listesi bulunamazsa (LIST_NOT_FOUND) relay pasif edilir (fail-safe: listsiz bloke edilmez). |
| LOC-04 | LOCAL_BLOCKING aktifken aktif sürücü kart okutarak checkout yaparsa relay aktif edilir. |
| LOC-05 | Sürücü check-out yaparken kontak açıksa işlem reddedilir. |
| LOC-06 | Farklı bir sürücü kart okutursa mevcut sürücüye checkout yapılır, ardından yeni sürücü için check-in değerlendirmesi yapılır. |
| LOC-07 | Kontak açılırken aktif sürücü yoksa kırmızı LED yanar ve yapılandırılmış süre sonra buzzer çalar. |
| LOC-08 | AUTOMATIC_CHECKOUT aktif ve kontak kapanırsa, checkout_tmo saniye sonra otomatik checkout yapılır ve relay aktif edilir. |
| LOC-09 | LOCAL_BLOCKING + AUTOMATIC_CHECKOUT kombinasyonunda checkout_tmo minimum 15 saniye olmak zorundadır. |
| LOC-10 | LOCAL_BLOCKING konfigürasyonu aktif olduğu anda, mevcut sürücü ve kontak durumu değerlendirilir; sürücü yoksa relay aktif edilir. |

## 1.3 Remote Blokaj

| ID | Gereksinim |
| --- | --- |
| REM-01 | Sunucudan BLOCK_VEHICLE komutu geldiğinde araç hareket halindeyse şu koşullar sağlanana kadar beklenir: araç sabit (VEHICLE_IDLE), GPS fix, hız < 1.0 m/s, GSM bağlı. |
| REM-02 | Koşullar sağlanınca 30 saniyelik doğrulama süresi başlar; süre dolunca relay aktif edilir. |
| REM-03 | Araç motor kapalı durumdayken (VEHICLE_STABLE / VEHICLE_POWERED) BLOCK_VEHICLE komutu gelirse koşul beklenmeden relay aktif edilir. |
| REM-04 | Sunucudan UNBLOCK_VEHICLE komutu geldiğinde relay pasif edilir ve sunucuya başarı cevabı gönderilir. |
| REM-05 | Relay devre dışıyken (relay_available == false) block/unblock komutu gelirse COMMAND_FAIL dönülür. |
| REM-06 | Yeni block/unblock komutu gelirken önceki işlem bekleme durumundaysa iptal edilir (VEHICLE_COMMAND_CANCEL) ve yeni komut işlenir. |
| REM-07 | Remote stratejiyle uygulanan blokaj, yerel bir kart okutma işlemiyle kaldırılamaz. block_option LOCAL_BLOCKING'e geçmedikçe remote blokaj korunur. |

## 1.4 Backup ve Reset

| ID | Gereksinim |
| --- | --- |
| BAK-01 | Cihaz resetlendiğinde flash'taki blokaj durumu okunur ve relay aynı duruma getirilir. |
| BAK-02 | Reset öncesi bekleme durumunda kalınmışsa ilgili işlem yeniden başlatılır. |

## 1.5 Güç ve Relay

| ID | Gereksinim |
| --- | --- |
| PWR-01 | Relay konfigürasyonu devre dışı bırakılırsa (RELAY_PWR == false) aktif blokaj iptal edilir. |
| PWR-02 | Power Manager'ın tetiklediği UNBLOCK_WITHOUT_SAVE komutu relay'i pasif eder fakat flash güncellenmez. |

## 1.6 Sürücü Listesi

| ID | Gereksinim |
| --- | --- |
| DRV-01 | Sürücü listesi güncellenirken (silme + yeniden yazma arasında) blokaj durumu değiştirilmez. |
| DRV-02 | Tüm liste silindiğinde (DELETE_ALL), aktif lokal blokaj korunur; yeni bir check-in olmadan relay pasif edilmez. |

# BÖLÜM 2 - EKSİKLİKLER

## 2.1 Konfigürasyon Aktivasyon Sorunları

| Ref | İlgili Gereksinim | Eksiklik | Öncelik |
| --- | --- | --- | --- |
| E-01 | LOC-10, GEN-03 | LOCAL_BLOCKING config geldiğinde UpdateConfig() sonrası mevcut durum yeniden değerlendirilmiyor. Kart okutulana veya kontak değişene kadar relay güncellenmez. | Yüksek |
| E-03 | GEN-03 | block_option herhangi bir değere değiştiğinde relay durumu anlık güncellenmiyor. | Yüksek |
| E-04 | GEN-03 | LOCAL_BLOCKING -> NO_BLOCKING geçişinde araç bloke durumdaysa relay kart okutulana kadar aktif kalıyor. | Orta |

## 2.2 Strateji Yönetimi Sorunları

| Ref | İlgili Gereksinim | Eksiklik | Öncelik |
| --- | --- | --- | --- |
| E-02 | GEN-02 | blocking_strategy için nötr başlangıç değeri yok. Default 0 = LOCAL olduğundan sistem ilk açılışta hiç blokaj yapılmamışken kendini LOCAL stratejide sanabiliyor. | Yüksek |
| E-05 | REM-07 | LOCAL -> SERVER_BLOCKING geçişinde kart okutulursa NO_STOP_COMMAND gönderilir. ActionManager'da blocking_strategy == LOCAL koşulu hâlâ geçerli olduğundan relay gizlice pasif ediliyor. | Orta |
| E-07 | REM-07 | NO_BLOCKING veya SERVER_BLOCKING modunda kart okutulunca NO_STOP_COMMAND gönderilir; vehicle_blocking_state == VEHICLE_BLOCKED koşuluyla remote blokaj da kaldırılıyor. | Kritik |

## 2.3 Timeout Eksiklikleri

| Ref | İlgili Gereksinim | Eksiklik | Öncelik |
| --- | --- | --- | --- |
| E-08 | REM-04 | WAITING_ALARM_BLOCK_RESPONSE durumunda sunucudan cevap gelmezse sistem süresiz askıda kalıyor. | Orta |
| E-09 | REM-04 | WAITING_ALARM_UNBLOCK_RESPONSE durumunda sunucudan cevap gelmezse sistem süresiz askıda kalıyor. | Orta |

## 2.4 Sürücü Listesi Güvenliği

| Ref | İlgili Gereksinim | Eksiklik | Öncelik |
| --- | --- | --- | --- |
| E-10 | DRV-01 | Liste güncellenirken oluşan boşlukta kart okutulursa LIST_NOT_FOUND -> UNBLOCK mantığıyla relay pasif edilebilir. Mutex varlığı yeterli mi belirsiz. | Orta |
| E-11 | DRV-02 | DELETE_ALL sonrası sürücü checkout yaparsa LIST_NOT_FOUND -> LOCAL_UNBLOCK_VEHICLE gidip araç serbest bırakılıyor. | Kritik |
