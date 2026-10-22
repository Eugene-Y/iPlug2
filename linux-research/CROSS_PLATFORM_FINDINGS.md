# Найденные НЕ-Linux баги (касаются mac/win) — решения за автором

Источник: задача T-003 (сборка Gneiss под Linux), три независимых ревью в ветке `linux/T-003`,
`linux-research/evidence/t003/reviews/2-findings-review.md`. В рамках порта они **не исправлялись**:
порт не меняет бизнес-логику и поведение на других платформах (AGENTS §2).

| ID | Серьёзность | Где | Суть | Затрагивает mac/win? |
|---|---|---|---|---|
| **C1** ✅ исправлено (hvoya 3474f3d55, hvoya-linux e7c882995) | **Высокая** | `hvoya/utils/audio_buffer.cpp:210-214` (`AudioBuffer::reserve`), вызывающий — `Gneiss.cpp:189→1767` (`_osBuffer`) | `reserve` перевыделяет память, но не перепривязывает указатели каналов → **порча кучи**. Срабатывает, если первый оверсемплированный блок стерео и `блок × OS-фактор = 512` (256@2x, 128@4x, 64@8x) — например, переключение на 2x при буфере 256 или загрузка проекта с такой настройкой. Подтверждено тестом на маке (clang) и ASan на Linux | **Да** |
| **X1** ✅ исправлено (T-025; hvoya-linux 1e0e4be70, hvoya 8ca9f3324; одобрено автором 2026-10-19) | Средняя (узкое окно, но краш; доказано ASan/TSan стресс-тестом T-025: 10/10 до, 0/10 после. Падения release Gneiss x86_64 в VST3-валидаторе (T-021: 5/40) в T-025 не воспроизвелись, 0/40, так что их связь с X1 — гипотеза) | `IPlugProcessor::SetLatency` → `NChanDelayLine::SetDelayTime` (`NChanDelay.h:26-31`) | Длина задержки меняется и буфер переаллоцируется без синхронизации, а аудиопоток читает его в `PassThroughBuffers`, когда плагин в собственном bypass (VST3/AU/AAX). Смена OS-фактора (→ латентность) во время собственного bypass может упасть | **Да** (upstream iPlug) |
| **C3** ✅ исправлено (T-028, Gneiss `b600156`; master `c6f4442`, linux-experiment `f799cb3`) | Низкая–средняя | `Gneiss.cpp:~870` (downsample в `input`, mix/gain, `input += _dryBuf`) | Gneiss пишет во **входной** буфер хоста. В 64-bit (double) режиме CLAP/VST3 iPlug отдаёт указатели хоста напрямую → у хостов с out-of-place обработкой портится их вход. При любом OS-факторе | **Да** (64-bit хосты) |
| **C2** ✅ исправлено автором 2026-10-16 (Gneiss 3cf2e2e `setParamAppliedByCaller` + `isTransientWorkspaceValue`, hvoya 0b2d8dbef; отдельно не перепроверялось) | Средняя (UX) | `morph_controller.hpp:~1980` (`applyMorphModeParams`), дедуп `_prevParamVals` (`Gneiss.cpp:1940`) | После загрузки пресета кнопка MORPH требует лишних кликов: прямой `IParam::Set(0)` не обновляет кэш дедупликации; фабричные пресеты несут `morph_active=1`. Плюс на мгновение `_active=true` во время восстановления | **Да** (все форматы) |
| **C4** | Средняя | `IPlugVST3.h:62` (`InformHostOfPresetChange` пустой), preset strip `>` | После загрузки пресета через полосу пресетов VST3-хост видит старые значения параметров (кэш контроллера не обновляется); CLAP-хосты с кэшем тоже. Фикс: `DirtyParametersFromUI()` после загрузки в PresetManager | **Да** |
| **L1** | Неизвестна на маке | `IGEditorDelegate` размер редактора; `IPlugCLAP.cpp:972`, `IPlugVST3_View.h:76` | Gneiss создаёт редактор сразу нужной высоты (morph on / scale≠1), а сохранённый размер делегата не обновляется → CLAP/VST3 сообщают хосту устаревший размер. На Linux обойдено (Linux-only), на маке зависит от хоста (Cubase/Live берут размер из getSize) | Возможно |
| **C5** ✅ исправлено (T-028, Gneiss `36e1712` и `0f912c3` тексты, `5b31aa0` хвост 15 с по решению автора; master `c6f4442`) | Низкая | `IPlugParameter.cpp:397` (`StringToValue` = `atof`), `hzDisplay` | Ввод «1.06 kHz» → 1.06 Hz; «50%» → клип до 1. clap-validator `param-conversions` падает. Также Gneiss не объявляет tail → `process-sleep-process-status` | Да (ввод текста в хосте) |
| **X3** | Низкая | `IPlugAPP.cpp` `#import <Cocoa/Cocoa.h>` (hvoya cf2b2a1f4) | Пример IPlugEffect APP на маке не собирается (компилируется как C++). Gneiss не задет | Только пример |

Также (Linux-only, к сведению): GLIBC floor у Gneiss 2.35 из-за `hypot/hypotf` (можно прибить к 2.34 через `.symver`).

## Что предлагаю
- **C1 — исправить сразу** в `hvoya` (однострочный-двухстрочный фикс: перепривязать каналы в `reserve`), с юнит-тестом.
- **X1** — исправлено и перенесено в `hvoya` (T-025).
- **C3, C2, C4** — правки в Gneiss/hvoya PresetManager; лучше отдельной задачей на mac с тестом в DAW.
- **L1** — проверить в Cubase/Live на маке.

## Из T-005 (`Examples/duplicate.py`, upstream; касается mac/win)
| ID | Серьёзность | Суть |
|---|---|---|
| T5-1 | Низкая–средняя | `duplicate.py <ex> <new> <mfr> <путь вне дерева iPlug2>` оставляет в `CMakeLists.txt` `IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."`. `newroot` считается относительно `config/`, а заменяется только значение `IPLUG2_ROOT` в xcconfig. CMake-сборка такого дубликата не работает ни на одной платформе. Пользователь разрешил исправить в T-011 (2026-10-16). Повтор: `python3 duplicate.py IPlugEffect MyFx Mfr /tmp/x; grep IPLUG2_DIR /tmp/x/MyFx/CMakeLists.txt` |

## Из T-008 (IPopupMenuControl, upstream iPlug; касается mac/win только у плагинов со своим `AttachPopupMenuControl`)
| ID | Серьёзность | Суть |
|---|---|---|
| T8-1 | Низкая | Меню, запрошенное во время затухания предыдущего, игнорируется (на Linux исправлено только для стенд-инов по клику) |
| T8-2 | Низкая | Меню, открытое из completion-колбэка другого меню, теряет владельца |
| T8-3 | Низкая | `IPopupMenuControl` держит max bounds от первого меню и после ресайза UI |
Gneiss на mac/win использует нативные меню — его не касается. Подробности: `tasks/T-008-p3-polish.md`.

## Из T-006 (CLAP state, общий код `IPlug/CLAP/IPlugCLAP.cpp`) — исправлено в `linux/T-006`, ждёт решения автора
| ID | Серьёзность | Суть | Затрагивает mac/win? |
|---|---|---|---|
| T6-1 ✅ одобрено автором 2026-10-17 | Средняя | `stateLoad` восстанавливает параметры, но не зовёт `host->params->rescan(CLAP_PARAM_RESCAN_VALUES)`. Хосты с кэшем значений (общий UI параметров, полосы автоматизации) после открытия проекта / загрузки пресета хостом показывают старые значения. clap-validator `state-reproducibility-basic/-binary/-buffered` падают у всех iPlug2 CLAP, в том числе у установленного mac Gneiss. Фикс 46d10516c: rescan после `OnRestoreState()`; после него все три теста проходят на Linux и mac (IPlugEffect, IPlugControls, IPlugInstrument, Gneiss). Изменение на mac/win: после загрузки состояния хост перечитывает значения параметров; автоматизация из этого не пишется (так по спеке). Доказательства: `tasks/T-006-clap-state-repro.md`, `evidence/t006/` | **Да** (все CLAP) |
Рядом: CLAP не переопределял `InformHostOfPresetChange`. iPlug зовёт его только из загрузчиков FXP/FXB (`LoadStateFromFXP`/`LoadBankFromFXB` …), не из `RestorePreset`. Предложение T23-1 ниже (T-023).

## Из T-004 (standalone APP, общий код `IPlug/APP/IPlugAPP_host.cpp`) — исправлено с согласия автора
Меняют поведение mac/win только в сбойных случаях, нормальная работа не меняется. В `hvoya-linux`; в `hvoya` попадут при будущем слиянии. **Перенесено в `hvoya` 2026-10-20 по команде автора:** b304a5a3c, 41c689868, de16d8c8c; там же 593293cfb (CLAP `paramsValueToText`: запись на байт за буфер, коммит из основы порта).
| ID | Коммит | Суть |
|---|---|---|
| A2 | 013328ba4 | `GetMIDIPortNumber`: разыменование null, если RtMidi не инициализировался, а в settings.ini имя порта ≠ "off" |
| A3 | 013328ba4 | `ProbeAudioIO` держал ID устройств по умолчанию от прежнего драйвера (Windows DS↔ASIO тоже) |
| A1 | 7cce657a1 | `TryToChangeAudio`: null `mDAC`, если `Init()` остановился рано (нет папки настроек) |
| A4 | 7cce657a1 | Плагин без входов (инструмент) требовал входное устройство («Please check the audio settings» на машине без микрофона) |
| m7 | bb004604a | При отсутствии входного устройства список частот дискретизации был пуст; теперь частоты выхода |

## Из T-012 (Sand и общий код hvoya; касается mac/win) — не исправлялось, решения за автором
Подробности и ссылки на строки: `tasks/T-012-sand-linux-build.md`, раздел Log.
| ID | Серьёзность | Суть |
|---|---|---|
| **F1** ✅ исправлено (T-024, Sand `e8cb1f0`) | **Высокая** | Sand обрабатывает блоки из 0 сэмплов: VST3-хосты зовут `process` с `numSamples = 0` (сброс параметров), `SandModule::process` читает `chan.back()/front()`, а `hvoya::utils::derivativeInplace` пишет в `data.front()` пустого span — выход за границы буферов хоста (ASan: heap-buffer-overflow / use-after-free; VST3-валидатор падает в 1–4 из 10 прогонов). Фикс: ранний выход из `Sand::ProcessBlock` при `nFrames == 0` и/или `derivativeInplace` терпит пустой span |
| **F2** ✅ исправлено (T-024: iPlug2 `fix/midi-cc-host-notify` 17765295e + 8f40400b4, Sand `cdad970`; Gneiss `4dc02eb`; DMT/Melter/Crust_new — строка в OnIdle незакоммиченной правкой в worktree `hvoya/<имя>-t024`, Scape — вручную) | Средняя | MIDI CC → значение параметра не доходит до VST3-хоста: `hvoya/utils/midi_cc/mediator.hpp` из аудиопотока зовёт `BeginInformHostOfParamChange`/`SendParameterValueFromDelegate` — в VST3 это вызовы `IComponentHandler` вне UI-потока (нарушение правил VST3), REAPER видит старое значение. Общий код hvoya: Gneiss/Scape/DMT тоже. Родственно C4 |
| **F3** ✅ исправлено (T-024: Sand `da0e4bc`, iPlug2 `fix/hilbert-max-coefs` a29b8dfd7). Серьёзность поднята до высокой: NaN при 88.2/96 кГц с os 8x | Низкая | NaN на выходе при 768 кГц с параметрами по умолчанию (clap-validator `process-varying-sample-rates`) |
| F4 ✅ исправлено (T-024: iPlug2 `fix/param-text-to-value` 32d7a3ac5 + cd9e8922f, Sand `e9ea624`) | Низкая | Текст `flt hz` не парсится обратно ('1.01 kHz' → 1.0 Hz), как C5 у Gneiss |
| F5 ✅ исправлено (T-024, Sand `00cbc41`) | Низкая–средняя | Sand пишет во входной буфер хоста (как C3 у Gneiss): портит вход у out-of-place хостов в 64-bit |
| **F7** ✅ исправлено (T-024, Sand `87beabb`; на Windows не собрано) | Средняя | **Windows-проекты Sand, вероятно, не линкуются:** с 6036dec (2026-05-13) детекторы огибающей hvoya зовут `hiir::PolyphaseIIR2Designer::compute_coefs`, Xcode-проект компилирует `PolyphaseIIR2Designer.cpp`, ни один `projects/*.vcxproj` — нет (на Windows не проверено) |
| F8 ✅ исправлено (T-024, Sand `aed6804`; причина другая, см. ниже) | Низкая | `xcodebuild` Sand из командной строки (и на master) падает: `plog/Log.h` не найден, `$(HVOYA_PATH)/**` доходит до clang нераскрытым. Важно для CI/скриптов |

**Влито 2026-10-17** (по команде автора): iPlug2 `hvoya` — a865ee9f7 (F3), 228f2efc2 (F4), 083bc87c5 (F2), fa98170e8 (cherry-pick ef002e167, CLAP-текст для шаговых параметров); Sand `master` — 54be993 (все F); Gneiss `master` — 4dc02eb (строка `_midiCCMediator.OnIdle()`). DMT, Melter, Crust_new, Scape: строку `OnIdle` автор добавляет сам (DMT и Melter и так не собираются с текущим hvoya; в Crust_new незакоммиченные правки). Не запушено.

Итоги T-024 (подробности, доказательства и предложения — `tasks/T-024-sand-cross-platform-fixes.md`, Log):
- F1: кроме падений, каждый пустой блок давал щелчок (до 0.31 при пике 0.69).
- F3: две причины, обе — порча памяти на высоких частотах модуля (частота хоста × оверсэмплинг):
  - **`hvoya/utils/hilbert_envelope_detector.hpp`**: выше 384 кГц нужно больше коэффициентов, чем `kMaxCoefs = 14`; проверка только `assert`. Отсюда NaN. В реальности бывает при 88.2/96 кГц с os 8x, 176.4/192 кГц с os 4x/8x. **Исправлено:** `kMaxCoefs = 18`; проектирование коэффициентов не менялось.
  - `dmt_grind.hpp` (Sand): хранилище рассчитано на 3.07 МГц. **Исправлено** (`da0e4bc`).
- F8: причина не в «нераскрытом `/**`». WIP cd9f063 убрал `/**` только у таргета CLAP — он не собирался и в IDE; VST3/APP собирались.
- Новое, не исправлялось:
  - **N1:** `SandModule::process` выделяет память в аудиопотоке (`AudioBuffer dryCopy (processed)` на каждый блок).
  - **N2:** свежий клон Sand не собирается на маке: macOS `Info.plist` убраны из git (9d0c8ea), а `prepare_resources-mac.py` их только обновляет. → вероятно, закрыто коммитом автора 961be6c «puts back plists» (plist'ы снова в git).
  - **N3:** `Sand-aax.vcxproj` устарел целиком (нет `SandUI.cpp` и исходников hvoya).
  - **N4:** краш в FL Studio из BACKLOG — кандидаты F1/F3, не проверено.
  - На маке `param-conversions` падал на enum-параметрах ('0.0202' → '0' → 'x1'). Исправлено: hvoya-linux `ef002e167`, hvoya `fa98170e8`.

## Из T-026 (Sand и общий код hvoya; касается mac/win) — подробности и доказательства: `tasks/T-026-sand-followups.md`, Log
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| **N1** | Средняя | `SandModule::process` выделял память в аудиопотоке: копия `dryCopy` на каждом блоке, плюс рост `_osBuffer`, оверсэмплера и скретч-буферов на первом блоке и при каждой смене OS. Счётчик malloc внутри `process()` (Linux CLAP, 47 конфигураций): до — 1 на блок + 12–24 на первом, после — 0 везде | ✅ Sand `9c2dad2` (`fix/t026-followups`); звук побитово тот же (Linux 47, mac 46 рендеров), CPU без изменений |
| N2 | — | Свежий клон Sand на маке | ✅ закрыто коммитом автора 961be6c: CLAP/VST3/AUv2/APP собираются из клона |
| N3 | Средняя (только AAX/win) | `Sand-aax.vcxproj` был шаблоном IPlugEffect: нет `SandUI.cpp`, hvoya, HIIR | ✅ список файлов `cc81d9d`, настройки C++23 + prebuild `plug_build_info` `c073716` (одобрено автором); Sand `master` 0b71747. На Windows не собрано |
| N4 | Высокая (у пользователя) | Краш в FL Studio при движении фильтра / mix. Класс воспроизведён на Sand до T-024 (ASan: heap-buffer-overflow на первом 0-кадровом блоке в 160/160) — это F1; на ветке 0 из 320 рендеров (Release+ASan), VST3 ×20 и ASan ×3 чисто | Ждёт проверки автором в FL (чеклист в Log) |
| F2-эхо | Низкая | Откат значения в конце быстрого свипа CC (VST3-хост возвращает отчитанное значение) | Не воспроизводится в REAPER VST3 (35 эхо, 0 устаревших), в CLAP эха нет; mac-хосты — ручной чеклист |
| **T21-5** | Низкая (только отображение) | `hvoya/utils/host_info_model.hpp:187`: номер доли верен только при 120 BPM (PPQ умножался на `60/tempo`), в 3/4 и 6/8 перенос по `den` вместо `num`, дробь всегда 0. Затрагивает Sand, Gneiss, Scape, DMT, Melter, все форматы | ✅ hvoya `4fbfc579d` (merge `b36d0d545`, по решению автора 2026-10-18), hvoya-linux `03f13302a`; табличный тест: 168/216 неверных → 0 (Apple clang, GCC 13) |
| T26-a | Низкая | `host_time_info.hpp` не включает `<cstdint>`; `HostInfoModel` не обновляется в позиции 0 (`_lastSamplePos` стартует с 0) и логирует «trigger not set», когда он задан (только Debug) | Не исправлялось |
| T26-b | Низкая | iPlug core `IPlugProcessor::AttachBuffers` (`IPlugProcessor.cpp:485`, `CastCopy`): хост, приславший блок длиннее объявленного максимума при 32-bit I/O, переполняет scratch iPlug (ASan heap-buffer-overflow; Release: glibc «corrupted size»), до и после N1 одинаково. VST3/CLAP-хосты так делать не должны, AU отклоняет. Найдено ревьюером T-026 | Только отчёт |
| T26-c | Низкая–средняя | Sand: AU `DoReset` / `kAudioUnitProperty_BypassEffect` → `OnReset` → `updateOversampling` сбрасывает модуль и меняет его частоту (длины линий grind), пока другой поток может быть в `ProcessBlock` (давняя гонка этого пути; N1 её не трогает и свои буферы там больше не трогает) | Только отчёт |

## Из T-019 (Gneiss; не Linux-специфично)
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T19-1 | Низкая (UX) | Файл раскладки, брошенный на вкладку USER, доходит до `UserTabPanel::OnDrop` только в зазорах между её дочерними контролами: IGraphics отдаёт drop верхнему контролу под точкой, и ручка его игнорирует. Так же на mac/win (тот же `IGraphics::OnDrop`). Вариант: дочерним контролам USER-вкладки пробрасывать `OnDrop` в панель | Только отчёт |

## Из T-028 (Gneiss C3/C5; общий код iPlug, касается mac/win) — только отчёт, подробности и доказательства: `tasks/T-028-gneiss-c3-c5.md`, Log
| ID | Серьёзность | Где | Суть | Статус |
|---|---|---|---|---|
| **T28-a** | Низкая | `IPlug/IPlugParameter.cpp` `IParam::GetDisplay` (`if (!displayValue) displayValue = 0.0`) | Штатное отображение сворачивает в «0» только точный ноль: значение в (−0.5·10^−p, 0) показывается как «-0.000», а текст читается обратно как 0 → «0.000». Не проходит round trip (clap-validator `param-conversions` может попасть случайно). У Gneiss: `pt tilt`, `preFlt band mode`, 11 UB-параметров; у любого плагина с биполярным параметром без своей display-функции. Предложение: сворачивать значения, которые округляются до нуля при `mDisplayPrecision` | В Gneiss исправлено (`0f912c3`, по решению автора); в iPlug — только отчёт |
| **T28-b** | Низкая–средняя | `IParam::StringToValue` (`atof`) и hvoya `makeSegmentedParser` (`strtod`) | Текст «nan» (ввод в поле параметра хоста или в text entry плагина) даёт NaN, `Constrain`/`Clip` его пропускают → NaN в значении параметра и в DSP (на x64 — индексы таблиц вне границ, см. Gneiss ARCHITECTURE §4). У Gneiss после T-028 так ещё 41 параметр со штатным разбором; его собственные парсеры (`param_text.hpp`) читают «nan» как 0. Предложение: в `StringToValue` заменять NaN (например на 0 до `Constrain`) — одна точка для всех форматов | В Gneiss исправлено (`0f912c3`: «nan» → 0 у всех параметров); в iPlug — только отчёт |
| T28-c | — (харнесс) | `scripts/steps/gneiss.txt` шаг `g04b-context-menu` | В REAPER VST3 под Xvfb контекстное меню по правому клику на ручке не появляется (CLAP — появляется). Падает одинаково на базе и на ветке, и во всех merge-smoke с pre-T-007 | Давнее, не T-028 |

## Из T-010 (Gneiss; только Linux)
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T10-1 | Низкая (UX) | Alt‑drag / Alt‑клик по ручкам частоты (привязка к ноте) на Xfce, MATE, Cinnamon, KDE Plasma 5 перехватывает оконный менеджер: окно двигается вместо привязки. GNOME и Plasma 6 используют Super | Решение автора: пометка в MANUAL (Gneiss `linux-experiment`), без изменения кода |

## Из T-034 (общий код iPlug, NanoVG на GL: Linux, Windows GL2/GL3) — подробности: `tasks/T-034-nanovg-fbo-stencil.md`
| ID | Серьёзность | Где | Суть | Статус |
|---|---|---|---|---|
| T34-1 | Низкая–средняя | `IGraphics/Drawing/IGraphicsNanoVG.cpp` `DrawResize` | Stencil главного FBO (`mMainFrameBuffer`) после создания не очищался: содержимое нового renderbuffer не определено, `BeginFrame` чистит буферы окна, а не FBO. Вогнутые заливки (SVG-лого) на первом кадре рисовались ограничивающими прямоугольниками. На VM (virgl) в Ardour — 3 из 4 первых открытий; на Windows не наблюдалось (драйверы обычно отдают обнулённую память), Metal не затронут | Исправлено: hvoya-linux 29022e440, `hvoya` 07953bf26 (OK автора 2026-10-20) |

## Сообщено автором (mac)
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| S-1 | Средняя (UX), все плагины на маке | **Причина найдена (2026-10-19).** Нативное поле ввода на маке (`IGraphicsMac_view.mm` `createTextEntry` + `IGRAPHICS_FORMATTER isPartialStringValid`) проверяет фильтром символов (цифры `.+-`) **всю строку поля**, а не только введённый символ. Поле открывается с текстом дисплея («10.00 kHz»), поэтому любая правка, включая Backspace, отклоняется, пока весь текст не выделен и не заменён цифрой: «только цифры и не с первого раза», одинаково в CLAP и VST3 (автор). На Windows (`ParamEditProc`, `WM_CHAR`) и в `ITextEntryControl` (Linux) проверяется каждый введённый символ: правка работает, но единицы («kHz») тоже не набрать, хотя после T-024/T-028 плагины их разбирают | Предложение: (а) мак — проверять только вставленные символы, как Windows; (б) все платформы — не фильтровать символы, если у параметра есть `TextToValueFunc` (плагин сам разбирает текст). Ждёт решения автора |

## Из T-023 (CLAP, общий код `IPlug/CLAP/IPlugCLAP.*`; касается mac/win) — принято автором 2026-10-20
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T23-1 | Низкая–средняя | После загрузки FXP/FXB (iPlug зовёт `InformHostOfPresetChange`) CLAP-хост продолжал показывать и автоматизировать старые значения. Предложение `linux/T-023`: `InformHostOfPresetChange` всегда откладывает `params.rescan(VALUES)` до ближайшего `on_main_thread` хоста (через `request_callback`, без мьютекса и аллокаций), чтобы хост прочитал значения уже после того, как вызывающий код доприменит состояние. **Поведение на mac/win:** у Gneiss загрузка пользовательского пресета и шаги по полосе пресетов (`PresetManager` → `LoadStateFromFXP`) теперь приводят к повторному чтению значений хостом, уже после восстановления workspace; заводские пресеты (из чанка) — нет; Sand не затронут | Исправлено (OK автора 2026-10-20): hvoya-linux, `hvoya`. Открыто: звать ли то же из `RestorePreset` (общий для всех форматов) |
| T23-2 | Средняя (Release-краш) | `FlushParamsIfNeeded` звал `params.request_flush` без проверки `canUseParams()`: у хоста без `clap.params` Debug падает на ассерте clap-helpers, **Release падает** (проверено тестовым хостом ревьюера T-023). Предложение: проверять `canUseParams()` | Исправлено (OK автора 2026-10-20): hvoya-linux, `hvoya` |

## Из T-011 (`Examples/duplicate.py`, `Examples/gitignore_template`; касается mac/win) — OK автора на правку 2026-10-16, в `hvoya` не перенесено
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T11-1 | Средняя | Дубликат вне дерева iPlug2 (4-й аргумент) получал верный `IPLUG2_ROOT` только в `config/*.xcconfig`/props; `CMakeLists.txt` (`IPLUG2_DIR`), `CMakePresets.json`, `-linux.code-workspace`, `.vscode/c_cpp_properties.json` сохраняли `../..` — CMake-сборка такого дубликата ломалась на всех платформах. Теперь эти файлы переписываются (пути с `/` на всех ОС; цель и корень через `realpath`). Вывод Xcode/VS для обычных путей байт-в-байт тот же (20 примеров × 5 размещений) | Исправлено: hvoya-linux 839e1b51d. Перенос в `hvoya` — решение автора |
| T11-2 | Средняя | Дублирование IPlugControls падало на полпути (`UnicodeDecodeError` на `scripts/IBitmapSlider.knob`) на всех ОС: `.knob` теперь бинарный | Исправлено: hvoya-linux 839e1b51d |
| T11-3 | Низкая | `gitignore_template` не игнорировал `plug_build_info.hpp` (мак-сборка оставляла его неотслеживаемым в каждом дубликате) | Исправлено: hvoya-linux 839e1b51d |
| T11-4 | Низкая (Windows) | Существующая (апстрим, 2018) замена путей в `config/`/`projects/` пишет обратные слэши `relpath` во все файлы, включая mac xcconfig/pbxproj, web `.mk`, `makedist-*.sh`, `prepare_resources-*.py`; если iPlug2 не прямой потомок целевой папки, путь удваивается (`..\..\..\iPlug2\iPlug2`). Сборка на Windows не страдает, дубликат с Windows не собирается на mac/iOS/web. Исправление в одну строку: нормализовать `newroot` к `/` до `dirwalk()` | Не исправлено; тикет T-037 (автор 2026-10-22) |
| T11-5 | Низкая | Не исправлено, отчёт: путь цели с `[ ] * ?` делает старый `replacestrs` молчаливым no-op (glob); цель на другом диске Windows → `ValueError`; первая сборка Xcode меняет 6 отслеживаемых `resources/*-Info.plist`; `.RPP` дубликата ссылается на плагин примера | Отчёт |

## Из T-031 (REAPER-расширения на Linux; общий код `IPlug/ReaperExt/*`, `IPlugEditorDelegate.h`, `IPlugAPIBase.h`, `Scripts/cmake/{ReaperExt,Deploy}.cmake`)
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T31-1 | — | Все изменения в общих файлах под `OS_LINUX` / `UNIX AND NOT APPLE`; на mac/win поведение не меняется. Единственное общее изменение без гарда: `iplug_add_plugin_resources(target format)` в `Examples/IPlugReaperPlugin/CMakeLists.txt` получила аргумент формата (на mac тот же результат) | hvoya-linux cbb4d7105; в `hvoya` — вместе с Linux-портом |

## Из T-032 (общий код `IGraphics/Controls/IVPresetManagerControls.h`, `IGraphics.h`; касается mac/win) — по решению автора 2026-10-22 («асинхронно, если нет весомых причин»)
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| T32-0 | — | **Изменение поведения:** кнопка Load у `IVDiskPresetManagerControl` вызывает `PromptForFile` с completion handler. На маке панель — sheet на окне плагина (была app-modal); Windows без изменений (handler вызывается до возврата); iOS больше не падает на ассерте (не собиралось). Доки: `linux-development.md` §9, комментарии к диалогам в `IGraphics.h` | hvoya-linux; в `hvoya` не перенесено |
| XP-1 | Низкая | Мак: `PromptForColor` без handler бросает `bad_function_call`, когда пользователь выбирает цвет (`IGraphicsMac_view.mm:1300`) | Отчёт (`tasks/T-032-sync-dialogs.md`, Log) |
| XP-2 | Средняя | `IVDiskPresetManagerControl` с `showFileExtensions=false` передаёт в `OnLoadFunc` путь без расширения — плагин просят загрузить несуществующий файл | Отчёт |
| XP-3 | Тривиальная | В двух тестовых контролах `SetDirty(false)` вне `if` (нет фигурных скобок) | Отчёт |
| XP-4 | Низкая (web) | `ShowMessageBox`: ветка Yes-No/OK-Cancel проваливается, возвращает `kNoResult` и не зовёт handler | Отчёт |
