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
| **F1** | **Высокая** | Sand обрабатывает блоки из 0 сэмплов: VST3-хосты зовут `process` с `numSamples = 0` (сброс параметров), `SandModule::process` читает `chan.back()/front()`, а `hvoya::utils::derivativeInplace` пишет в `data.front()` пустого span — выход за границы буферов хоста (ASan: heap-buffer-overflow / use-after-free; VST3-валидатор падает в 1–4 из 10 прогонов). Фикс: ранний выход из `Sand::ProcessBlock` при `nFrames == 0` и/или `derivativeInplace` терпит пустой span |
| **F2** | Средняя | MIDI CC → значение параметра не доходит до VST3-хоста: `hvoya/utils/midi_cc/mediator.hpp` из аудиопотока зовёт `BeginInformHostOfParamChange`/`SendParameterValueFromDelegate` — в VST3 это вызовы `IComponentHandler` вне UI-потока (нарушение правил VST3), REAPER видит старое значение. Общий код hvoya: Gneiss/Scape/DMT тоже. Родственно C4 |
| F3 | Низкая | NaN на выходе при 768 кГц с параметрами по умолчанию (clap-validator `process-varying-sample-rates`) |
| F4 | Низкая | Текст `flt hz` не парсится обратно ('1.01 kHz' → 1.0 Hz), как C5 у Gneiss |
| F5 | Низкая–средняя | Sand пишет во входной буфер хоста (как C3 у Gneiss): портит вход у out-of-place хостов в 64-bit |
| **F7** | Средняя | **Windows-проекты Sand, вероятно, не линкуются:** с 6036dec (2026-05-13) детекторы огибающей hvoya зовут `hiir::PolyphaseIIR2Designer::compute_coefs`, Xcode-проект компилирует `PolyphaseIIR2Designer.cpp`, ни один `projects/*.vcxproj` — нет (на Windows не проверено) |
| F8 | Низкая | `xcodebuild` Sand из командной строки (и на master) падает: `plog/Log.h` не найден, `$(HVOYA_PATH)/**` доходит до clang нераскрытым. Важно для CI/скриптов |
