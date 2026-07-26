# MaxOx DSP roadmap

Цель: увеличить психоакустическую громкость при том же true-peak ceiling, не
ухудшая прозрачность, стереообраз и транзиенты. Каждая следующая стадия
принимается только после сравнения с предыдущей при одинаковых LUFS и dBTP.

## Стадия 1 — корректный limiter core

- [x] Выполнять нелинейную часть limiter в 4× oversampled domain.
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

- [ ] Заменить O(sample-rate²) поиск по look-ahead окну на ограниченный по CPU
      scheduler/envelope без потери peak guarantee.
- [ ] Сравнить linear-dB, cosine и двухступенчатые attack/release curves.
- [ ] Переделать AdaptiveLowCut в stereo-linked dynamic resonance control.
- [ ] Разделить RMS state каналов и исключить дрейф stereo image.
- [ ] Разрешать LF reduction только при измеримом уменьшении будущего limiter GR.
- [ ] Добавить DC/infrasonic cleanup с минимальной фазовой окраской.

## Стадия 3 — адаптивное фазовое уменьшение пиков

- [ ] Реализовать общий для L/R all-pass phase rotator.
- [ ] Оценивать несколько безопасных конфигураций на look-ahead окне.
- [ ] Автоматически обходить обработку, если выигрыш peak меньше 0.3 dB.
- [ ] Ограничить group delay и контролировать размытие транзиентов.
- [ ] Проверить mono compatibility и устойчивость stereo image.

## Стадия 4 — каскадный transient peak shaving

- [ ] Добавить короткий oversampled soft-clip stage перед clean limiter.
- [ ] Ограничить его работу transient-событиями и глубиной 0.5–1.5 dB.
- [ ] Подбирать кривую по остаточному aliasing и слышимости искажений.
- [ ] Разделять нагрузку между clipper и limiter по прогнозируемой цене артефактов.

## Стадия 5 — психоакустическое управление

- [ ] Реализовать ERB/Bark-анализ исходного и разностного сигналов.
- [ ] Оценивать simultaneous и temporal masking.
- [ ] Ослаблять peak shaving, когда residual выходит выше masking threshold.
- [ ] Оставить консервативные ограничения для tonal и side-channel материала.

## Стадия 6 — валидация и продуктовые режимы

- [ ] Собрать корпус музыки разных жанров и sample rates 44.1–192 kHz.
- [ ] Автоматически измерять LUFS-I/S, PLR, dBTP, GR, спектральную ошибку,
      stereo correlation, CPU и latency.
- [ ] Проверять overshoots после AAC/MP3 round-trip.
- [ ] Провести слепое сравнение малых деградаций по принципам ITU-R BS.1116.
- [ ] Зафиксировать режимы Transparent и High Quality и их CPU budgets.

