# iPlug2 на Linux: исследование (2026-10-10/11)

Цель: собрать Gneiss (и другие hvoya-плагины) под Linux как CLAP + VST3 (+ standalone) с полноценным
IGraphics-UI, работающим в реальных DAW. Ветка порта — **от `hvoya`**.

Рабочие материалы: [`linux-research/`](linux-research/)
- `notes/01-existing-efforts.md` — кто и что уже делал (форки, PR, ветки)
- `notes/02-how-others-do-it.md` — как Linux-UI решают JUCE / DPF / VSTGUI / pugl / Visage / baseview
- `notes/03-build-test-results.md` — реальная сборка и прогон кандидатов в Docker
- `notes/04-architecture-review.md` — ревью кода кандидатов + план поверх `hvoya` с оценками
- `notes/05-review-of-build-results.md` — перепроверка 03 (часть выводов 03 опровергнута; где они
  расходятся, верен 05)
- `notes/06-review-of-final-doc.md` — фактчек этого документа (замечания внесены)
- `docker/`, `scripts/` — переиспользуемый харнесс (сборка + REAPER под Xvfb + скриншоты + xdotool)
- `evidence/` — логи, скриншоты, бэктрейсы, патчи

---

## TL;DR

1. **Готового рабочего решения нет.** В upstream master Linux отсутствует (`IGraphicsLinux.h` — заглушка,
   которая даже не компилируется; в CMake — «to be added later»).
2. **Полноценные Linux-билды Gneiss реальны.** Прецеденты на iPlug2 есть: PR #1336 — VST3+CLAP в
   REAPER/Renoise (мы подтвердили сборку и отрисовку); Stillwell — Skia/VST3 proof-of-concept в
   коммерческом форке; Deimos (TomTrnk) — заявлен Linux CLAP (beta). Сам Gneiss почти платформонезависим.
3. **Ни один кандидат нельзя взять целиком.** Решающая причина — модель потоков: у Gneiss `OnIdle()`
   трогает UI и `SetLatency()` («message thread only»), а у всех кандидатов `IPlugTimer`/`OnIdle` хотя бы
   в части форматов крутится на собственном потоке плагина. Плюс найденные баги (гонка старта потока,
   краш в OS text entry, GTK внутри процесса).
4. **Рекомендация:** своя Linux-прослойка на ветке от `hvoya`, где UI и таймеры гонит **хост** (CLAP
   `timer-support`/`posix-fd`, VST3 `IRunLoop`, свой цикл в standalone). Сборку/CMake и X11-утилиты
   (clipboard, курсоры, resize) брать из PR #1336, VST3 run-loop и жизненный цикл редактора — из форка
   Stillwell (его окно Skia-only — придётся перевести на NanoVG/GL).
   **Оценка: ≈ 31–49 человеко-дней** (≈ 7–10 недель одного разработчика), из них 5–8 — на прогон по DAW.
   Это оценка по чтению кода, без прототипа.
5. Брать куски из PR #1336 на `hvoya` технически дёшево: его коммиты черри-пикаются на `hvoya` с
   механическими конфликтами (11 файлов / 15 ханков, проверено — ветка `linux-trial/rangerrick`).

---

## 1. Что существует

| Кандидат | Активность | Что есть | Главные проблемы |
|---|---|---|---|
| **PR #1336** RangerRick ([PR](https://github.com/iPlug2/iPlug2/pull/1336), [ветка](https://github.com/RangerRick/iPlug2/tree/feature/linux-support)) | открыт с 2026-03, посл. коммит 2026-06, ~57 позади master, конфликты | X11 + GLX/GL3 + NanoVG; APP/VST3/CLAP; upstream-CMake; clipboard, курсоры, resize; Оли хвалил на Wayland-машине | рисование VST3/APP и **все** `IPlugTimer` на своих pthread'ах; гонка старта потока; GTK-диалоги внутри процесса (зависания, краш); `nullptr` в OS text entry; символы не скрыты; общий `resources/` рядом с `.clap` |
| **stillwellaudio/iPlug2** ([repo](https://github.com/stillwellaudio/iPlug2)) | очень активен (2026-10-09), коммерческий | Skia-only; VST3 гонится **от `IRunLoop` хоста** (их PR #1); хороший жизненный цикл редактора | CLAP/APP/закрытый VST3 — на worker-потоке; диалогов нет (заглушки); своей CMake нет; 503 позади, смешан с продуктовым кодом |
| upstream `linux-bindernews` (2025-12) | — | PlatformX11, glad_GLX, fontconfig, zenity | UI на приватном потоке; CLAP `#error Not Implemented`, VST3 не грузится; только APP |
| upstream `linux` (2019–22, xcbt) | мёртв | правильная идея: X-события через цикл хоста, `IPlugVST3_RunLoop`, LV2 | 1731 позади master |
| TomTrnk `linux-tomtrnk` | спит с 2026-01 | Linux CLAP Deimos (по словам автора) | VST3 хосты не видят; 513 позади |
| iPlug3 (Оли) | закрытый код | Linux в роадмапе | несовместим с iPlug2 |

## 2. Что проверено руками

Условия: Docker, linux/arm64, **только REAPER 7.82** под Xvfb + Mesa llvmpipe; валидаторы headless.
Bitwig/Carla/Ardour, x86_64, реальные GPU и Wayland не проверялись.

| | PR #1336 | Stillwell | PR #1336 поверх `hvoya` |
|---|---|---|---|
| Сборка APP/VST3/CLAP | ✅ (2 мелких патча) | ✅ VST3/CLAP (пришлось собрать Skia и написать CMake) | ✅ (+ патч CMake примера на чистом чекауте) |
| VST3 validator | 537/537 | 537/537 | 537/537 |
| clap-validator (IPlugControls) | 4–5 падений из 44 | 0–1 | 3 (исчез `layout-audio-ports-config`; 3× `state-reproducibility` остались; IPlugInstrument — 4) |
| UI в REAPER, CLAP | ✅ рисуется; ❌ краш на OS text entry | ✅ | ✅ рисуется; ❌ краш на OS text entry |
| UI в REAPER, VST3 | ✅ IPlugEffect; IPlugControls — ❌ краш на OS text entry; ⚠️ та же гонка старта (код идентичен) | ✅ | ⚠️ первое открытие пустое под gdb (гонка); после фикса гонки — ❌ тот же краш OS text entry |
| Поток рисования | CLAP — главный поток хоста; VST3/APP — свой pthread | VST3 — хост; CLAP — свой поток | как у PR |
| Поток `OnIdle` | **всегда свой pthread** | worker при CLAP/APP/закрытом VST3 | **всегда свой pthread** |

### Найденные баги (важно для будущего порта)

1. **Гонка старта потока в PR #1336** (`IGraphicsLinux::StartTimer`, `IPlugTimer.cpp` `Timer_impl`):
   `mRunning = true` выставляется *после* `pthread_create`, а поток проверяет `while (mRunning)` →
   может сразу выйти; тогда редактор не рисуется и не реагирует, `OnIdle`/`ISender` молчат.
   Замеры: 1/400 в изолированном тесте; почти всегда под gdb; без отладчика редко (64/64 таймеров
   живы, 8/8 первых открытий отрисованы), но в двух наших прогонах без gdb у одного из двух экземпляров
   так и не поднялся поток `OnIdle`-таймера. В поле — редкий, но реальный баг.
   Затронуты все форматы (CLAP-рисование нет, но `IPlugTimer` — да). Фикс — флаг до `pthread_create`.
   *Первоначальный отчёт сборщика это пропустил («VST3 passes everything») — поймано по скриншотам.*
2. **Краш OS text entry** (VST3 и CLAP): синхронный `gtk_dialog_run` → коммит обнуляет
   `mInTextEntry` → `IGraphics.cpp:1987` разыменовывает `nullptr`. Корень — вложенный GTK-цикл в
   плагине (для CLAP — на главном потоке хоста). IGraphics-текстовый ввод работает.
   Для Gneiss, вероятно, не критично (его ручки не редактируются текстом), но GTK-путь всё равно убирать.
3. **Потенциальный дедлок при закрытии редактора** в PR #1336 (lock → join потока, который ждёт тот
   же lock). Подтверждён по коду, в 1000 циклах open/close не воспроизвёлся — дефект дизайна.
4. **Баг в `hvoya` уже сейчас:** `IPlug/IPlugLogger.h` `CurrentTime()` — `snprintf(&cStr[i], 32, …)`
   при `i≈15` завышает размер буфера. На Linux/glibc с `_FORTIFY_SOURCE=3` (по умолчанию в Ubuntu
   24.04/gcc 13) **плагин падает при конструировании**. На macOS (Apple clang, fortify 2 и 3) не падает,
   у MSVC fortify нет — т.е. проблема только Linux, но это реальный выход за буфер. Однострочный фикс:
   `snprintf(&cStr[i], sizeof(cStr) - i, …)` (в PR #1336 он уже есть).
5. Перерисовка на каждом тике таймера даже без изменений (у PR #1336).
6. **`state-reproducibility` в clap-validator** падает и на `hvoya`-ветке. Возможно, причина в общем
   коде CLAP-обёртки, и тогда это касается и mac/win-сборок Gneiss. Не проверено — стоит прогнать
   clap-validator на маковском CLAP Gneiss.

## 3. Как правильно (сводка лучших практик, notes/02)

- **Только X11** (через XWayland работает везде; Wayland-встраивание есть лишь в VST3 SDK 3.8 /
  Studio One). Окно-слой держать абстрактным на будущее.
- **Никакого собственного GUI-потока.** X-события, рисование, `OnIdle` — в колбэках хоста на его
  главном потоке: таймер ~16 мс (CLAP `timer-support`, VST3 `IRunLoop::registerTimer`) +
  опционально fd X-соединения (`posix-fd` / `registerEventHandler`); на каждом тике выбирать
  `XPending` досуха. Так делают DPF и VSTGUI; JUCE переключается на поток хоста при первой возможности.
- **GL:** GLX (EGL на будущее), контекст на редактор, **swap interval 0** (vsync блокирует главный
  поток хоста × число открытых редакторов), темп задаёт таймер.
- **Диалоги без GTK в процессе** (ABI-конфликт с хостом, Ardour падает): xdg-desktop-portal через
  D-Bus или zenity/kdialog подпроцессом с fd в цикле хоста; меню и ввод текста — IGraphics.
- **Сборка:** `-fvisibility=hidden` + version script (только entry points) + `--exclude-libs,ALL`,
  `-static-libstdc++`, сборка на старом glibc (Ubuntu 22.04 / manylinux_2_28). VST3 — бандл
  `Contents/x86_64-linux/*.so`, CLAP — один `.so`.
- **LV2 не нужен** в v1: Ardour грузит VST3 (CLAP в Ardour нет).

## 4. План поверх `hvoya` (из notes/04 + фактчек; человеко-дни)

| # | Работа | Откуда | Дни |
|---|---|---|---|
| 0 | Ветка `linux` от `hvoya`; влить upstream master (1 конфликт) **или** перенести только CMake. Вливание тянет 242 коммита → нужна перепроверка Gneiss на mac/win (в оценку не входит) | — | 0.5–2 |
| 1 | Ядро IPlug под Linux: платформа, пути, VST3 `linuxmain` + X11 platform type, CLAP X11 API | PR #1336, Stillwell | 1.5–2 |
| 2 | **Таймеры от хоста**: Linux `Timer_impl` на CLAP `timer-support` / VST3 `IRunLoop` (из host context, как JUCE) / GLib в APP; worker-поток — только как fallback с предупреждением | новое + Stillwell | 4–6 |
| 3 | **IGraphicsLinux, только host-driven**: база Stillwell (перевести со Skia на NanoVG/GL) + resize/clipboard/курсоры из PR #1336; X fd, swap 0, `DrawNow`, `mInOpenWindow`, XEmbed-фокус | Stillwell + PR #1336 | 6–9 |
| 4 | Асинхронные диалоги: portal → zenity/kdialog; без GTK | новое | 4–6 |
| 5 | HiDPI: CLAP `set_scale`, VST3 content scale, Xft.dpi | PR #1336 + новое | 2–3 |
| 6 | NanoVG GL3 на Linux + фиксы FBO для llvmpipe; визуальная сверка Gneiss с Metal/GL2 | PR #1336 | 1–2 |
| 7 | CMake-проект Gneiss под Linux (+ перенос Xcode/py prebuild-скриптов), гигиена символов, контейнер со старым glibc; **ресурсы внутри бандла / встроенные** (не общий `resources/` — у RR его отсутствие роняет REAPER, а у нескольких hvoya-продуктов они бы конфликтовали) | PR #1336 + новое | 3–4 |
| 8 | Standalone: SWELL/GDK + RtAudio (ALSA/JACK/Pulse) на цикле GLib | PR #1336 | 2–3 |
| 9 | Валидация и DAW: validators под Xvfb; REAPER, Bitwig, Carla, Renoise, Ardour (VST3); мультиинстансы, 100× open/close, state при открытом/закрытом редакторе, **смена латентности при закрытом редакторе** (ключевое требование Gneiss) | — | 5–8 |
| 10 | Сторона Gneiss: fallback для `SetLatency` вне главного потока; `preset_manager` `openFolder()` (`xdg-open`); popup-меню Gneiss идут через платформенный путь → на Linux перевести на IGraphics-меню (`AttachPopupMenuControl`) и проверить обрезку краем окна | новое | 2–4 (моя оценка) |
| | **Итого** | | **≈ 31–49** |

Принцип для мерджей с upstream в будущем: Linux-код в **новых файлах** с тонкими `#ifdef OS_LINUX`-хуками,
не трогать общий resize-код для всех платформ (как делает PR #1336) — тогда возможный мердж #1336 в
upstream не превратится в ад.

### Риски
1. Хосты, которые не дают `IRunLoop`, пока редактор закрыт → `OnIdle` придётся гонять с worker-потока;
   Gneiss тогда должен уметь отложить `SetLatency`. **Проверить первым делом** по каждому хосту.
2. XEmbed-фокус клавиатуры по-разному работает в разных хостах (Bitwig, Carla).
3. Драйверы (NVIDIA vs Mesa), vsync, масштабирование в XWayland — Docker этого не покажет.
4. Upstream может смерджить #1336 позже — расхождение с нашим вариантом.
5. Оценка сделана по чтению кода без прототипа — первые 1–2 недели (п. 0–2 + минимальное окно)
   стоит считать проверкой оценки.

## 5. Решения (статус на 2026-10-11)

Принято:
- Upstream master влит в `hvoya` (merge `5d854712d`; разбор — `linux-research/notes/07-upstream-merge-analysis.md`).
- Фикс `IPlugLogger.h` (баг №4) внесён в `hvoya` (`46f834a92`); заодно `IText::SetFont`/`WithFont` (`3a05dd2f4`).
- Порт ведётся в ветке **`hvoya-linux`** (от `hvoya` после мерджа), рабочий план — `linux-research/PLAN.md`.
- Объём v1: CLAP + VST3 + standalone, NanoVG/GL, X11. Без Skia, Wayland-native, LV2.

Открыто:
1. Проверить `state-reproducibility` clap-validator на маковском CLAP Gneiss (баг №6).
2. Для финальной проверки нужна x86_64-машина с Linux-десктопом (Docker/VM на маке — arm64, без
   реального GPU). ARM-VM в UTM с REAPER — для визуальных тестов во время разработки
   (см. `LINUX_VM_SETUP.md`).

## 6. Харнесс

```bash
colima start
```

Дальше — `linux-research/docker/README.md`: сборка образа, `scripts/build-cmake.sh <worktree> <builddir> <targets>`,
`scripts/run-gui-test.sh <plugin> <outdir>` (REAPER под Xvfb, скриншоты, xdotool-шаги, gdb на краш,
подсчёт потоков). Работает для любого worktree, включая ветки от `hvoya` (для чистого чекаута — патч
CMake примера, см. README). Ветка `linux-trial/rangerrick` (PR #1336 на `hvoya`) лежит в
`linux-research/wt/hvoya-trial-rangerrick` (только локально; разрешения конфликтов — в
`linux-research/evidence/hvoya-trial/cherry-pick.log`).
