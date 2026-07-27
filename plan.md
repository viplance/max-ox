# MaxOx DSP roadmap

Цель: увеличить психоакустическую громкость при том же true-peak ceiling, не
ухудшая прозрачность, стереообраз и транзиенты. Каждая следующая стадия
принимается только после сравнения с предыдущей при одинаковых LUFS и dBTP.

## Стадия 1 — корректный limiter core

- [x] Выполнять нелинейную часть limiter в 8× oversampled domain.
- [x] Детектировать межсемпловые пики до расчёта gain reduction.
- [x] Заменить псевдо crest factor на отношение peak/RMS.
- [x] Сделать program-dependent release непрерывным между fast и slow limits.
- [x] Убрать финальный hard clamp на исходной sample rate.
- [x] Сообщать DAW полную latency: look-ahead плюс oversampling filters.
- [x] Предвычислить cosine attack window вне audio callback.
- [x] Добавить DSP regression tests для latency, silence и true-peak ceiling.

Критерий готовности: проект и тесты собираются; тестовый сигнал после limiter
не превышает ceiling более чем на допуск true-peak измерителя; reported latency
совпадает с фактической задержкой.

## Стадия 2 — прозрачность envelope и low-frequency control

- [x] Заменить O(sample-rate²) поиск по look-ahead окну на ограниченный по CPU
      scheduler/envelope без потери peak guarantee.
- [x] Сравнить linear-dB, cosine и двухступенчатые attack/release curves:
      выбран proactive exponential attack с due-target clamp и непрерывным
      program-dependent release.
- [x] Переделать AdaptiveLowCut в stereo-linked dynamic resonance control.
- [x] Разделить RMS state каналов и исключить дрейф stereo image.
- [x] Разрешать LF reduction только при измеримом уменьшении linked peak,
      используемого как proxy будущего limiter GR.
- [x] Добавить DC/infrasonic cleanup с минимальной фазовой окраской.

Результат: knee уменьшен с 3 до 0.5 dB, диапазон release сокращён с
25–400 до 18–120 ms, а непрерывный поиск по всему окну заменён событийным
cosine scheduler: полная кривая строится только при появлении нового, более
глубокого peak; устойчивые участки продлеваются за O(1).

Уточнение после прослушивания: DC/infrasonic cleanup заменён на Butterworth
high-pass четвёртого порядка с частотой среза 30 Hz и крутизной
24 dB/октаву. Максимальная динамическая коррекция полос 55–250 Hz снижена,
а её release сокращён с 80 до 50 ms. Для неглубокой gain reduction основной
limiter использует release 15 ms, true-peak guard — 12 ms; при глубоком
лимитировании сохраняются более медленные program-dependent времена.

## Стадия 3 — адаптивное фазовое уменьшение пиков

- [x] Реализовать общий для L/R all-pass phase rotator.
- [x] Оценивать несколько безопасных конфигураций на коротком окне.
- [x] Автоматически обходить обработку, если выигрыш peak меньше 0.3 dB.
- [x] Ограничить group delay и контролировать размытие транзиентов.
- [x] Проверить mono compatibility и устойчивость stereo image.

Результат: восемь четырёхсекционных all-pass кандидатов непрерывно
анализируются в общем для L/R 50 ms окне. Диапазоны 40–200 Hz и
r = 0.65–0.98 удерживают impulse response в исследованной ultra-short области.
Переключения защищены преимуществом 0.15 dB, minimum hold 750 ms и cosine
crossfade 20 ms. Regression test получает 2.29 dB peak reduction на
асимметричном сигнале, сохраняет энергию и идентичность каналов; чистый синус
автоматически остаётся в bypass.

## Стадия 4 — каскадный transient peak shaving

- [x] Добавить короткий oversampled soft-clip stage перед clean limiter.
- [x] Ограничить его работу transient-событиями и глубиной 0.5–1.5 dB.
- [x] Подбирать кривую по остаточному aliasing и слышимости искажений.
- [x] Разделять нагрузку между clipper и limiter по прогнозируемой цене артефактов.

Результат: shaver работает внутри существующего 8× тракта без дополнительной
latency. Peak/RMS crest и peak novelty отделяют транзиенты от sustained
материала; smooth exponential knee ограничен жёстким бюджетом 1.5 dB.
Оставшуюся работу всегда выполняет clean look-ahead limiter.

## Стадия 5 — психоакустическое управление

- [x] Реализовать ERB/Bark-анализ исходного и разностного сигналов.
- [x] Оценивать simultaneous и temporal masking.
- [x] Ослаблять peak shaving, когда residual выходит выше masking threshold.
- [x] Оставить консервативные ограничения для tonal и side-channel материала.

Результат: 16 ERB-spaced полос анализируют source и residual после
антиалиасингового усреднения 8× потока. Раздельные attack/release envelopes
моделируют simultaneous и forward masking. Noise-to-mask feedback непрерывно
управляет глубиной shaver; tonal concentration и side-energy дополнительно
уменьшают разрешённую нелинейность.

### Уточнение true-peak ceiling

Постоянный reconstruction margin заменён отдельным 8× true-peak guard после
downsampling. Основной limiter теперь использует ceiling −0.1 dB, а guard с
2 ms look-ahead компенсирует только фактический inter-sample overshoot.
Стресс-тест достигает −0.101 dBTP вместо прежних приблизительно −0.5 dB,
сохраняя true-peak guarantee. Дополнительная latency сообщается DAW.

## Стадия 6 — валидация и продуктовые режимы

- [ ] Собрать корпус музыки разных жанров и sample rates 44.1–192 kHz.
- [ ] Автоматически измерять LUFS-I/S, PLR, dBTP, GR, спектральную ошибку,
      stereo correlation, CPU и latency.
- [ ] Проверять overshoots после AAC/MP3 round-trip.
- [ ] Провести слепое сравнение малых деградаций по принципам ITU-R BS.1116.
- [ ] Зафиксировать режимы Transparent и High Quality и их CPU budgets.
