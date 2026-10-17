# Найденные НЕ-Linux баги (касаются mac/win) — решения за автором

Источник: задача T-003 (сборка Gneiss под Linux), три независимых ревью в ветке `linux/T-003`,
`linux-research/evidence/t003/reviews/2-findings-review.md`. В рамках порта они **не исправлялись**:
порт не меняет бизнес-логику и поведение на других платформах (AGENTS §2).

| ID | Серьёзность | Где | Суть | Затрагивает mac/win? |
|---|---|---|---|---|
| **C1** ✅ исправлено (hvoya 3474f3d55, hvoya-linux e7c882995) | **Высокая** | `hvoya/utils/audio_buffer.cpp:210-214` (`AudioBuffer::reserve`), вызывающий — `Gneiss.cpp:189→1767` (`_osBuffer`) | `reserve` перевыделяет память, но не перепривязывает указатели каналов → **порча кучи**. Срабатывает, если первый оверсемплированный блок стерео и `блок × OS-фактор = 512` (256@2x, 128@4x, 64@8x) — например, переключение на 2x при буфере 256 или загрузка проекта с такой настройкой. Подтверждено тестом на маке (clang) и ASan на Linux | **Да** |
| **X1** | Средняя (узкое окно, но краш) | `IPlugProcessor::SetLatency` → `NChanDelayLine::SetDelayTime` (`NChanDelay.h:26-31`) | Длина задержки меняется и буфер переаллоцируется без синхронизации, а аудиопоток читает его в `PassThroughBuffers`, когда плагин в собственном bypass (VST3/AU/AAX). Смена OS-фактора (→ латентность) во время собственного bypass может упасть | **Да** (upstream iPlug) |
| **C3** | Низкая–средняя | `Gneiss.cpp:~870` (downsample в `input`, mix/gain, `input += _dryBuf`) | Gneiss пишет во **входной** буфер хоста. В 64-bit (double) режиме CLAP/VST3 iPlug отдаёт указатели хоста напрямую → у хостов с out-of-place обработкой портится их вход. При любом OS-факторе | **Да** (64-bit хосты) |
| **C2** | Средняя (UX) | `morph_controller.hpp:~1980` (`applyMorphModeParams`), дедуп `_prevParamVals` (`Gneiss.cpp:1940`) | После загрузки пресета кнопка MORPH требует лишних кликов: прямой `IParam::Set(0)` не обновляет кэш дедупликации; фабричные пресеты несут `morph_active=1`. Плюс на мгновение `_active=true` во время восстановления | **Да** (все форматы) |
| **C4** | Средняя | `IPlugVST3.h:62` (`InformHostOfPresetChange` пустой), preset strip `>` | После загрузки пресета через полосу пресетов VST3-хост видит старые значения параметров (кэш контроллера не обновляется); CLAP-хосты с кэшем тоже. Фикс: `DirtyParametersFromUI()` после загрузки в PresetManager | **Да** |
| **L1** | Неизвестна на маке | `IGEditorDelegate` размер редактора; `IPlugCLAP.cpp:972`, `IPlugVST3_View.h:76` | Gneiss создаёт редактор сразу нужной высоты (morph on / scale≠1), а сохранённый размер делегата не обновляется → CLAP/VST3 сообщают хосту устаревший размер. На Linux обойдено (Linux-only), на маке зависит от хоста (Cubase/Live берут размер из getSize) | Возможно |
| **C5** | Низкая | `IPlugParameter.cpp:397` (`StringToValue` = `atof`), `hzDisplay` | Ввод «1.06 kHz» → 1.06 Hz; «50%» → клип до 1. clap-validator `param-conversions` падает. Также Gneiss не объявляет tail → `process-sleep-process-status` | Да (ввод текста в хосте) |
| **X3** | Низкая | `IPlugAPP.cpp` `#import <Cocoa/Cocoa.h>` (hvoya cf2b2a1f4) | Пример IPlugEffect APP на маке не собирается (компилируется как C++). Gneiss не задет | Только пример |

Также (Linux-only, к сведению): GLIBC floor у Gneiss 2.35 из-за `hypot/hypotf` (можно прибить к 2.34 через `.symver`).

## Что предлагаю
- **C1 — исправить сразу** в `hvoya` (однострочный-двухстрочный фикс: перепривязать каналы в `reserve`), с юнит-тестом.
- **X1** — фикс в iPlug (менять задержку только из аудиопотока / двойная буферизация) или в Gneiss менять латентность только когда не идёт процесс.
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
Рядом, не исправлено: CLAP не переопределяет `InformHostOfPresetChange`, поэтому пресеты iPlug (`RestorePreset`, FXP/FXB) тоже оставляют CLAP-хосту старые значения. Gneiss это не покрыло бы: его PresetManager не зовёт `InformHostOfPresetChange`, а morph undo вызывает `UnserializeState` напрямую. Для Gneiss остаётся фикс C4.

## Из T-004 (standalone APP, общий код `IPlug/APP/IPlugAPP_host.cpp`) — исправлено с согласия автора
Меняют поведение mac/win только в сбойных случаях, нормальная работа не меняется. В `hvoya-linux`; в `hvoya` попадут при будущем слиянии.
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
| **F2** ✅ исправлено (T-024: iPlug2 `fix/midi-cc-host-notify` 17765295e + 8f40400b4, Sand `cdad970`; Gneiss/DMT/Melter/Crust_new — строка в OnIdle (ветки `fix/midi-cc-host-notify`, не закоммичено), Scape — вручную) | Средняя | MIDI CC → значение параметра не доходит до VST3-хоста: `hvoya/utils/midi_cc/mediator.hpp` из аудиопотока зовёт `BeginInformHostOfParamChange`/`SendParameterValueFromDelegate` — в VST3 это вызовы `IComponentHandler` вне UI-потока (нарушение правил VST3), REAPER видит старое значение. Общий код hvoya: Gneiss/Scape/DMT тоже. Родственно C4 |
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
  - На маке `param-conversions` ещё падает на enum-параметрах ('0.0202' → '0' → 'x1'). Исправлено в hvoya-linux `ef002e167`, в hvoya ещё нет.

## Из T-026 (Sand и общий код hvoya; касается mac/win) — подробности и доказательства: `tasks/T-026-sand-followups.md`, Log
| ID | Серьёзность | Суть | Статус |
|---|---|---|---|
| **N1** | Средняя | `SandModule::process` выделял память в аудиопотоке: копия `dryCopy` на каждом блоке, плюс рост `_osBuffer`, оверсэмплера и скретч-буферов на первом блоке и при каждой смене OS. Счётчик malloc внутри `process()` (Linux CLAP, 47 конфигураций): до — 1 на блок + 12–24 на первом, после — 0 везде | ✅ Sand `9c2dad2` (`fix/t026-followups`); звук побитово тот же (Linux 47, mac 46 рендеров), CPU без изменений |
| N2 | — | Свежий клон Sand на маке | ✅ закрыто коммитом автора 961be6c: CLAP/VST3/AUv2/APP собираются из клона |
| N3 | Средняя (только AAX/win) | `Sand-aax.vcxproj` был шаблоном IPlugEffect: нет `SandUI.cpp`, hvoya, HIIR | ✅ список файлов `cc81d9d` (на Windows не собрано). Ещё не хватает `stdcpp23` и prebuild `plug_build_info` — патч-предложение `evidence/t026/proposals/n3-aax-settings.diff` |
| N4 | Высокая (у пользователя) | Краш в FL Studio при движении фильтра / mix. Класс воспроизведён на Sand до T-024 (ASan: heap-buffer-overflow на первом 0-кадровом блоке в 160/160) — это F1; на ветке 0 из 320 рендеров (Release+ASan), VST3 ×20 и ASan ×3 чисто | Ждёт проверки автором в FL (чеклист в Log) |
| F2-эхо | Низкая | Откат значения в конце быстрого свипа CC (VST3-хост возвращает отчитанное значение) | Не воспроизводится в REAPER VST3 (35 эхо, 0 устаревших), в CLAP эха нет; mac-хосты — ручной чеклист |
| **T21-5** | Низкая (только отображение) | `hvoya/utils/host_info_model.hpp:187`: номер доли верен только при 120 BPM (PPQ умножался на `60/tempo`), в 3/4 и 6/8 перенос по `den` вместо `num`, дробь всегда 0. Затрагивает Sand, Gneiss, Scape, DMT, Melter, все форматы | Предложение: iPlug2 `fix/host-info-beat` `b36d0d545`; табличный тест рукописная таблица: 168/216 неверных → 0 (Apple clang, GCC 13). Ждёт решения автора |
| T26-a | Низкая | `host_time_info.hpp` не включает `<cstdint>`; `HostInfoModel` не обновляется в позиции 0 (`_lastSamplePos` стартует с 0) и логирует «trigger not set», когда он задан (только Debug) | Не исправлялось |
| T26-b | Низкая | iPlug core `IPlugProcessor::AttachBuffers` (`IPlugProcessor.cpp:485`, `CastCopy`): хост, приславший блок длиннее объявленного максимума при 32-bit I/O, переполняет scratch iPlug (ASan heap-buffer-overflow; Release: glibc «corrupted size»), до и после N1 одинаково. VST3/CLAP-хосты так делать не должны, AU отклоняет. Найдено ревьюером T-026 | Только отчёт |
| T26-c | Низкая–средняя | Sand: AU `DoReset` / `kAudioUnitProperty_BypassEffect` → `OnReset` → `updateOversampling` сбрасывает модуль и меняет его частоту (длины линий grind), пока другой поток может быть в `ProcessBlock` (давняя гонка этого пути; N1 её не трогает и свои буферы там больше не трогает) | Только отчёт |
