import type {Locale} from './native/NativeAppSettings';

export const en = {
  tagline: 'Your pace. Your control.', heroHint: 'Ready for your next session.',
  language: 'Language', languageSave: 'Language changed for this session, but could not be saved. Try again.',
  ready: 'Ready when you are', running: 'Clicking', paused: 'Paused · target not focused',
  capturing: 'Focus your target now…', needsAttention: 'Stopped · needs attention', connecting: 'Connecting to click engine…',
  start: 'Start clicking', stop: 'Stop clicking', cancelCapture: 'Cancel capture', retryRelease: 'Retry button release',
  shortcuts: '1 starts · 2 stops · F6 toggles · Esc stops', perSecond: '/ sec', submitted: 'Submitted to Windows',
  session: 'This session', rateCaution: 'Applications may receive or process fewer clicks.',
  target: 'Target window', noTarget: 'No window captured', untitled: 'Untitled window', process: 'Process {pid}',
  captureDelay: 'Capture in 2 seconds', switchWindows: 'Switch windows now…', capture: 'Capture window', recapture: 'Recapture window',
  focusPause: 'Pauses when the target loses focus.', focusOff: 'Focus lock is off.',
  speed: 'Click speed', configured: '{rate} clicks/sec configured', intervalLabel: 'Click interval in milliseconds',
  sliderHint: 'Use arrow keys to change the interval by one millisecond. Home is slowest; End is fastest.',
  sliderValue: '{rate} clicks per second, {interval} milliseconds',
  mouseButton: 'Mouse button', left: 'Left', right: 'Right', middle: 'Middle',
  advanced: 'Advanced settings', press: 'Press duration', pressLabel: 'Press duration in milliseconds',
  pressHint: '0 ms submits down and up together. Longer presses reduce speed and must stay below the interval.',
  focusLock: 'Only click while target is focused', clearTarget: 'Clear captured window', appearance: 'Appearance',
  system: 'System', light: 'Light', dark: 'Dark', highTimer: 'High-resolution timer', standardTimer: 'Standard timer fallback',
  acrylic: 'Native Acrylic', solid: 'Solid backdrop', diagnostics: '{skipped} deadlines skipped · {failures} errors',
  shortcutHint: '1 starts only outside RoClick. 2, Escape and F6 work everywhere. Typing 2 also stops clicking.',
  performance: '1,000 clicks/sec is a performance target, not a real-time guarantee.',
  footer: 'Local only · No telemetry · No automatic updates',
  applyInterval: 'Apply a valid interval with Enter or Tab.', applyPress: 'Apply a valid press duration with Enter or Tab.',
  invalidInterval: 'Enter a whole interval from 1 to 1,000 ms.', invalidPress: 'Enter a whole press duration from 0 to {max} ms.',
  durationBelow: 'Set press duration below the new interval first.', windowsCode: 'Windows code {code}',
  error_invalidConfig: 'Use a whole interval of 1–1,000 ms and a press duration below the interval.',
  error_targetClosed: 'The captured window closed. Capture a target again before starting.',
  error_captureFailed: 'Could not capture this window. Focus another desktop app and capture again.',
  error_inputRejected: 'Windows rejected mouse input. Stop, check the target’s permissions or input restrictions, and retry. RoClick does not elevate automatically.',
  error_releaseRejected: 'Windows rejected a button release. Release the physical mouse button, check desktop/target permissions, then Stop to retry the release.',
  error_targetMissing: 'Capture a target first, or turn off target focus lock in Advanced.',
  error_monitoringUnavailable: 'Native input monitoring is unavailable. Restart RoClick on the interactive desktop.',
  error_timerUnavailable: 'Windows could not arm the scheduler timer. Restart RoClick.',
  error_schedulerWaitFailed: 'Windows scheduler wait failed. Restart RoClick.',
  error_unknown: 'The click engine encountered an error. Stop and restart RoClick.',
};
export type MessageKey = keyof typeof en;
export const tr: Record<MessageKey, string> = {
  tagline: 'Hız senin. Kontrol senin.', heroHint: 'Bir sonraki oturumuna hazır.',
  language: 'Dil', languageSave: 'Dil bu oturum için değiştirildi ancak kaydedilemedi. Yeniden deneyin.',
  ready: 'Hazır olduğunda başla', running: 'Tıklanıyor', paused: 'Duraklatıldı · hedef odakta değil',
  capturing: 'Şimdi hedef pencereye geç…', needsAttention: 'Durduruldu · işlem gerekiyor', connecting: 'Tıklama motoruna bağlanılıyor…',
  start: 'Tıklamayı başlat', stop: 'Tıklamayı durdur', cancelCapture: 'Seçimi iptal et', retryRelease: 'Tuşu bırakmayı yeniden dene',
  shortcuts: '1 başlatır · 2 durdurur · F6 aç/kapat · Esc durdurur', perSecond: '/ sn', submitted: 'Windows’a gönderilen',
  session: 'Bu oturum', rateCaution: 'Uygulamalar daha az tıklama alabilir veya işleyebilir.',
  target: 'Hedef pencere', noTarget: 'Pencere seçilmedi', untitled: 'Başlıksız pencere', process: 'İşlem {pid}',
  captureDelay: '2 saniye sonra seçilir', switchWindows: 'Şimdi pencere değiştir…', capture: 'Pencere seç', recapture: 'Yeniden pencere seç',
  focusPause: 'Hedef odaktan çıkınca duraklatılır.', focusOff: 'Odak kilidi kapalı.',
  speed: 'Tıklama hızı', configured: 'Ayarlanan hız: {rate} tıklama/sn', intervalLabel: 'Milisaniye cinsinden tıklama aralığı',
  sliderHint: 'Ok tuşları aralığı bir milisaniye değiştirir. Home en yavaş, End en hızlı ayardır.',
  sliderValue: 'Saniyede {rate} tıklama, {interval} milisaniye',
  mouseButton: 'Fare tuşu', left: 'Sol', right: 'Sağ', middle: 'Orta',
  advanced: 'Gelişmiş ayarlar', press: 'Basılı tutma süresi', pressLabel: 'Milisaniye cinsinden basılı tutma süresi',
  pressHint: '0 ms, basma ve bırakmayı birlikte gönderir. Uzun basışlar hızı düşürür; süre, tıklama aralığından kısa olmalıdır.',
  focusLock: 'Yalnızca hedef odaktayken tıkla', clearTarget: 'Seçili pencereyi temizle', appearance: 'Görünüm',
  system: 'Sistem', light: 'Açık', dark: 'Koyu', highTimer: 'Yüksek çözünürlüklü zamanlayıcı', standardTimer: 'Standart zamanlayıcı',
  acrylic: 'Yerel Akrilik', solid: 'Düz arka plan', diagnostics: '{skipped} zamanlama atlandı · {failures} hata',
  shortcutHint: '1 yalnızca RoClick dışındayken başlatır. 2, Escape ve F6 her yerde çalışır. Bir alana 2 yazmak da tıklamayı durdurur.',
  performance: 'Saniyede 1.000 tıklama bir performans hedefidir; gerçek zaman garantisi değildir.',
  footer: 'Yerel çalışır · Telemetri yok · Otomatik güncelleme yok',
  applyInterval: 'Geçerli aralığı Enter veya Tab ile uygula.', applyPress: 'Geçerli basılı tutma süresini Enter veya Tab ile uygula.',
  invalidInterval: '1–1.000 ms arasında tam sayı bir aralık gir.', invalidPress: '0–{max} ms arasında tam sayı bir basılı tutma süresi gir.',
  durationBelow: 'Önce basılı tutma süresini yeni aralığın altına indir.', windowsCode: 'Windows kodu {code}',
  error_invalidConfig: '1–1.000 ms arasında tam sayı bir aralık ve aralıktan kısa bir basılı tutma süresi kullan.',
  error_targetClosed: 'Seçili pencere kapandı. Başlatmadan önce yeniden pencere seç.',
  error_captureFailed: 'Bu pencere seçilemedi. Başka bir masaüstü uygulamasına geçip yeniden seç.',
  error_inputRejected: 'Windows fare girişini reddetti. Durdur, hedefin izinlerini veya giriş kısıtlamalarını kontrol edip yeniden dene. RoClick kendiliğinden yönetici yetkisi istemez.',
  error_releaseRejected: 'Windows fare tuşunu bırakma işlemini reddetti. Fiziksel fare tuşunu bırak, masaüstü/hedef izinlerini kontrol et ve yeniden bırakmayı denemek için Durdur’a bas.',
  error_targetMissing: 'Önce hedef pencere seç veya Gelişmiş ayarlardan hedef odak kilidini kapat.',
  error_monitoringUnavailable: 'Yerel giriş izleme kullanılamıyor. RoClick’i etkileşimli masaüstünde yeniden başlat.',
  error_timerUnavailable: 'Windows zamanlayıcıyı başlatamadı. RoClick’i yeniden başlat.',
  error_schedulerWaitFailed: 'Windows zamanlama beklemesi başarısız oldu. RoClick’i yeniden başlat.',
  error_unknown: 'Tıklama motorunda hata oluştu. RoClick’i durdurup yeniden başlat.',
};

export function translate(locale: Locale, key: MessageKey, values: Record<string, string | number> = {}) {
  return (locale === 'tr' ? tr : en)[key].replace(/\{(\w+)\}/g, (_, name: string) => String(values[name] ?? `{${name}}`));
}
export function formatNumber(value: number, locale: Locale, decimals = 0): string {
  const [whole, fraction] = value.toFixed(decimals).split('.');
  const grouped = whole.replace(/\B(?=(\d{3})+(?!\d))/g, locale === 'tr' ? '.' : ',');
  return fraction === undefined ? grouped : grouped + (locale === 'tr' ? ',' : '.') + fraction;
}
export function formatRate(interval: number, locale: Locale): string {
  const rate = Number((1000 / interval).toFixed(1));
  return formatNumber(rate, locale, Number.isInteger(rate) ? 0 : 1);
}
